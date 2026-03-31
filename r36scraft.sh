#!/bin/bash
# PortMaster Launch Script for r36sCraft

# Get the directory where the script is located
PORTDIR=$(dirname "$0")
GAMEDIR="$PORTDIR/r36scraft"
cd "$GAMEDIR"

# Standard PortMaster log setup
exec > >(tee "$GAMEDIR/log.txt") 2>&1

# Set up environment variables for Raylib/OpenGL on handhelds
export LD_LIBRARY_PATH="$GAMEDIR/libs:$LD_LIBRARY_PATH"
export SDL_GAMECONTROLLERCONFIG_FILE="$GAMEDIR/gamecontrollerdb.txt"

# Run the game binary
chmod +x ./r36scraft
./r36scraft
