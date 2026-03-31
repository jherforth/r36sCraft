/**
 * ARM-Voxel-Survival
 * A ruthlessly optimized voxel game for low-RAM ARM devices.
 * Built with C and raylib 5.0.
 */

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

// --- Constants ---
#define CHUNK_X 16
#define CHUNK_Z 16
#define CHUNK_Y 128
#define CHUNK_VOL (CHUNK_X * CHUNK_Z * CHUNK_Y)
#define RENDER_DIST 5
#define WORLD_SIZE (RENDER_DIST * 2 + 1)
#define MAX_CHUNKS (WORLD_SIZE * WORLD_SIZE)

#define PLAYER_HEIGHT 1.8f
#define PLAYER_RADIUS 0.3f
#define GRAVITY -18.0f
#define JUMP_FORCE 7.0f
#define WALK_SPEED 5.0f

// --- Types ---
typedef enum {
    BLOCK_AIR = 0,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_WOOD,
    BLOCK_LEAVES,
    BLOCK_SAND,
    BLOCK_WATER,
    BLOCK_GLOWSTONE,
    BLOCK_SNOW,
    BLOCK_COUNT
} BlockType;

typedef struct {
    uint8_t type;
} Block;

typedef struct {
    int x, z;
    Block blocks[CHUNK_VOL];
    Mesh mesh;
    Model model;
    bool loaded;
    bool dirty;
    bool has_mesh;
} Chunk;

typedef struct {
    Vector3 pos;
    Vector3 vel;
    float yaw, pitch;
    float health;
    float hunger;
    int selected_block;
    bool grounded;
    float damage_timer;
} Player;

typedef enum { MOB_CHICKEN, MOB_ZOMBIE } MobType;
typedef struct {
    Vector3 pos;
    MobType type;
    float health;
    Vector3 vel;
    bool active;
} Mob;

// --- Global State ---
Chunk chunks[MAX_CHUNKS];
Player player;
Mob mobs[32];
int world_seed = 12345;

// --- Noise Functions (Simple Perlin-like) ---
float hash(float n) { return fmodf(sinf(n) * 43758.5453123f, 1.0f); }
float noise2d(float x, float y) {
    float ix = floorf(x);
    float iy = floorf(y);
    float fx = x - ix;
    float fy = y - iy;
    float a = hash(ix + iy * 57.0f);
    float b = hash(ix + 1.0f + iy * 57.0f);
    float c = hash(ix + (iy + 1.0f) * 57.0f);
    float d = hash(ix + 1.0f + (iy + 1.0f) * 57.0f);
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);
    return Lerp(Lerp(a, b, ux), Lerp(c, d, ux), uy);
}

float fbm2d(float x, float y, int octaves) {
    float val = 0;
    float amp = 0.5;
    for (int i = 0; i < octaves; i++) {
        val += noise2d(x, y) * amp;
        x *= 2.1; y *= 2.1;
        amp *= 0.5;
    }
    return val;
}

// --- Block Properties ---
Color GetBlockColor(BlockType type, int face) {
    Color base;
    switch (type) {
        case BLOCK_GRASS:
            if (face == 2) base = (Color){ 100, 200, 50, 255 }; // Top
            else if (face == 3) base = (Color){ 100, 70, 40, 255 };  // Bottom
            else base = (Color){ 80, 150, 40, 255 };                // Sides
            break;
        case BLOCK_DIRT: base = (Color){ 100, 70, 40, 255 }; break;
        case BLOCK_STONE: base = (Color){ 120, 120, 120, 255 }; break;
        case BLOCK_WOOD: base = (Color){ 80, 50, 30, 255 }; break;
        case BLOCK_LEAVES: base = (Color){ 40, 180, 40, 180 }; break;
        case BLOCK_SAND: base = (Color){ 220, 200, 120, 255 }; break;
        case BLOCK_WATER: base = (Color){ 50, 100, 200, 150 }; break;
        case BLOCK_GLOWSTONE: base = (Color){ 255, 255, 150, 255 }; break;
        case BLOCK_SNOW: base = (Color){ 240, 240, 255, 255 }; break;
        default: base = WHITE; break;
    }

    // Apply shading based on face (Top = 1.0, Sides = 0.8, Bottom = 0.6)
    float factor = 0.8f;
    if (face == 2) factor = 1.0f; // Top
    if (face == 3) factor = 0.6f; // Bottom
    
    if (type == BLOCK_GLOWSTONE) factor = 1.0f; // Glowstone doesn't shade

    return (Color){ (unsigned char)(base.r * factor), (unsigned char)(base.g * factor), (unsigned char)(base.b * factor), base.a };
}

