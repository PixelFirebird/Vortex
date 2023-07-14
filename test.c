#include <dirent.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include "uthash.h"
#include <magic.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h> // Add this header for error handling
#include <sys/stat.h> // Add this header for mkdir function
#include <sys/types.h> // Add this header for mkdir function

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#define MAX_BUF_SIZE 1024

void process_files_recursive(const char *directory, const char *sorted_root_directory);
void process_file(const char *filename, const char *sorted_root_directory, const char *mime_type);
const char *get_mime_type(const char *filename);
int is_directory_empty(const char *directory);
int remove_directory(const char *path);

// Hashing function
char *sha256_hash_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return NULL;

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();

    EVP_DigestInit(mdctx, EVP_sha256());

    char buffer[MAX_BUF_SIZE];
    int bytesRead = 0;
    while ((bytesRead = fread(buffer, 1, MAX_BUF_SIZE, file)))
    {
        EVP_DigestUpdate(mdctx, buffer, bytesRead);
    }

    EVP_DigestFinal(mdctx, hash, &hash_len);
    EVP_MD_CTX_free(mdctx);
    fclose(file);

    char *outputBuffer = malloc(sizeof(char) * ((hash_len * 2) + 1));
    if (!outputBuffer)
        return NULL;

    for (int i = 0; i < hash_len; i++)
    {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }

    return outputBuffer;
}

// Hash table entry structure
struct file_hash
{
    char hash[2 * SHA256_DIGEST_LENGTH + 1]; // key
    UT_hash_handle hh;                        // makes this structure hashable
};

struct file_hash *file_hashes = NULL; // hash table

// Function to add a hash to the hash table
void add_hash(char *file_hash)
{
    struct file_hash *s = malloc(sizeof(struct file_hash));
    strcpy(s->hash, file_hash);
    HASH_ADD_STR(file_hashes, hash, s);
}

// Function to find a hash in the hash table
struct file_hash *find_hash(char *file_hash)
{
    struct file_hash *s;
    HASH_FIND_STR(file_hashes, file_hash, s);
    return s;
}

void build_hash_table(const char *target_directory) {
    DIR *dir;
    struct dirent *entry;
    char path[PATH_MAX];

    dir = opendir(target_directory);
    if (dir == NULL) {
        perror("Error opening directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue; // skip "." and ".."

        snprintf(path, sizeof(path), "%s/%s", target_directory, entry->d_name);
        
        struct stat path_stat;
        stat(path, &path_stat);
        
        if (S_ISREG(path_stat.st_mode)) { // Check if we're dealing with regular files
            char *hash = sha256_hash_file(path);
            if (hash != NULL) {
                // Add each hash to the file_hashes
                add_hash(hash);
                free(hash); // Remember to free the hash if it's dynamically allocated in sha256_hash_file
            }
        } else if (S_ISDIR(path_stat.st_mode)) {
            // Recursively call this function if we're dealing with directories
            build_hash_table(path);
        }
    }

    closedir(dir);
}

void replace_forward_slashes(char *str) {
    if (str == NULL) {
        return;
    }
  
    for (size_t i = 0; i < strlen(str); i++) {
        if (str[i] == '/') {
            str[i] = '\\';
        }
    }
}

// Function to create directories
int create_directory(const char *dir)
{
    struct stat st = {0};

    // Use forward slash as the directory separator for Windows paths
    char converted_dir[256];
    for (int i = 0; i < strlen(dir); i++)
    {
        if (dir[i] == '\\')
            converted_dir[i] = '/';
        else
            converted_dir[i] = dir[i];
    }
    converted_dir[strlen(dir)] = '\0';

    if (stat(converted_dir, &st) == -1)
    {
#ifdef _WIN32
        int result = mkdir(converted_dir);
#else
        int result = mkdir(converted_dir, 0777);
#endif

        if (result == -1)
        {
            char parent_dir[256];
            strncpy(parent_dir, converted_dir, sizeof(parent_dir));
            char *last_slash = strrchr(parent_dir, '/');
            if (last_slash != NULL)
            {
                *last_slash = '\0';
                if (!create_directory(parent_dir))
                {
                    return 0; // Failed to create parent directory
                }
#ifdef _WIN32
                result = mkdir(converted_dir);
#else
                result = mkdir(converted_dir, 0777);
#endif
                if (result == -1)
                {
                    fprintf(stderr, "Error creating directory: %s (%s)\n", converted_dir, strerror(errno));
                    return 0; // Directory creation failed
                }
            }
            else
            {
                fprintf(stderr, "Error creating directory: %s (%s)\n", converted_dir, strerror(errno));
                return 0; // Directory creation failed
            }
        }
    }

    return 1; // Directory created successfully or already exists
}





void escape_file_path(char *path)
{
    char escape_chars[] = "\\\'\"$`!*?{}[]()<>#%&|;:= ";
    char *p;
    size_t len;

    while ((p = strpbrk(path, escape_chars)) != NULL)
    {
        len = strlen(p);
        memmove(p + 1, p, len + 1);
        *p = '\\';

        // Move to the next character
        path = p + 2;
    }
}


// Function to copy a file
int copy_file(const char *src, const char *dest)
{
    printf("Copying file: %s\n", src);
    printf("Destination: %s\n", dest);

    FILE *src_file = fopen(src, "rb");
    if (!src_file)
    {
        printf("Error opening source file: %s (%s)\n", src, strerror(errno));
        return -1;
    }

    FILE *dest_file = fopen(dest, "wb");
    if (!dest_file)
    {
        printf("Error creating destination file: %s (%s)\n", dest, strerror(errno));
        fclose(src_file);
        return -1;
    }

    char buffer[MAX_BUF_SIZE];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), src_file)) > 0)
    {
        if (fwrite(buffer, 1, bytesRead, dest_file) != bytesRead)
        {
            printf("Error writing to destination file: %s\n", dest);
            fclose(src_file);
            fclose(dest_file);
            return -1;
        }
    }

    fclose(src_file);
    fclose(dest_file);

    return 0;
}

