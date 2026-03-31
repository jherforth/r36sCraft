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
    
    echo "PortMaster package created in 'port_package/' directory."
    echo "To install: Copy 'r36scraft.sh' and 'r36scraft/' folder to your SD card's /roms/ports/ directory."
else
    echo "Build failed."
fi
