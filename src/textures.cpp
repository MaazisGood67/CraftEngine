// CraftEngine - procedural tile fallback + jar texture loading.
#include "textures.h"

#include "zip.h"

#include <zlib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

struct Color {
    float r, g, b;
};

inline uint32_t hashp(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

inline float rnd(int x, int y, int tile) {
    return static_cast<float>(hashp(static_cast<uint32_t>(x) * 73856093u ^
                                    static_cast<uint32_t>(y) * 19349663u ^
                                    static_cast<uint32_t>(tile) * 83492791u) & 0xFFFFu) /
           65535.0f;
}

inline uint8_t toByte(float v) {
    return static_cast<uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
}

void shade(Color& c, float f) { c.r *= f; c.g *= f; c.b *= f; }

void putPixel(uint8_t* dst, int x, int y, const Color& c) {
    const int o = (y * 16 + x) * 4;
    dst[o + 0] = toByte(c.r);
    dst[o + 1] = toByte(c.g);
    dst[o + 2] = toByte(c.b);
    dst[o + 3] = 255;
}

void GenerateTilePixels(uint8_t* dst, int tile) {
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            const float n = rnd(x, y, tile);
            Color c{0.5f, 0.5f, 0.5f};
            switch (tile) {
                case TILE_GRASS_TOP: { c = {0.34f, 0.60f, 0.22f}; shade(c, 0.82f + n * 0.34f); break; }
                case TILE_GRASS_SIDE: {
                    if (y < 3) { c = {0.32f, 0.58f, 0.21f}; shade(c, 0.85f + n * 0.3f); }
                    else { c = {0.47f, 0.33f, 0.22f}; shade(c, 0.82f + n * 0.34f); }
                    break;
                }
                case TILE_DIRT: { c = {0.47f, 0.33f, 0.22f}; shade(c, 0.82f + n * 0.34f); break; }
                case TILE_STONE: { c = {0.53f, 0.53f, 0.55f}; shade(c, 0.78f + n * 0.38f); break; }
                case TILE_SAND: { c = {0.85f, 0.81f, 0.60f}; shade(c, 0.88f + n * 0.22f); break; }
                case TILE_WATER: { c = {0.18f, 0.38f, 0.80f}; shade(c, 0.85f + n * 0.25f); break; }
                case TILE_LOG_SIDE: {
                    const float streak = 0.85f + rnd(x, 0, 99) * 0.3f;
                    c = {0.42f, 0.30f, 0.17f};
                    shade(c, streak * (0.9f + n * 0.15f));
                    break;
                }
                case TILE_LOG_TOP: {
                    const float dx = static_cast<float>(x) - 7.5f;
                    const float dy = static_cast<float>(y) - 7.5f;
                    const float d = std::sqrt(dx * dx + dy * dy);
                    const bool ring = (static_cast<int>(d) % 2) == 0;
                    c = ring ? Color{0.50f, 0.37f, 0.22f} : Color{0.40f, 0.28f, 0.16f};
                    shade(c, 0.9f + n * 0.15f);
                    break;
                }
                case TILE_LEAVES: { c = {0.20f, 0.46f, 0.16f}; shade(c, 0.70f + n * 0.55f); break; }
                case TILE_PLANKS: {
                    c = {0.66f, 0.50f, 0.30f};
                    const float line = (y % 5 == 0) ? 0.72f : 1.0f;
                    shade(c, line * (0.9f + n * 0.18f));
                    break;
                }
                case TILE_COBBLE: {
                    const int blob = static_cast<int>(rnd(x / 4, y / 4, 7) * 3.0f);
                    c = {0.45f, 0.45f, 0.47f};
                    shade(c, 0.70f + blob * 0.12f + n * 0.12f);
                    break;
                }
                case TILE_BEDROCK: { c = {0.26f, 0.26f, 0.29f}; shade(c, 0.6f + n * 0.7f); break; }
                default: break;
            }
            putPixel(dst, x, y, c);
        }
    }
}

} // namespace

