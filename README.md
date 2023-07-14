# Vortex
An alternate file organisation system. 

## Compile Instructions
### Main Program
```
gcc -g vortex.c -o vortex -lcrypto -lmagic
```
### GUI
```
gcc vortex-gui.c -o vortex-gui `pkg-config --cflags --libs gtk+-3.0`
```

## Run Instructions
### GUI
```
./vortex-gui
```
### CLI
```
./vortex _ingest directory_ _Vortexed directory_
```
