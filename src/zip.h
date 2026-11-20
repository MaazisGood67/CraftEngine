// CraftEngine - minimal in-memory ZIP reader (central directory + local headers).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct ZipEntry {
    std::string name;              // forward-slash separated, e.g. assets/minecraft/textures/block/stone.png
    std::vector<uint8_t> data;    // decompressed payload
};

// Loads all entries of a zip file. Returns false on any structural error.
bool LoadZip(const std::string& path, std::vector<ZipEntry>& out, std::string& err);
