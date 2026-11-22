// World: 16x16 chunk columns, deterministic terrain + trees.
#pragma once
#include <cstdint>
#include <vector>

#include "chunk.h"

class World {
public:
    static constexpr int CX = 16;
    static constexpr int CZ = 16;
    static constexpr int SEA_LEVEL = 24;

    void generate(uint32_t seed);

    // Builds all chunk meshes in parallel (2 workers on a 2-core CPU).
    void buildMeshes(std::vector<ChunkMesh>& meshes) const;

    Block getBlock(int wx, int wy, int wz) const;
    bool setBlock(int wx, int wy, int wz, Block b);
    int heightAt(int wx, int wz) const;

    Chunk& chunk(int cx, int cz) { return chunks_[static_cast<size_t>(cz) * CX + cx]; }
    const Chunk& chunk(int cx, int cz) const { return chunks_[static_cast<size_t>(cz) * CX + cx]; }

    static bool inBounds(int cx, int cz) { return cx >= 0 && cx < CX && cz >= 0 && cz < CZ; }
    static int worldSizeX() { return CX * Chunk::SX; }
    static int worldSizeZ() { return CZ * Chunk::SZ; }

private:
    std::vector<Chunk> chunks_ = std::vector<Chunk>(CX * CZ);
    uint32_t seed_ = 0;
};
