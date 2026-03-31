#!/bin/bash
# Build script for ARM-Voxel-Survival
# Optimized for low-end ARM devices (e.g. R36S, RG351P)

CC=gcc
CFLAGS="-O3 -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -ffast-math"
LIBS="-lraylib -lGL -lm -lpthread -ldl -lrt -lX11"

echo "Compiling r36sCraft..."
$CC main.c -o r36scraft $CFLAGS $LIBS

if [ $? -eq 0 ]; then
    echo "Build successful!"
    
    # Create PortMaster structure
    mkdir -p port_package/r36scraft
    cp r36scraft port_package/r36scraft/
    cp port.json port_package/r36scraft/
    cp r36scraft.sh port_package/
    chmod +x port_package/r36scraft.sh
    chmod +x port_package/r36scraft/r36scraft
    
    echo "PortMaster package created in 'port_package/' directory."
    echo "To install: Copy the CONTENTS of 'port_package/' to your SD card's /roms/ports/ directory."
    echo "The structure should be:"
    echo "  /roms/ports/r36scraft.sh"
    echo "  /roms/ports/r36scraft/r36scraft"
    echo "  /roms/ports/r36scraft/port.json"
    
    if command -v zip >/dev/null 2>&1; then
        cd port_package && zip -r ../r36scraft.zip . && cd ..
        echo "Created r36scraft.zip for easy transfer."
    fi
else
    echo "Build failed."
fi
