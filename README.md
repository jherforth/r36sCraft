# ARM-Voxel-Survival

A ruthlessly optimized voxel game written in C using raylib 5.0. Designed to run on low-RAM ARM devices (<= 500MB RAM) like the R36S or RG351 series.

## Features
- **Greedy Meshing (Face Culling):** Optimized chunk rendering to keep triangle counts low.
- **No Textures:** Uses vertex colors for ultra-low memory footprint.
- **Infinite World:** Procedural generation with biomes (Plains, Forest, Desert).
- **Survival Elements:** Health and Hunger systems.
- **Low RAM Usage:** Designed to stay under 350MB RAM with 10+ chunks loaded.

## Controls
- **WASD:** Move
- **Mouse:** Look
- **Space:** Jump
- **Left Click:** Break Block
- **Right Click:** Place Block (Coming soon in next update)
- **1-9:** Select block type
- **ESC:** Exit

## Compilation
To compile for an ARM device (Linux):
1. Ensure `raylib` 5.0+ is installed.
2. Run the build script:
   ```bash
   chmod +x build.sh
   ./build.sh
   ```

## Optimization Notes
- **Chunk Size:** 16x16x128.
- **Render Distance:** 5 chunks (160 blocks).
- **Memory:** Uses 8-bit block indices. Mesh data is rebuilt only on change.
- **CPU:** Simple AABB physics and basic noise generation.

## Customization
Change the `RENDER_DIST` constant in `main.c` to adjust performance vs. visual range.
Change `world_seed` for a different terrain layout.
