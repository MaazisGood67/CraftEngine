// CraftEngine - procedural tile fallback + jar texture loading.
#pragma once
#include <string>
#include <vector>

#include "blocks.h"
#include "png.h"

struct LoadedTexture {
    std::string path; // e.g. textures/block/stone.png
    int tile = -1;    // resolved Tile id
    PngInfo png;
};

// Generates a procedural 16x16 tile into dst (16*16*4 bytes RGBA).
void GenerateProceduralTile(int tile, uint8_t* dst);

// Loads every compressed image entry under textures/** from the jar.
bool LoadTexturesFromJar(const std::string& jarPath, std::vector<LoadedTexture>& out, std::string& err);

// Resolves which Tile a jar texture path belongs to.
int TileForPath(const std::string& path);

// Finds the best matching loaded texture for a set of candidate substrings.
int FindTile(const std::vector<LoadedTexture>& loaded, const std::vector<const char*>& candidates);

// Nearest-neighbour scales src (sw x sh) RGBA into dst (16x16).
void ScaleTo16(const uint8_t* src, int sw, int sh, uint8_t* dst);