// --- Persistence ---
void SaveChunk(Chunk *c) {
    char filename[64];
    sprintf(filename, "chunk_%d_%d.dat", c->x, c->z);
    FILE *f = fopen(filename, "wb");
    if (f) {
        fwrite(c->blocks, sizeof(Block), CHUNK_VOL, f);
        fclose(f);
    }
}

bool LoadChunk(Chunk *c) {
    char filename[64];
    sprintf(filename, "chunk_%d_%d.dat", c->x, c->z);
    FILE *f = fopen(filename, "rb");
    if (f) {
        size_t read = fread(c->blocks, sizeof(Block), CHUNK_VOL, f);
        fclose(f);
        if (read == CHUNK_VOL) {
            c->loaded = true;
            c->dirty = true;
            return true;
        }
    }
    return false;
}

void SavePlayer() {
    FILE *f = fopen("player.dat", "wb");
    if (f) {
        fwrite(&player, sizeof(Player), 1, f);
        fclose(f);
    }
}

void LoadPlayer() {
    FILE *f = fopen("player.dat", "rb");
    if (f) {
        size_t read = fread(&player, sizeof(Player), 1, f);
        fclose(f);
        if (read != 1) {
            // Reset player if load fails
            player.pos = (Vector3){ 0, 80, 0 };
            player.health = 3.0f;
            player.hunger = 10.0f;
        }
    }
}

// --- World Generation ---
void GenerateChunk(Chunk *c) {
    if (LoadChunk(c)) return;

    for (int x = 0; x < CHUNK_X; x++) {
        for (int z = 0; z < CHUNK_Z; z++) {
            float nx = (float)(c->x * CHUNK_X + x) * 0.01f;
            float nz = (float)(c->z * CHUNK_Z + z) * 0.01f;
            
            float h_noise = fbm2d(nx, nz, 4);
            int height = 64 + (int)(h_noise * 30.0f);
            
            // Biome logic (3 biomes: Forest, Desert, Tundra)
            float biome_noise = noise2d(nx * 0.05f, nz * 0.05f);
            
            for (int y = 0; y < CHUNK_Y; y++) {
                BlockType t = BLOCK_AIR;
                if (y < height - 4) t = BLOCK_STONE;
                else if (y < height - 1) t = BLOCK_DIRT;
                else if (y < height) {
                    if (biome_noise > 0.7f) t = BLOCK_SAND; // Desert
                    else if (biome_noise < 0.3f) t = BLOCK_SNOW; // Tundra
                    else t = BLOCK_GRASS; // Forest
                }
                
                if (y < 65 && t == BLOCK_AIR) t = BLOCK_WATER;
                
                c->blocks[x + z * CHUNK_X + y * CHUNK_X * CHUNK_Z].type = t;
            }
            
            // Simple Trees (Forest and Tundra only)
            if (biome_noise <= 0.7f && biome_noise >= 0.2f && hash(nx * 100 + nz) > 0.98f && height > 66) {
                for(int ty=0; ty<4; ty++) c->blocks[x + z * CHUNK_X + (height+ty) * CHUNK_X * CHUNK_Z].type = BLOCK_WOOD;
                for(int lx=-1; lx<=1; lx++) {
                    for(int lz=-1; lz<=1; lz++) {
                        for(int ly=0; ly<2; ly++) {
                            int ox = x+lx, oz = z+lz, oy = height+3+ly;
                            if (ox >= 0 && ox < CHUNK_X && oz >= 0 && oz < CHUNK_Z && oy < CHUNK_Y) {
                                if (biome_noise < 0.3f)
                                    c->blocks[ox + oz * CHUNK_X + oy * CHUNK_X * CHUNK_Z].type = BLOCK_SNOW;
                                else
                                    c->blocks[ox + oz * CHUNK_X + oy * CHUNK_X * CHUNK_Z].type = BLOCK_LEAVES;
                            }
                        }
                    }
                }
            }
        }
    }
    c->dirty = true;
    c->loaded = true;
}

