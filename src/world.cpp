#include "world.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <thread>

namespace {

inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

inline float rand01(int x, int z, uint32_t seed) {
    const uint32_t h = hash32(static_cast<uint32_t>(x) * 374761393u ^
                              static_cast<uint32_t>(z) * 668265263u ^ seed);
    return static_cast<float>(h >> 8) * (1.0f / 16777216.0f);
}

inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

float valueNoise(float x, float z, uint32_t seed) {
    const int xi = static_cast<int>(std::floor(x));
    const int zi = static_cast<int>(std::floor(z));
    const float fx = x - static_cast<float>(xi);
    const float fz = z - static_cast<float>(zi);
    const float u = fx * fx * (3.0f - 2.0f * fx);
    const float v = fz * fz * (3.0f - 2.0f * fz);

    const float a = rand01(xi, zi, seed);
    const float b = rand01(xi + 1, zi, seed);
    const float c = rand01(xi, zi + 1, seed);
    const float d = rand01(xi + 1, zi + 1, seed);

    return lerpf(lerpf(a, b, u), lerpf(c, d, u), v);
}

} // namespace

int World::heightAt(int wx, int wz) const {
    const float wxF = static_cast<float>(wx);
    const float wzF = static_cast<float>(wz);

    const float continents = valueNoise(wxF * 0.0125f, wzF * 0.0125f, seed_);
    const float hills = valueNoise(wxF * 0.05f, wzF * 0.05f, seed_ ^ 0x9e3779b9u);
    const float detail = valueNoise(wxF * 0.16f, wzF * 0.16f, seed_ ^ 0x85ebca6bu);

    float h = 10.0f + continents * 30.0f + hills * 9.0f + detail * 2.0f;
    int height = static_cast<int>(h);
    return std::clamp(height, 3, Chunk::SY - 20);
}

void World::generate(uint32_t seed) {
    seed_ = seed;

    for (int cz = 0; cz < CZ; ++cz) {
        for (int cx = 0; cx < CX; ++cx) {
            Chunk& c = chunk(cx, cz);
            const int ox = cx * Chunk::SX;
            const int oz = cz * Chunk::SZ;

            for (int x = 0; x < Chunk::SX; ++x) {
                for (int z = 0; z < Chunk::SZ; ++z) {
                    const int wx = ox + x;
                    const int wz = oz + z;
                    const int h = heightAt(wx, wz);
                    const bool beach = h <= SEA_LEVEL + 1;

                    for (int y = 0; y <= Chunk::SY - 1; ++y) {
                        Block b = AIR;
                        if (y == 0) {
                            b = BEDROCK;
                        } else if (y < h - 3) {
                            b = STONE;
                        } else if (y < h) {
                            b = beach ? SAND : DIRT;
                        } else if (y == h) {
                            b = beach ? SAND : GRASS;
                        } else if (y <= SEA_LEVEL) {
                            b = WATER;
                        }
                        c.set(x, y, z, b);
                    }
                }
            }

            // Trees, kept 3 blocks inside the chunk so they never cross chunk borders.
            for (int x = 3; x < Chunk::SX - 3; ++x) {
                for (int z = 3; z < Chunk::SZ - 3; ++z) {
                    const int wx = ox + x;
                    const int wz = oz + z;
                    if (rand01(wx, wz, seed_ ^ 0x1234abcdu) > 0.010f) continue;

                    const int h = heightAt(wx, wz);
                    if (h <= SEA_LEVEL + 1 || h + 8 >= Chunk::SY) continue;
                    if (c.get(x, h, z) != GRASS) continue;

                    const int trunk = 4 + static_cast<int>(rand01(wx, wz, seed_ ^ 0x77u) * 3.0f);
                    for (int i = 1; i <= trunk; ++i) c.set(x, h + i, z, LOG);

                    const int top = h + trunk;
                    for (int dy = -2; dy <= 1; ++dy) {
                        const int radius = (dy <= -1) ? 2 : 1;
                        for (int dx = -radius; dx <= radius; ++dx) {
                            for (int dz = -radius; dz <= radius; ++dz) {
                                if (dx == 0 && dz == 0 && dy < 0) continue;
                                if (std::abs(dx) == radius && std::abs(dz) == radius &&
                                    rand01(wx + dx * 31, wz + dz * 17, seed_ ^ 0x55u) < 0.5f)
                                    continue;
                                const int ly = top + dy;
                                if (ly >= 0 && ly < Chunk::SY && c.get(x + dx, ly, z + dz) == AIR)
                                    c.set(x + dx, ly, z + dz, LEAVES);
                            }
                        }
                    }
                }
            }
        }
    }
}

Block World::getBlock(int wx, int wy, int wz) const {
    if (wy < 0 || wy >= Chunk::SY) return AIR;
    const int cx = wx >> 4;
    const int cz = wz >> 4;
    if (!inBounds(cx, cz)) return AIR;
    return chunk(cx, cz).get(wx & 15, wy, wz & 15);
}

bool World::setBlock(int wx, int wy, int wz, Block b) {
    if (wy < 0 || wy >= Chunk::SY) return false;
    const int cx = wx >> 4;
    const int cz = wz >> 4;
    if (!inBounds(cx, cz)) return false;
    chunk(cx, cz).set(wx & 15, wy, wz & 15, b);
    return true;
}

void World::buildMeshes(std::vector<ChunkMesh>& meshes) const {
    meshes.resize(static_cast<size_t>(CX) * CZ);

    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 2;
    const unsigned workers = std::clamp(hw / 2u, 1u, 4u);

    std::atomic<int> next{0};
    const int total = CX * CZ;

    auto worker = [&]() {
        for (;;) {
            const int i = next.fetch_add(1, std::memory_order_relaxed);
            if (i >= total) return;
            const int cx = i % CX;
            const int cz = i / CX;
            chunks_[static_cast<size_t>(i)].buildMesh(meshes[static_cast<size_t>(i)],
                                                      cx * Chunk::SX, 0, cz * Chunk::SZ);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(workers);
    for (unsigned t = 0; t < workers; ++t) threads.emplace_back(worker);
    for (auto& th : threads) th.join();
}
