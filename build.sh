#!/bin/bash
# Build script for ARM-Voxel-Survival
# Optimized for low-end ARM devices (e.g. R36S, RG351P)

CC=gcc
CFLAGS="-O3 -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -ffast-math"
LIBS="-lraylib -lGL -lm -lpthread -ldl -lrt -lX11"

echo "Compiling ARM-Voxel-Survival..."
$CC main.c -o voxel_game $CFLAGS $LIBS

if [ $? -eq 0 ]; then
    echo "Build successful! Run with ./voxel_game"
else
    echo "Build failed."
fi