// --- Greedy Meshing ---
void BuildChunkMesh(Chunk *c) {
    if (c->has_mesh) {
        UnloadModel(c->model);
        c->has_mesh = false;
    }

    // Temporary buffers for greedy meshing
    // This is a simplified greedy mesher that handles face culling and basic merging
    // For 16x16x128, we can afford some temporary memory
    
    int vertexCount = 0;
    int maxVertices = CHUNK_VOL * 36; // Overkill but safe for stack if small, better use heap
    float *vertices = (float *)malloc(maxVertices * 3 * sizeof(float));
    float *normals = (float *)malloc(maxVertices * 3 * sizeof(float));
    unsigned char *colors = (unsigned char *)malloc(maxVertices * 4 * sizeof(unsigned char));

    int vIdx = 0;

    for (int f = 0; f < 6; f++) {
        // 0:Left, 1:Right, 2:Top, 3:Bottom, 4:Front, 5:Back
        for (int y = 0; y < CHUNK_Y; y++) {
            for (int z = 0; z < CHUNK_Z; z++) {
                for (int x = 0; x < CHUNK_X; x++) {
                    BlockType t = c->blocks[x + z * CHUNK_X + y * CHUNK_X * CHUNK_Z].type;
                    if (t == BLOCK_AIR) continue;

                    bool draw = false;
                    if (f == 0 && (x == 0 || c->blocks[(x - 1) + z * CHUNK_X + y * CHUNK_X * CHUNK_Z].type == BLOCK_AIR)) draw = true;
                    if (f == 1 && (x == CHUNK_X - 1 || c->blocks[(x + 1) + z * CHUNK_X + y * CHUNK_X * CHUNK_Z].type == BLOCK_AIR)) draw = true;
                    if (f == 2 && (y == CHUNK_Y - 1 || c->blocks[x + z * CHUNK_X + (y + 1) * CHUNK_X * CHUNK_Z].type == BLOCK_AIR)) draw = true;
                    if (f == 3 && (y == 0 || c->blocks[x + z * CHUNK_X + (y - 1) * CHUNK_X * CHUNK_Z].type == BLOCK_AIR)) draw = true;
                    if (f == 4 && (z == 0 || c->blocks[x + (z - 1) * CHUNK_X + y * CHUNK_X * CHUNK_Z].type == BLOCK_AIR)) draw = true;
                    if (f == 5 && (z == CHUNK_Z - 1 || c->blocks[x + (z + 1) * CHUNK_X + y * CHUNK_X * CHUNK_Z].type == BLOCK_AIR)) draw = true;

                    if (draw) {
                        Color col = GetBlockColor(t, f);
                        
                        // Add detail: jitter color based on block position
                        float jitter = (float)((x ^ z ^ y) % 10) / 100.0f;
                        col.r = (unsigned char)fmaxf(0, fminf(255, col.r + jitter * 255));
                        col.g = (unsigned char)fmaxf(0, fminf(255, col.g + jitter * 255));
                        col.b = (unsigned char)fmaxf(0, fminf(255, col.b + jitter * 255));

                        // Vertical gradient for more detail
                        float grad = 0.9f + (float)(y % 4) * 0.05f;
                        col.r = (unsigned char)fminf(255, col.r * grad);
                        col.g = (unsigned char)fminf(255, col.g * grad);
                        col.b = (unsigned char)fminf(255, col.b * grad);

                        // Add 2 triangles (6 vertices)
                        // This is face culling. True greedy merging would combine these.
                        // For brevity and reliability in a single file, we do face culling.
                        float x0 = x, x1 = x + 1, y0 = y, y1 = y + 1, z0 = z, z1 = z + 1;
                        Vector3 v[4];
                        Vector3 n;
                        if (f == 0) { v[0]=(Vector3){x0,y0,z0}; v[1]=(Vector3){x0,y0,z1}; v[2]=(Vector3){x0,y1,z1}; v[3]=(Vector3){x0,y1,z0}; n=(Vector3){-1,0,0}; }
                        if (f == 1) { v[0]=(Vector3){x1,y0,z1}; v[1]=(Vector3){x1,y0,z0}; v[2]=(Vector3){x1,y1,z0}; v[3]=(Vector3){x1,y1,z1}; n=(Vector3){1,0,0}; }
                        if (f == 2) { v[0]=(Vector3){x0,y1,z0}; v[1]=(Vector3){x0,y1,z1}; v[2]=(Vector3){x1,y1,z1}; v[3]=(Vector3){x1,y1,z0}; n=(Vector3){0,1,0}; }
                        if (f == 3) { v[0]=(Vector3){x0,y0,z1}; v[1]=(Vector3){x0,y0,z0}; v[2]=(Vector3){x1,y0,z0}; v[3]=(Vector3){x1,y0,z1}; n=(Vector3){0,-1,0}; }
                        if (f == 4) { v[0]=(Vector3){x1,y0,z0}; v[1]=(Vector3){x0,y0,z0}; v[2]=(Vector3){x0,y1,z0}; v[3]=(Vector3){x1,y1,z0}; n=(Vector3){0,0,-1}; }
                        if (f == 5) { v[0]=(Vector3){x0,y0,z1}; v[1]=(Vector3){x1,y0,z1}; v[2]=(Vector3){x1,y1,z1}; v[3]=(Vector3){x0,y1,z1}; n=(Vector3){0,0,1}; }

                        int indices[] = { 0, 1, 2, 0, 2, 3 };
                        for (int i = 0; i < 6; i++) {
                            vertices[vIdx * 3 + 0] = v[indices[i]].x;
                            vertices[vIdx * 3 + 1] = v[indices[i]].y;
                            vertices[vIdx * 3 + 2] = v[indices[i]].z;
                            normals[vIdx * 3 + 0] = n.x;
                            normals[vIdx * 3 + 1] = n.y;
                            normals[vIdx * 3 + 2] = n.z;
                            colors[vIdx * 4 + 0] = col.r;
                            colors[vIdx * 4 + 1] = col.g;
                            colors[vIdx * 4 + 2] = col.b;
                            colors[vIdx * 4 + 3] = col.a;
                            vIdx++;
                        }
                    }
                }
            }
        }
    }

    c->mesh = (Mesh){ 0 };
    c->mesh.vertexCount = vIdx;
    c->mesh.triangleCount = vIdx / 3;
    c->mesh.vertices = (float *)malloc(vIdx * 3 * sizeof(float));
    c->mesh.normals = (float *)malloc(vIdx * 3 * sizeof(float));
    c->mesh.colors = (unsigned char *)malloc(vIdx * 4 * sizeof(unsigned char));
    memcpy(c->mesh.vertices, vertices, vIdx * 3 * sizeof(float));
    memcpy(c->mesh.normals, normals, vIdx * 3 * sizeof(float));
    memcpy(c->mesh.colors, colors, vIdx * 4 * sizeof(unsigned char));

    UploadMesh(&c->mesh, false);
    c->model = LoadModelFromMesh(c->mesh);
    c->has_mesh = true;
    c->dirty = false;

    free(vertices);
    free(normals);
    free(colors);
}