void GenerateProceduralTile(int tile, uint8_t* dst) {
    GenerateTilePixels(dst, tile);
}

int TileForPath(const std::string& path) {
    std::string p = path;
    for (char& c : p) {
        if (c == '\\') c = '/';
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    struct Cand { const char* key; int tile; };
    static const Cand map[] = {
        {"grass_block_top", TILE_GRASS_TOP}, {"grass_top", TILE_GRASS_TOP},
        {"grass_block_side", TILE_GRASS_SIDE}, {"grass_side", TILE_GRASS_SIDE},
        {"dirt", TILE_DIRT},
        {"stone", TILE_STONE},
        {"sand", TILE_SAND},
        {"water", TILE_WATER},
        {"log", TILE_LOG_SIDE},
        {"logs", TILE_LOG_SIDE},
        {"log_top", TILE_LOG_TOP},
        {"leaves", TILE_LEAVES},
        {"plank", TILE_PLANKS},
        {"cobble", TILE_COBBLE},
        {"bedrock", TILE_BEDROCK},
    };
    for (const Cand& e : map) {
        if (p.find(e.key) != std::string::npos) return e.tile;
    }
    return -1;
}

bool LoadTexturesFromJar(const std::string& jarPath, std::vector<LoadedTexture>& out, std::string& err) {
    out.clear();
    err.clear();

    std::vector<ZipEntry> entries;
    if (!LoadZip(jarPath, entries, err)) return false;

    size_t texHit = 0, magicBad = 0, pathBad = 0, decBad = 0, tileMiss = 0, okCount = 0;
    for (const ZipEntry& e : entries) {
        if (e.data.size() < 8 || e.data[0] != 0x89 || e.data[1] != 'P') { ++magicBad; continue; }
        std::string l = e.name;
        for (char& c : l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (l.find("textures/") == std::string::npos) { ++pathBad; continue; }
        if (l.find(".png") == std::string::npos) { ++pathBad; continue; }
        ++texHit;

        LoadedTexture lt;
        lt.path = e.name;
        if (!DecodePng(e.data.data(), e.data.size(), lt.png)) { ++decBad; continue; }
        lt.tile = TileForPath(e.name);
        if (lt.tile < 0) { ++tileMiss; continue; }
        ++okCount;
        out.push_back(std::move(lt));
    }
    {
        FILE* dbg = fopen("C:\\Users\\Administrator\\Desktop\\REA Minecraaft C++\\tex_debug.txt", "w");
        if (dbg) { fprintf(dbg, "entries=%zu texHit=%zu magicBad=%zu pathBad=%zu decBad=%zu tileMiss=%zu ok=%zu\n", entries.size(), texHit, magicBad, pathBad, decBad, tileMiss, okCount); fclose(dbg); }
    }
    if (err.empty()) err = "zipOk";
    return true;
}

int FindTile(const std::vector<LoadedTexture>& loaded, const std::vector<const char*>& candidates) {
    int bestTile = -1;
    int bestScore = -1;
    for (const LoadedTexture& lt : loaded) {
        if (lt.tile < 0) continue;
        int score = 0;
        for (const char* c : candidates) {
            if (lt.path.find(c) != std::string::npos) ++score;
        }
        if (score > bestScore) { bestScore = score; bestTile = lt.tile; }
    }
    return bestTile;
}

void ScaleTo16(const uint8_t* src, int sw, int sh, uint8_t* dst) {
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            const int sx = static_cast<int>(std::round(x * (sw - 1) / 15.0));
            const int sy = static_cast<int>(std::round(y * (sh - 1) / 15.0));
            const int si = (sy * sw + sx) * 4;
            const int di = (y * 16 + x) * 4;
            dst[di + 0] = src[si + 0];
            dst[di + 1] = src[si + 1];
            dst[di + 2] = src[si + 2];
            dst[di + 3] = src[si + 3];
        }
    }
}
