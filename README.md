# r36sCraft

A ruthlessly optimized voxel game written in C using raylib 5.0. Designed to run on low-RAM ARM devices (<= 500MB RAM) like the R36S or RG351 series.

## Features
- **Greedy Meshing (Face Culling):** Optimized chunk rendering to keep triangle counts low.
- **No Textures:** Uses vertex colors for ultra-low memory footprint.
- **Infinite World:** Procedural generation with biomes (Plains, Forest, Desert).
- **Survival Elements:** Health and Hunger systems.
- **Low RAM Usage:** Designed to stay under 350MB RAM with 10+ chunks loaded.

## Controls
### Keyboard & Mouse
- **WASD:** Move
- **Mouse:** Look
- **Space:** Jump
- **Left Click:** Break Block
- **Right Click:** Place Block
- **1-9:** Select block type
- **ESC:** Exit

### R36S / Handheld Gamepad
- **Left Stick:** Move
- **Right Stick:** Look
- **A Button:** Jump
- **R2 (Right Trigger):** Break Block
- **L2 (Left Trigger):** Place Block
- **L1 / R1:** Cycle through block types
- **Start + Select:** Exit (Standard ArkOS shortcut)

## Compilation & Setup
To build **r36sCraft** on a Linux (Ubuntu/Debian) system, run the following commands:

```bash
# Install dependencies
sudo apt update
sudo apt install -y build-essential git cmake libraylib-dev \
    libasound2-dev libx11-dev libxrandr-dev libxi-dev \
    libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev \
    libxinerama-dev libwayland-dev libxkbcommon-dev

# Build the project
chmod +x build.sh
./build.sh
```

> **Note:** If building on a standard PC (non-ARM), remove `-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard` from `build.sh`.

## PortMaster Installation (R36S / Handhelds)
1.  **Compile the game** on your ARM device (or cross-compile) using `./build.sh`.
2.  **Locate the `port_package/` folder** created by the build script.
3.  **Copy the files** to your SD card:
    -   Copy `r36scraft.sh` to `/roms/ports/`
    -   Copy the `r36scraft/` folder to `/roms/ports/`
4.  **Restart your device** and find "r36sCraft" in the Ports menu.

## Optimization Notes
- **Chunk Size:** 16x16x128.
- **Render Distance:** 5 chunks (160 blocks).
- **Memory:** Uses 8-bit block indices. Mesh data is rebuilt only on change.
- **CPU:** Simple AABB physics and basic noise generation.

## Customization
Change the `RENDER_DIST` constant in `main.c` to adjust performance vs. visual range.
Change `world_seed` for a different terrain layout.
