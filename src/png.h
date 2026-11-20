// CraftEngine - minimal PNG decoder (8-bit, non-interlaced, RGB/RGBA/gray) -> RGBA8.
#pragma once
#include <cstdint>
#include <vector>

struct PngInfo {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba; // width*height*4
};

// Returns false and clears info on unsupported/corrupt input.
bool DecodePng(const uint8_t* data, size_t size, PngInfo& info);