// --- Physics & Collision ---
bool IsBlockSolid(int x, int y, int z) {
    if (y < 0 || y >= CHUNK_Y) return false;
    int cx = floorf((float)x / CHUNK_X);
    int cz = floorf((float)z / CHUNK_Z);
    for (int i = 0; i < MAX_CHUNKS; i++) {
        if (chunks[i].loaded && chunks[i].x == cx && chunks[i].z == cz) {
            int lx = x - cx * CHUNK_X;
            int lz = z - cz * CHUNK_Z;
            BlockType t = chunks[i].blocks[lx + lz * CHUNK_X + y * CHUNK_X * CHUNK_Z].type;
            return (t != BLOCK_AIR && t != BLOCK_WATER);
        }
    }
    return false;
}

void UpdatePlayer(float dt) {
    if (player.health <= 0) {
        player.pos = (Vector3){ 0, 80, 0 };
        player.health = 3.0f;
        player.hunger = 10.0f;
        player.vel = (Vector3){ 0, 0, 0 };
        return;
    }

    // --- Rotation (Mouse + Right Stick) ---
    Vector2 mouseDelta = GetMouseDelta();
    player.yaw -= mouseDelta.x * 0.1f;
    player.pitch -= mouseDelta.y * 0.1f;

    if (IsGamepadAvailable(0)) {
        float rx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X);
        float ry = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y);
        if (fabsf(rx) > 0.1f) player.yaw -= rx * 2.0f;
        if (fabsf(ry) > 0.1f) player.pitch -= ry * 2.0f;
    }

    if (player.pitch > 89) player.pitch = 89;
    if (player.pitch < -89) player.pitch = -89;

    // --- Movement (WASD + Left Stick) ---
    Vector3 forward = (Vector3){ sinf(player.yaw * DEG2RAD), 0, cosf(player.yaw * DEG2RAD) };
    Vector3 right = (Vector3){ cosf(player.yaw * DEG2RAD), 0, -sinf(player.yaw * DEG2RAD) };
    Vector3 move = { 0 };
    
    if (IsKeyDown(KEY_W)) move = Vector3Add(move, forward);
    if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, forward);
    if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, right);
    if (IsKeyDown(KEY_D)) move = Vector3Add(move, right);

    if (IsGamepadAvailable(0)) {
        float lx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        float ly = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        if (fabsf(lx) > 0.1f) move = Vector3Add(move, Vector3Scale(right, lx));
        if (fabsf(ly) > 0.1f) move = Vector3Subtract(move, Vector3Scale(forward, ly));
    }
    
    if (Vector3Length(move) > 0.1f) {
        move = Vector3Scale(Vector3Normalize(move), WALK_SPEED * dt);
        
        // X Collision (Check feet and head)
        player.pos.x += move.x;
        if (IsBlockSolid(player.pos.x + (move.x > 0 ? PLAYER_RADIUS : -PLAYER_RADIUS), player.pos.y, player.pos.z) ||
            IsBlockSolid(player.pos.x + (move.x > 0 ? PLAYER_RADIUS : -PLAYER_RADIUS), player.pos.y + 1.0f, player.pos.z)) {
            player.pos.x -= move.x;
        }
        
        // Z Collision
        player.pos.z += move.z;
        if (IsBlockSolid(player.pos.x, player.pos.y, player.pos.z + (move.z > 0 ? PLAYER_RADIUS : -PLAYER_RADIUS)) ||
            IsBlockSolid(player.pos.x, player.pos.y + 1.0f, player.pos.z + (move.z > 0 ? PLAYER_RADIUS : -PLAYER_RADIUS))) {
            player.pos.z -= move.z;
        }
    }

    // --- Gravity & Jumping ---
    player.vel.y += GRAVITY * dt;
    player.pos.y += player.vel.y * dt;
    
    // Hard Floor
    if (player.pos.y < 1.0f) {
        player.pos.y = 1.0f;
        player.vel.y = 0;
        player.grounded = true;
    }

    if (IsBlockSolid(player.pos.x, player.pos.y, player.pos.z)) {
        player.pos.y = floorf(player.pos.y) + 1.0f;
        player.vel.y = 0;
        player.grounded = true;
    } else {
        player.grounded = false;
    }

    if (player.grounded && (IsKeyPressed(KEY_SPACE) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))) {
        player.vel.y = JUMP_FORCE;
        player.grounded = false;
    }

    // --- Block Selection ---
    for (int i = 0; i < 9; i++) {
        if (IsKeyPressed(KEY_ONE + i)) player.selected_block = i + 1;
    }
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1)) player.selected_block--;
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) player.selected_block++;
    
    if (player.selected_block < 1) player.selected_block = BLOCK_COUNT - 1;
    if (player.selected_block >= BLOCK_COUNT) player.selected_block = 1;

    // Hunger
    player.hunger -= 0.01f * dt;
    if (player.hunger < 0) {
        player.hunger = 0;
        player.health -= 0.1f * dt;
    }
}

