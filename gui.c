#include <gtk/gtk.h>
#include <dirent.h>

GtkWidget *box;  // This will be the main box that contains everything
GtkWidget *tree_box;  // This will hold the tree views
GtkWidget *button_box;  // This will hold the buttons
GtkWidget *ingest_box;  // This will hold the tree view for the ingest directory
GtkWidget *sorted_box;  // This will hold the tree view for the sorted directory
GtkWidget *ingest_button, *sorted_button;  // The two buttons
GtkWidget *ingest_tree_view;  // Tree view for the ingest directory
GtkWidget *sorted_tree_view;  // Tree view for the sorted directory
GtkWidget *window;
GtkWidget *ingest_scrolled_window;
GtkWidget *sorted_scrolled_window;

GtkWidget *execute_button;
char *ingest_folder = NULL;
char *sorted_folder = NULL;
int folders_selected_count = 0;



GtkWidget *create_tree_view(const char *path);

void on_ingest_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new("Choose a folder", GTK_WINDOW(window),
                                         GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Open", GTK_RESPONSE_ACCEPT, NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *folder;
        folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        ingest_folder = g_strdup(folder);
        if (ingest_tree_view != NULL) {
            gtk_container_remove(GTK_CONTAINER(ingest_box), GTK_WIDGET(ingest_scrolled_window));
        }
        ingest_tree_view = create_tree_view(folder);
        ingest_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_container_add(GTK_CONTAINER(ingest_scrolled_window), ingest_tree_view);
        gtk_box_pack_start(GTK_BOX(ingest_box), ingest_scrolled_window, TRUE, TRUE, 0);
        gtk_widget_show(ingest_tree_view);
        gtk_widget_show(ingest_scrolled_window);
        g_free(folder);
        folders_selected_count++;
        if (folders_selected_count == 2) {
            gtk_widget_set_sensitive(execute_button, TRUE);
        }
    }
    gtk_widget_destroy(dialog);
}

void on_sorted_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new("Choose a folder", GTK_WINDOW(window),
                                         GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Open", GTK_RESPONSE_ACCEPT, NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *folder;
        folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        sorted_folder = g_strdup(folder);
        if (sorted_tree_view != NULL) {
            gtk_container_remove(GTK_CONTAINER(sorted_box), GTK_WIDGET(sorted_scrolled_window));
        }
        sorted_tree_view = create_tree_view(folder);
        sorted_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_container_add(GTK_CONTAINER(sorted_scrolled_window), sorted_tree_view);
        gtk_box_pack_start(GTK_BOX(sorted_box), sorted_scrolled_window, TRUE, TRUE, 0);
        gtk_widget_show(sorted_tree_view);
        gtk_widget_show(sorted_scrolled_window);
        g_free(folder);
        folders_selected_count++;
        if (folders_selected_count == 2) {
            gtk_widget_set_sensitive(execute_button, TRUE);
        }
    }
    gtk_widget_destroy(dialog);
}

void on_execute_button_clicked(GtkWidget *widget, gpointer data) {
    GError *error = NULL;

    g_print("Ingest folder: %s\n", ingest_folder);  // Debug print
    g_print("Sorted folder: %s\n", sorted_folder);  // Debug print

    #ifdef _WIN32
    // On Windows, use "test.exe"
    char *argv[] = {"test.exe", ingest_folder, sorted_folder, NULL};
    #else
    // On Unix-like systems, use "./test"
    char *argv[] = {"./test", ingest_folder, sorted_folder, NULL};
    #endif

    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error)) {
        g_printerr("Error executing test: %s\n", error->message);
        g_error_free(error);
    }
}

// Function to fill the tree view with data
void populate_model(GtkListStore *store, const char *path) {
    DIR *dir;
    struct dirent *entry;
    GtkTreeIter iter;

    dir = opendir(path);
    if (dir == NULL) {
        perror("Failed to open directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')  // Ignore hidden files and directories
            continue;
        
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, entry->d_name, -1);
    }

    closedir(dir);
}

// Function to create a tree view
GtkWidget *create_tree_view(const char *path) {
    GtkWidget *tree_view;
    GtkListStore *store;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    // Create a new list store with one column of type G_TYPE_STRING
    store = gtk_list_store_new(1, G_TYPE_STRING);

    // Create a new tree view widget and set the model
    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);  // The tree view has taken ownership of the model

    // Create a new text cell renderer
    renderer = gtk_cell_renderer_text_new();

    // Create a new column using the renderer
    column = gtk_tree_view_column_new_with_attributes("Filename", renderer, "text", 0, NULL);

    // Add the column to the tree view
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    // Fill the model with data
    populate_model(store, path);

    return tree_view;
}

// This is the callback for the "destroy" event of the window.
static void on_destroy(GtkWidget *widget, gpointer data) {
    gtk_main_quit();
}

int main(int argc, char *argv[]) {
    GtkWidget *window;

    gtk_init(&argc, &argv);

    // Create the window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "File Organizer");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Create the main box, which will hold everything
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), box);

    // Create the tree_box, which will hold the tree views
    tree_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(box), tree_box, TRUE, TRUE, 0);

    // Create the button_box, which will hold the buttons
    button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(box), button_box, FALSE, FALSE, 0);

    // Create the ingest and sorted boxes, and add them to the tree_box
    ingest_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    sorted_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(tree_box), ingest_box, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(tree_box), sorted_box, TRUE, TRUE, 0);

    // Create the buttons and add them to the button_box
    ingest_button = gtk_button_new_with_label("Select Ingest Directory");
    sorted_button = gtk_button_new_with_label("Select Sorted Directory");
    execute_button = gtk_button_new_with_label("Run test.exe");
    gtk_widget_set_sensitive(execute_button, FALSE);  // Disable the button by default

    gtk_box_pack_start(GTK_BOX(button_box), ingest_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), sorted_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), execute_button, FALSE, FALSE, 0);

    // Connect the buttons to their callback functions
    g_signal_connect(ingest_button, "clicked", G_CALLBACK(on_ingest_button_clicked), NULL);
    g_signal_connect(sorted_button, "clicked", G_CALLBACK(on_sorted_button_clicked), NULL);
    g_signal_connect(execute_button, "clicked", G_CALLBACK(on_execute_button_clicked), NULL);

    // Show all widgets
    gtk_widget_show_all(window);

    gtk_main();

    return 0;
}
