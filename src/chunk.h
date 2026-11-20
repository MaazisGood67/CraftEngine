// Greedy-meshed 16 x 128 x 16 chunk column.
#pragma once
#include <cstdint>
#include <vector>

#include "blocks.h"

struct Vertex {
    float x, y, z;
    float u, v;
    float tile;
    float light;
};

struct ChunkMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t quads = 0;
};

class Chunk {
public:
    static constexpr int SX = 16;
    static constexpr int SY = 128;
    static constexpr int SZ = 16;

    Chunk() : blocks_(SX * SY * SZ, AIR) {}

    Block get(int x, int y, int z) const {
        if (x < 0 || x >= SX || y < 0 || y >= SY || z < 0 || z >= SZ) return AIR;
        return static_cast<Block>(blocks_[index(x, y, z)]);
    }
    void set(int x, int y, int z, Block b) {
        if (x < 0 || x >= SX || y < 0 || y >= SY || z < 0 || z >= SZ) return;
        blocks_[index(x, y, z)] = b;
    }

    // Builds a greedy mesh in world space (origin = chunk world position).
    void buildMesh(ChunkMesh& out, int ox, int oy, int oz) const;

    static int index(int x, int y, int z) { return (y * SZ + z) * SX + x; }

private:
    std::vector<uint8_t> blocks_;
};