// --- HUD ---
void DrawHUD() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    // Crosshair
    DrawRectangle(sw / 2 - 10, sh / 2 - 1, 20, 2, WHITE);
    DrawRectangle(sw / 2 - 1, sh / 2 - 10, 2, 20, WHITE);

    // Hotbar
    int slotSize = 40;
    int barWidth = slotSize * 9;
    int startX = sw / 2 - barWidth / 2;
    int startY = sh - slotSize - 10;

    DrawRectangle(startX - 5, startY - 5, barWidth + 10, slotSize + 10, (Color){ 0, 0, 0, 150 });

    const char* slotNames[] = { "Pick", "Grass", "Dirt", "Stone", "Wood", "Leaf", "Sand", "Water", "Glow" };
    BlockType slotBlocks[] = { BLOCK_AIR, BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE, BLOCK_WOOD, BLOCK_LEAVES, BLOCK_SAND, BLOCK_WATER, BLOCK_GLOWSTONE };

    for (int i = 0; i < 9; i++) {
        int x = startX + i * slotSize;
        DrawRectangleLines(x, startY, slotSize, slotSize, (i + 1 == player.selected_block) ? YELLOW : GRAY);
        
        if (i == 0) {
            // Pickaxe icon
            DrawRectangle(x + 10, startY + 10, 20, 5, GRAY);
            DrawRectangle(x + 18, startY + 15, 4, 15, BROWN);
        } else {
            DrawRectangle(x + 5, startY + 5, slotSize - 10, slotSize - 10, GetBlockColor(slotBlocks[i], 2));
        }
        
        DrawText(slotNames[i], x + 2, startY + slotSize - 10, 10, WHITE);
    }

    // Health & Hunger
    for (int i = 0; i < 3; i++) {
        DrawText("♥", 20 + i * 25, sh - 60, 30, (i < player.health) ? RED : DARKGRAY);
    }
    
    // Health Bar
    DrawRectangle(20, sh - 75, 100, 10, DARKGRAY);
    DrawRectangle(20, sh - 75, (int)(player.health * 33.3f), 10, RED);

    DrawRectangle(20, sh - 30, 100, 10, DARKGRAY);
    DrawRectangle(20, sh - 30, (int)player.hunger * 10, 10, ORANGE);

    // Damage Flash
    if (player.damage_timer > 0) {
        DrawRectangle(0, 0, sw, sh, Fade(RED, player.damage_timer * 0.5f));
        DrawRectangleLinesEx((Rectangle){0, 0, (float)sw, (float)sh}, 20, Fade(RED, player.damage_timer));
    }
}

// --- Sound ---
typedef struct {
    Sound punch;
    Sound place;
    Sound bird;
    Sound zombie;
} GameSounds;
GameSounds sounds;

