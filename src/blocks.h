// Block registry + texture-array tile ids.
#pragma once
#include <cstdint>

enum Block : uint8_t {
    AIR = 0,
    STONE,
    DIRT,
    GRASS,
    SAND,
    WATER,
    LOG,
    LEAVES,
    PLANKS,
    COBBLE,
    BEDROCK,
    BLOCK_COUNT
};

// Texture array slices (16x16, generated procedurally in renderer.cpp).
enum Tile : uint8_t {
    TILE_GRASS_TOP = 0,
    TILE_GRASS_SIDE,
    TILE_DIRT,
    TILE_STONE,
    TILE_SAND,
    TILE_WATER,
    TILE_LOG_SIDE,
    TILE_LOG_TOP,
    TILE_LEAVES,
    TILE_PLANKS,
    TILE_COBBLE,
    TILE_BEDROCK,
    TILE_COUNT
};

// Face indices: 0=+X 1=-X 2=+Y 3=-Y 4=+Z 5=-Z
inline bool isOpaque(Block b) {
    switch (b) {
        case AIR:
        case WATER:
            return false;
        default:
            return true;
    }
}

inline bool isRenderable(Block b) { return b != AIR; }

inline Tile tileFor(Block b, int face) {
    switch (b) {
        case GRASS:  return (face == 2) ? TILE_GRASS_TOP : (face == 3 ? TILE_DIRT : TILE_GRASS_SIDE);
        case DIRT:   return TILE_DIRT;
        case STONE:  return TILE_STONE;
        case SAND:   return TILE_SAND;
        case WATER:  return TILE_WATER;
        case LOG:    return (face == 2 || face == 3) ? TILE_LOG_TOP : TILE_LOG_SIDE;
        case LEAVES: return TILE_LEAVES;
        case PLANKS: return TILE_PLANKS;
        case COBBLE: return TILE_COBBLE;
        case BEDROCK:return TILE_BEDROCK;
        default:     return TILE_STONE;
    }
}

// Minecraft-style per-face brightness.
inline float faceBrightness(int face) {
    switch (face) {
        case 2:  return 1.00f; // +Y
        case 3:  return 0.50f; // -Y
        case 0:
        case 1:  return 0.72f; // +/-X
        default: return 0.86f; // +/-Z
    }
}