void process_files_recursive(const char *directory, const char *sorted_root_directory) {
    DIR *dir = opendir(directory);
    if (!dir) {
        printf("Error opening directory: %s (%s)\n", directory, strerror(errno));
        return;
    }

    struct dirent *ent;
    char path[256];

    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        snprintf(path, sizeof(path), "%s/%s", directory, ent->d_name);

        struct stat st;
        if (stat(path, &st) == -1) {
            printf("Error getting file/directory information: %s (%s)\n", path, strerror(errno));
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            process_files_recursive(path, sorted_root_directory);
        } else if (S_ISREG(st.st_mode)) {
            // Replace forward slashes with backslashes in file paths
            for (int i = 0; i < strlen(path); i++) {
                if (path[i] == '/') {
                    path[i] = '\\';
                }
            }

            // Determine the MIME type based on file extension
            const char *mime_type = get_mime_type(path);
            if (mime_type == NULL)
            {
                if (strcmp(path, "desktop.ini") == 0)
                {
                    printf("Deleting file: %s\n", path);
                    remove(path);
                    return;
                }
                printf("Unknown MIME type for file: %s\n", path);
                continue;
            }

            process_file(path, sorted_root_directory, mime_type);
        }
    }

    closedir(dir);
    // Check if the directory is empty
    if (is_directory_empty(directory) && strcmp(directory, sorted_root_directory) != 0)
    {
        // Delete the directory if it's empty
        int result = remove_directory(directory);
        if (result == 0)
        {
            printf("Deleted empty directory: %s\n", directory);
        }
        else
        {
            printf("Error deleting directory: %s\n", directory);
        }
    }
}