void InitSounds() {
    InitAudioDevice();
    
    // Punch: Short low-frequency thud
    Wave punchWave = { 0 };
    punchWave.frameCount = 44100 * 0.1;
    punchWave.sampleRate = 44100;
    punchWave.sampleSize = 16;
    punchWave.channels = 1;
    punchWave.data = malloc(punchWave.frameCount * sizeof(short));
    short *punchData = (short *)punchWave.data;
    for (int i = 0; i < punchWave.frameCount; i++) {
        float t = (float)i / punchWave.frameCount;
        punchData[i] = (short)(sinf(2.0f * PI * 80.0f * (1.0f - t) * i / 44100.0f) * 32767.0f * (1.0f - t));
    }
    sounds.punch = LoadSoundFromWave(punchWave);
    free(punchWave.data);

    // Place: Mid-frequency short click
    Wave placeWave = { 0 };
    placeWave.frameCount = 44100 * 0.05;
    placeWave.sampleRate = 44100;
    placeWave.sampleSize = 16;
    placeWave.channels = 1;
    placeWave.data = malloc(placeWave.frameCount * sizeof(short));
    short *placeData = (short *)placeWave.data;
    for (int i = 0; i < placeWave.frameCount; i++) {
        float t = (float)i / placeWave.frameCount;
        placeData[i] = (short)(sinf(2.0f * PI * 400.0f * i / 44100.0f) * 32767.0f * (1.0f - t));
    }
    sounds.place = LoadSoundFromWave(placeWave);
    free(placeWave.data);

    // Bird: High-pitched chirp
    Wave birdWave = { 0 };
    birdWave.frameCount = 44100 * 0.2;
    birdWave.sampleRate = 44100;
    birdWave.sampleSize = 16;
    birdWave.channels = 1;
    birdWave.data = malloc(birdWave.frameCount * sizeof(short));
    short *birdData = (short *)birdWave.data;
    for (int i = 0; i < birdWave.frameCount; i++) {
        float t = (float)i / birdWave.frameCount;
        birdData[i] = (short)(sinf(2.0f * PI * (1000.0f + 500.0f * sinf(10.0f * t)) * i / 44100.0f) * 16000.0f * (1.0f - t));
    }
    sounds.bird = LoadSoundFromWave(birdWave);
    free(birdWave.data);

    // Zombie: Low-frequency growl (noise-like)
    Wave zombieWave = { 0 };
    zombieWave.frameCount = 44100 * 0.5;
    zombieWave.sampleRate = 44100;
    zombieWave.sampleSize = 16;
    zombieWave.channels = 1;
    zombieWave.data = malloc(zombieWave.frameCount * sizeof(short));
    short *zombieData = (short *)zombieWave.data;
    for (int i = 0; i < zombieWave.frameCount; i++) {
        float t = (float)i / zombieWave.frameCount;
        float noise = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
        zombieData[i] = (short)(noise * 10000.0f * (1.0f - t) * (0.5f + 0.5f * sinf(20.0f * t)));
    }
    sounds.zombie = LoadSoundFromWave(zombieWave);
    free(zombieWave.data);
}

// --- Main ---
int main() {
    InitWindow(800, 480, "r36sCraft");
    SetTargetFPS(30);
    DisableCursor();

    bool isPaused = false;

    // Init Player
    player.pos = (Vector3){ 0, 80, 0 };
    player.health = 3.0f;
    player.hunger = 10.0f;
    player.selected_block = 1;
    player.damage_timer = 0.0f;

    LoadPlayer();
    InitSounds();

    Camera3D camera = { 0 };
    camera.up = (Vector3){ 0, 1, 0 };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    srand(time(NULL));
    world_seed = rand();

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT)) {
            isPaused = !isPaused;
            if (isPaused) EnableCursor();
            else DisableCursor();
        }

        float dt = GetFrameTime();
        
        if (!isPaused) {
            // Chunk Management
            int pcx = floorf(player.pos.x / CHUNK_X);
            int pcz = floorf(player.pos.z / CHUNK_Z);
            
            for (int i = 0; i < MAX_CHUNKS; i++) {
                int cx = pcx - RENDER_DIST + (i % WORLD_SIZE);
                int cz = pcz - RENDER_DIST + (i / WORLD_SIZE);
                
                bool found = false;
                for (int j = 0; j < MAX_CHUNKS; j++) {
                    if (chunks[j].loaded && chunks[j].x == cx && chunks[j].z == cz) {
                        found = true;
                        if (chunks[j].dirty) BuildChunkMesh(&chunks[j]);
                        break;
                    }
                }
                
                if (!found) {
                    // Find oldest/farthest chunk to reuse
                    int best_idx = -1;
                    float max_dist = -1;
                    for (int j = 0; j < MAX_CHUNKS; j++) {
                        if (!chunks[j].loaded) { best_idx = j; break; }
                        float d = Vector2Distance((Vector2){(float)chunks[j].x, (float)chunks[j].z}, (Vector2){(float)pcx, (float)pcz});
                        if (d > max_dist) { max_dist = d; best_idx = j; }
                    }
                    if (best_idx != -1) {
                        if (chunks[best_idx].loaded) SaveChunk(&chunks[best_idx]);
                        chunks[best_idx].x = cx;
                        chunks[best_idx].z = cz;
                        GenerateChunk(&chunks[best_idx]);
                        BuildChunkMesh(&chunks[best_idx]);
                    }
                }
            }

            UpdatePlayer(dt);

            // Camera update
            camera.position = (Vector3){ player.pos.x, player.pos.y + PLAYER_HEIGHT, player.pos.z };
            Vector3 forward = { cosf(player.pitch * DEG2RAD) * sinf(player.yaw * DEG2RAD),
                               sinf(player.pitch * DEG2RAD),
                               cosf(player.pitch * DEG2RAD) * cosf(player.yaw * DEG2RAD) };
            camera.target = Vector3Add(camera.position, forward);

            // Interaction
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_2)) {
                PlaySound(sounds.punch);
                Vector3 rayPos = camera.position;
                Vector3 rayDir = forward;
                
                // Punch Mobs
                bool hitMob = false;
                Ray punchRay = { rayPos, rayDir };
                for (int i = 0; i < 32; i++) {
                    if (mobs[i].active) {
                        float dist = Vector3Distance(rayPos, mobs[i].pos);
                        if (dist < 4.0f) { // Punch range
                            // Simple sphere collision for mobs
                            if (GetRayCollisionSphere(punchRay, mobs[i].pos, 0.8f).hit) {
                                mobs[i].active = false; // 1 hit kill
                                hitMob = true;
                                break;
                            }
                        }
                    }
                }

                if (!hitMob) {
                    // Break block
                    for(float d=0; d<5.0f; d+=0.1f) {
                        Vector3 p = Vector3Add(rayPos, Vector3Scale(rayDir, d));
                        int ix = floorf(p.x), iy = floorf(p.y), iz = floorf(p.z);
                        if (IsBlockSolid(ix, iy, iz)) {
                            int cx = floorf((float)ix / CHUNK_X);
                            int cz = floorf((float)iz / CHUNK_Z);
                            for(int i=0; i<MAX_CHUNKS; i++) {
                                if (chunks[i].loaded && chunks[i].x == cx && chunks[i].z == cz) {
                                    chunks[i].blocks[(ix - cx*CHUNK_X) + (iz - cz*CHUNK_Z)*CHUNK_X + iy*CHUNK_X*CHUNK_Z].type = BLOCK_AIR;
                                    chunks[i].dirty = true;
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
            }
            
            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_TRIGGER_2)) {
                if (player.selected_block > 1) { // Slot 1 is Pickaxe
                    PlaySound(sounds.place);
                    Vector3 rayPos = camera.position;
                    Vector3 rayDir = forward;
                    Vector3 prevP = rayPos;
                    for(float d=0; d<5.0f; d+=0.1f) {
                        Vector3 p = Vector3Add(rayPos, Vector3Scale(rayDir, d));
                        int ix = floorf(p.x), iy = floorf(p.y), iz = floorf(p.z);
                        if (IsBlockSolid(ix, iy, iz)) {
                            int px = floorf(prevP.x), py = floorf(prevP.y), pz = floorf(prevP.z);
                            int cx = floorf((float)px / CHUNK_X);
                            int cz = floorf((float)pz / CHUNK_Z);
                            for(int i=0; i<MAX_CHUNKS; i++) {
                                if (chunks[i].loaded && chunks[i].x == cx && chunks[i].z == cz) {
                                    chunks[i].blocks[(px - cx*CHUNK_X) + (pz - cz*CHUNK_Z)*CHUNK_X + py*CHUNK_X*CHUNK_Z].type = player.selected_block;
                                    chunks[i].dirty = true;
                                    break;
                                }
                            }
                            break;
                        }
                        prevP = p;
                    }
                }
            }

            // Mob Update
            for (int i = 0; i < 32; i++) {
                if (!mobs[i].active) {
                    if (GetRandomValue(0, 1000) < 5) {
                        mobs[i].active = true;
                        // Reduce zombie rate to 10% (1/10 chance to be zombie, 9/10 chicken)
                        mobs[i].type = (GetRandomValue(0, 9) == 0) ? MOB_ZOMBIE : MOB_CHICKEN;
                        mobs[i].pos = (Vector3){ player.pos.x + GetRandomValue(-20, 20), 100, player.pos.z + GetRandomValue(-20, 20) };
                        mobs[i].health = (mobs[i].type == MOB_CHICKEN) ? 3 : 4;
                    }
                    continue;
                }
                mobs[i].vel.y += GRAVITY * dt;
                mobs[i].pos.y += mobs[i].vel.y * dt;
                if (IsBlockSolid(mobs[i].pos.x, mobs[i].pos.y, mobs[i].pos.z)) {
                    mobs[i].pos.y = floorf(mobs[i].pos.y) + 1.0f;
                    mobs[i].vel.y = 0;
                }
                if (mobs[i].type == MOB_ZOMBIE) {
                    Vector3 toPlayer = Vector3Subtract(player.pos, mobs[i].pos);
                    float dist = Vector3Length(toPlayer);
                    if (dist < 20.0f) {
                        toPlayer.y = 0;
                        mobs[i].pos = Vector3Add(mobs[i].pos, Vector3Scale(Vector3Normalize(toPlayer), 2.0f * dt));
                        
                        // Damage player on contact
                        if (dist < 1.2f && player.damage_timer <= 0) {
                            player.health -= 1.0f;
                            player.damage_timer = 0.5f;
                            PlaySound(sounds.punch); // Reuse punch sound for taking damage
                        }
                        // Zombie growl when near
                        if (dist < 10.0f && GetRandomValue(0, 100) == 0) {
                            PlaySound(sounds.zombie);
                        }
                    }
                } else {
                    mobs[i].pos.x += sinf(GetTime() + i) * dt;
                    mobs[i].pos.z += cosf(GetTime() + i) * dt;
                }
                if (Vector3Distance(player.pos, mobs[i].pos) > 50.0f) mobs[i].active = false;
            }

            if (player.damage_timer > 0) player.damage_timer -= dt;

            // Ambient birds
            if (GetRandomValue(0, 500) == 0) {
                PlaySound(sounds.bird);
            }
        }

        BeginDrawing();
            ClearBackground(SKYBLUE);
            BeginMode3D(camera);
                for (int i = 0; i < MAX_CHUNKS; i++) {
                    if (chunks[i].loaded && chunks[i].has_mesh) {
                        DrawModel(chunks[i].model, (Vector3){ (float)chunks[i].x * CHUNK_X, 0, (float)chunks[i].z * CHUNK_Z }, 1.0f, WHITE);
                    }
                }
                
                // Selection Box
                if (!isPaused) {
                    Vector3 forward = { cosf(player.pitch * DEG2RAD) * sinf(player.yaw * DEG2RAD),
                                       sinf(player.pitch * DEG2RAD),
                                       cosf(player.pitch * DEG2RAD) * cosf(player.yaw * DEG2RAD) };
                    for(float d=0; d<5.0f; d+=0.1f) {
                        Vector3 p = Vector3Add(camera.position, Vector3Scale(forward, d));
                        if (IsBlockSolid(floorf(p.x), floorf(p.y), floorf(p.z))) {
                            DrawCubeWires((Vector3){floorf(p.x) + 0.5f, floorf(p.y) + 0.5f, floorf(p.z) + 0.5f}, 1.01f, 1.01f, 1.01f, BLACK);
                            break;
                        }
                    }
                }

                // Draw Mobs
                for (int i = 0; i < 32; i++) {
                    if (!mobs[i].active) continue;
                    if (mobs[i].type == MOB_CHICKEN) {
                        DrawCube(mobs[i].pos, 0.4f, 0.4f, 0.6f, WHITE);
                        DrawCube((Vector3){mobs[i].pos.x, mobs[i].pos.y + 0.3f, mobs[i].pos.z + 0.2f}, 0.2f, 0.3f, 0.2f, WHITE);
                        DrawCube((Vector3){mobs[i].pos.x, mobs[i].pos.y + 0.3f, mobs[i].pos.z + 0.35f}, 0.1f, 0.05f, 0.1f, ORANGE);
                    } else {
                        DrawCube(mobs[i].pos, 0.6f, 0.8f, 0.3f, DARKGREEN);
                        DrawCube((Vector3){mobs[i].pos.x, mobs[i].pos.y + 0.7f, mobs[i].pos.z}, 0.4f, 0.4f, 0.4f, GREEN);
                        DrawCube((Vector3){mobs[i].pos.x, mobs[i].pos.y + 0.2f, mobs[i].pos.z + 0.4f}, 0.2f, 0.2f, 0.6f, GREEN);
                    }
                }
            EndMode3D();

            DrawHUD();
            DrawFPS(10, 10);

            if (isPaused) {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 0, 0, 0, 100 });
                DrawText("PAUSED", GetScreenWidth() / 2 - 50, GetScreenHeight() / 2 - 40, 30, WHITE);
                
                if (GuiButton((Rectangle){ (float)GetScreenWidth() / 2 - 60, (float)GetScreenHeight() / 2, 120, 30 }, "RESUME")) {
                    isPaused = false;
                    DisableCursor();
                }
                
                if (GuiButton((Rectangle){ (float)GetScreenWidth() / 2 - 60, (float)GetScreenHeight() / 2 + 40, 120, 30 }, "EXIT")) {
                    break;
                }
            }
        EndDrawing();
    }

    // Save on exit
    SavePlayer();
    for (int i = 0; i < MAX_CHUNKS; i++) {
        if (chunks[i].loaded) SaveChunk(&chunks[i]);
    }

    CloseWindow();
    return 0;
}