void process_file(const char *filename, const char *sorted_root_directory, const char *mime_type)
{
    // Skip "desktop.ini" files
    if (strcmp(filename, "desktop.ini") == 0)
    {
        printf("Deleting file: %s\n", filename);
        remove(filename);
        return;
    }

    // Hash the file
    char *hash = sha256_hash_file(filename);
    if (hash == NULL)
    {
        printf("Error hashing file: %s\n", filename);
        return;
    }

    // Check if the file's hash is already in the hash table
    struct file_hash *s = find_hash(hash);
    if (s != NULL)
    {
        printf("Duplicate file found: %s\n", filename);
        remove(filename);
        free(hash);
        return;
    }

    // Add the hash to the hash table
    add_hash(hash);

    // Prepare the destination directory
    char newdir[256];
    snprintf(newdir, sizeof(newdir), "%s\\%s", sorted_root_directory, mime_type);
    if (!create_directory(newdir))
    {
        printf("Error creating destination directory: %s\n", newdir);
        free(hash);
        return;
    }

    // Generate the new file path in the sorted directory
    char newname[256];
    const char *file_extension = strrchr(filename, '.');
    snprintf(newname, sizeof(newname), "%s\\%s%s", newdir, hash, file_extension);

    // Generate the new source file path with the hashed name
    char new_filepath[256];
    strcpy(new_filepath, filename);  // Copy the current file path
    char *last_slash = strrchr(new_filepath, '\\');  // Find the last occurrence of backslash
    if (last_slash != NULL) {
        strcpy(last_slash + 1, hash);  // Replace the portion after the last backslash with the hash value
        strcat(new_filepath, file_extension);  // Append the file extension
    } else {
        // Handle the case when there's no backslash in the current file path
        printf("Invalid file path: %s\n", filename);
        free(hash);
        return;
    }


    // Generate escaped paths for source and destination files
    char escaped_filename[2 * MAX_BUF_SIZE + 1];
    char escaped_newname[2 * MAX_BUF_SIZE + 1];

    snprintf(escaped_filename, sizeof(escaped_filename), "%s", filename);
    snprintf(escaped_newname, sizeof(escaped_newname), "%s", newname);
    escape_file_path(escaped_filename);
    escape_file_path(escaped_newname);

    replace_forward_slashes(newname);

    // Declare a flag variable
    int copied = 0;

    // Rename the file to its hash
    if (rename(filename, new_filepath) != 0)
    {
        printf("Error renaming file: %s\n", new_filepath);
        free(hash);
        return;
    }

    // Create the copy command with escaped file paths
    char cmd[1024];
    #ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", new_filepath, newname);
    #else
    snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", escaped_filename, escaped_newname);
    #endif

    if(!copied) {
        // Execute the copy command
        int resultCode = system(cmd);
        if (resultCode != 0)
        {
            printf("Error copying file: %s\n", new_filepath);
            printf("Error code: %d\n", resultCode);
            free(hash);
            return;
        } else{
            if(remove(new_filepath) != 0)
            {
                printf("Error Deleting File: %s", new_filepath);
            }
        }
        // Set the flag to indicate that the file has been copied
        copied = 1;
    }
    free(hash);
}

int remove_directory(const char *path) {
    // Check if the directory is a symbolic link
    #ifdef _WIN32
        DWORD attributes = GetFileAttributes(path);
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            // Use Windows API to remove symbolic link directory
            if (RemoveDirectory(path)) {
                return 0;  // Removal succeeded
            } else {
                return -1;  // Removal failed
            }
        }
    #else
        struct stat st;
        if (lstat(path, &st) == 0 && S_ISLNK(st.st_mode)) {
            // Use system command to remove symbolic link directory on Unix-like systems
            char command[256];
            snprintf(command, sizeof(command), "rmdir \"%s\"", path);
            if (system(command) == 0) {
                return 0;  // Removal succeeded
            } else {
                return -1;  // Removal failed
            }
        }
    #endif

    DIR *dir = opendir(path);
    if (!dir) {
        printf("Error opening directory: %s\n", path);
        return -1;
    }

    int r = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char file_path[MAX_BUF_SIZE];
        snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(file_path, &st) == -1) {
            printf("Error getting file/directory information: %s\n", file_path);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            r = remove_directory(file_path);
            if (r != 0) {
                closedir(dir);
                return r;
            }
        } else if (S_ISREG(st.st_mode)) {
            if (strcmp(entry->d_name, "desktop.ini") != 0) {
                printf("Deleting invalid file: %s\n", file_path);
                r = remove(file_path);
                if (r != 0) {
                    printf("Error deleting file: %s\n", file_path);
                    closedir(dir);
                    return r;
                }
            }
        }
    }

    closedir(dir);

    // Check if the directory is empty or contains only "desktop.ini"
    if (is_directory_empty(path)) {
        #ifdef _WIN32
            // Convert path to wide-character string
            wchar_t wide_path[MAX_BUF_SIZE];
            MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, sizeof(wide_path) / sizeof(wide_path[0]));

            // Remove the directory using Windows API
            if (!RemoveDirectoryW(wide_path)) {
                //printf("Error deleting directory: %s\n", path);
                r = -1;
            }
        #else
            r = rmdir(path);
            if (r != 0) {
                printf("Error deleting directory: %s\n", path);
            }
        #endif
    }

    return r;
}


int is_directory_empty(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        printf("Error opening directory: %s (%s)\n", path, strerror(errno));
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, "desktop.ini") == 0) {
            continue;
        }
        closedir(dir);
        return 0;  // Directory is not empty
    }

    closedir(dir);
    return 1;  // Directory is empty
}

void strlower(char* str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char) str[i]);
    }
}

const char *get_mime_type(const char *filename) {
    if (strcmp(filename, "desktop.ini") == 0)
        return NULL;  // Ignore "desktop.ini" files

    const char *extension = strrchr(filename, '.');
    if (extension == NULL)
        return NULL;

    // Duplicate the extension and convert to lower case
    char *lower_ext = strdup(extension);
    strlower(lower_ext);

    if (strcmp(lower_ext, ".jpg") == 0 || strcmp(lower_ext, ".jpeg") == 0)
        return "image\\jpeg";
    else if (strcmp(lower_ext, ".png") == 0)
        return "image\\png";
    else if (strcmp(lower_ext, ".gif") == 0)
        return "image\\gif";
    else if (strcmp(lower_ext, ".webp") == 0)
        return "image\\webp";
    else if (strcmp(lower_ext, ".svg") == 0)
        return "image\\svg+xml";
    else if (strcmp(lower_ext, ".mp4") == 0)
        return "video\\mp4";
    else if (strcmp(lower_ext, ".psd") == 0)
        return "image\\vnd.adobe.photoshop";
    else if (strcmp(lower_ext, ".pdf") == 0)
        return "application\\pdf";
    else if (strcmp(lower_ext, ".docx") == 0)
        return "application\\vnd.openxmlformats-officedocument.wordprocessingml.document";
    else if (strcmp(lower_ext, ".xlsx") == 0)
        return "application\\vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    else if (strcmp(lower_ext, ".xlsm") == 0)
        return "application\\vnd.ms-excel.sheet.macroEnabled.12";
    else if (strcmp(lower_ext, ".pptx") == 0)
        return "application\\vnd.openxmlformats-officedocument.presentationml.presentation";
    else if (strcmp(lower_ext, ".php") == 0)
        return "application\\x-httpd-php";
    else if (strcmp(lower_ext, ".jnlp") == 0)
        return "application\\x-java-jnlp-file";
    else if (strcmp(lower_ext, ".zip") == 0)
        return "application\\zip";
    else if (strcmp(lower_ext, ".rdp") == 0)
        return "application\\rdp";
    else if (strcmp(lower_ext, ".rtf") == 0)
        return "application\\rtf";
    else if (strcmp(lower_ext, ".msg") == 0)
        return "application\\vnd. ms-outlook";
    else if (strcmp(lower_ext, ".iso") == 0)
        return "application\\x-iso9660-image";
    else if (strcmp(lower_ext, ".ini") == 0)
        return "text\\plain";
    else if (strcmp(lower_ext, ".c") == 0)
        return "text\\plain";
    else if (strcmp(lower_ext, ".css") == 0)
        return "text\\plain";
    else if (strcmp(lower_ext, ".txt") == 0)
        return "text\\plain";
    else if (strcmp(lower_ext, ".sql") == 0)
        return "text\\plain";
    else if (strcmp(lower_ext, ".js") == 0)
        return "text\\javascript";
    else if (strcmp(lower_ext, ".htm") == 0)
        return "text\\html";
    else if (strcmp(lower_ext, ".html") == 0)
        return "text\\html";
    else if (strcmp(lower_ext, ".env") == 0)
        return "text\\plain";
    else if (strcmp(lower_ext, ".yml") == 0)
        return "text\\plain";
    else if (strcmp(lower_ext, ".md") == 0)
        return "text\\markdown";
    else if (strcmp(lower_ext, ".msi") == 0)
        return "application\\x-ms-installer";
    else if (strcmp(lower_ext, ".exe") == 0)
        return "application\\vnd.microsoft.portable-executable";
    else if (strcmp(lower_ext, ".sh") == 0)
        return "text\\x-shellscript";
    else if (strcmp(lower_ext, ".csv") == 0)
        return "text\\csv";
    else
        return NULL;
}

// Main function
int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Usage: %s <ingest_directory> <sorted_root_directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *ingest_directory = argv[1];
    const char *sorted_root_directory = argv[2];

    // Populate hash table with existing files.
    build_hash_table(sorted_root_directory);

    // Process files recursively
    process_files_recursive(ingest_directory, sorted_root_directory);

    return 0;
}
