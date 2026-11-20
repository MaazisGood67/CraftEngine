#include "chunk.h"

#include <algorithm>
#include <cstring>

namespace {

struct FaceMask {
    uint8_t solid = 0;
    uint8_t tile = 0;
    uint8_t face = 0;
    uint8_t ao[4] = {0, 0, 0, 0};

    bool operator==(const FaceMask& o) const {
        return solid == o.solid && tile == o.tile && face == o.face &&
               ao[0] == o.ao[0] && ao[1] == o.ao[1] &&
               ao[2] == o.ao[2] && ao[3] == o.ao[3];
    }
};

constexpr float kAoFactor[4] = {0.45f, 0.66f, 0.84f, 1.00f};
constexpr int kAxis[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

} // namespace

void Chunk::buildMesh(ChunkMesh& out, int ox, int oy, int oz) const {
    out.vertices.clear();
    out.indices.clear();
    out.quads = 0;

    const int dims[3] = {SX, SY, SZ};

    // Reserve roughly one face per surface block; avoids realloc churn.
    out.vertices.reserve(8192 * 4);
    out.indices.reserve(8192 * 6);

    std::vector<FaceMask> mask(static_cast<size_t>(SY) * SZ);

    auto solidAt = [&](const int p[3]) { return isOpaque(get(p[0], p[1], p[2])); };

    for (int d = 0; d < 3; ++d) {
        const int u = (d + 1) % 3;
        const int v = (d + 2) % 3;
        const int du = dims[u];
        const int dv = dims[v];
        const int dn = dims[d];
        mask.assign(static_cast<size_t>(du) * dv, FaceMask{});

        int x[3] = {0, 0, 0};
        int q[3] = {0, 0, 0};
        q[d] = 1;

        for (x[d] = -1; x[d] < dn;) {
            // ---- build the slice mask ----
            int n = 0;
            for (x[v] = 0; x[v] < dv; ++x[v]) {
                for (x[u] = 0; x[u] < du; ++x[u], ++n) {
                    const Block a = (x[d] >= 0) ? get(x[0], x[1], x[2]) : AIR;
                    const Block b = (x[d] < dn - 1) ? get(x[0] + q[0], x[1] + q[1], x[2] + q[2]) : AIR;

                    FaceMask f;
                    const bool aOpaque = isOpaque(a);
                    const bool bOpaque = isOpaque(b);

                    if (a == b && !isOpaque(a)) {
                        mask[n] = f; // same transparent material on both sides: no face
                        continue;
                    }

                    if (isRenderable(a) && !bOpaque) {
                        // +d face of block a
                        const int face = d * 2;
                        f.solid = 1;
                        f.face = static_cast<uint8_t>(face);
                        f.tile = tileFor(a, face);

                        int p[3] = {x[0], x[1], x[2]};
                        p[d] += 1;
                        for (int c = 0; c < 4; ++c) {
                            const int su = (c == 1 || c == 2) ? 1 : -1;
                            const int sv = (c >= 2) ? 1 : -1;

                            int na[3] = {p[0], p[1], p[2]};
                            int nb[3] = {p[0], p[1], p[2]};
                            int nc[3] = {p[0], p[1], p[2]};
                            na[u] += su;
                            nb[v] += sv;
                            nc[u] += su;
                            nc[v] += sv;

                            const bool s1 = solidAt(na);
                            const bool s2 = solidAt(nb);
                            const bool cr = solidAt(nc);
                            const int level = (s1 && s2) ? 0 : 3 - (int(s1) + int(s2) + int(cr));
                            f.ao[c] = static_cast<uint8_t>(level);
                        }
                    } else if (isRenderable(b) && !aOpaque) {
                        // -d face of block b (one step further along d)
                        const int face = d * 2 + 1;
                        f.solid = 1;
                        f.face = static_cast<uint8_t>(face);
                        f.tile = tileFor(b, face);

                        int p[3] = {x[0], x[1], x[2]}; // outside cell (block b is at +q)
                        for (int c = 0; c < 4; ++c) {
                            const int su = (c == 1 || c == 2) ? 1 : -1;
                            const int sv = (c >= 2) ? 1 : -1;

                            int na[3] = {p[0], p[1], p[2]};
                            int nb[3] = {p[0], p[1], p[2]};
                            int nc[3] = {p[0], p[1], p[2]};
                            na[u] += su;
                            nb[v] += sv;
                            nc[u] += su;
                            nc[v] += sv;

                            const bool s1 = solidAt(na);
                            const bool s2 = solidAt(nb);
                            const bool cr = solidAt(nc);
                            const int level = (s1 && s2) ? 0 : 3 - (int(s1) + int(s2) + int(cr));
                            f.ao[c] = static_cast<uint8_t>(level);
                        }
                    }

                    mask[n] = f;
                }
            }

            ++x[d];

            // ---- greedy merge the mask into quads ----
            n = 0;
            for (int j = 0; j < dv; ++j) {
                for (int i = 0; i < du;) {
                    const FaceMask f = mask[n];
                    if (!f.solid) {
                        ++i;
                        ++n;
                        continue;
                    }

                    int w = 1;
                    while (i + w < du && mask[n + w] == f) ++w;

                    int h = 1;
                    bool stop = false;
                    while (j + h < dv && !stop) {
                        for (int k = 0; k < w; ++k) {
                            if (!(mask[n + k + h * du] == f)) {
                                stop = true;
                                break;
                            }
                        }
                        if (!stop) ++h;
                    }

                    // emit quad
                    const int base[3] = {x[0], x[1], x[2]};
                    int p0[3] = {0, 0, 0};
                    p0[d] = base[d];
                    p0[u] = base[u] + i;
                    p0[v] = base[v] + j;

                    const float bright = faceBrightness(f.face);
                    const uint32_t startVertex = static_cast<uint32_t>(out.vertices.size());

                    const int duVec[3] = {kAxis[u][0] * w, kAxis[u][1] * w, kAxis[u][2] * w};
                    const int dvVec[3] = {kAxis[v][0] * h, kAxis[v][1] * h, kAxis[v][2] * h};

                    const float corner[4][3] = {
                        {static_cast<float>(p0[0]), static_cast<float>(p0[1]), static_cast<float>(p0[2])},
                        {static_cast<float>(p0[0] + duVec[0]), static_cast<float>(p0[1] + duVec[1]), static_cast<float>(p0[2] + duVec[2])},
                        {static_cast<float>(p0[0] + duVec[0] + dvVec[0]), static_cast<float>(p0[1] + duVec[1] + dvVec[1]), static_cast<float>(p0[2] + duVec[2] + dvVec[2])},
                        {static_cast<float>(p0[0] + dvVec[0]), static_cast<float>(p0[1] + dvVec[1]), static_cast<float>(p0[2] + dvVec[2])}};
                    const float cornerUV[4][2] = {
                        {0.0f, 0.0f},
                        {static_cast<float>(w), 0.0f},
                        {static_cast<float>(w), static_cast<float>(h)},
                        {0.0f, static_cast<float>(h)}};

                    for (int c = 0; c < 4; ++c) {
                        Vertex vert;
                        vert.x = corner[c][0] + static_cast<float>(ox);
                        vert.y = corner[c][1] + static_cast<float>(oy);
                        vert.z = corner[c][2] + static_cast<float>(oz);
                        vert.u = cornerUV[c][0];
                        vert.v = cornerUV[c][1];
                        vert.tile = static_cast<float>(f.tile);
                        vert.light = bright * kAoFactor[f.ao[c]];
                        out.vertices.push_back(vert);
                    }

                    out.indices.push_back(startVertex + 0);
                    out.indices.push_back(startVertex + 1);
                    out.indices.push_back(startVertex + 2);
                    out.indices.push_back(startVertex + 0);
                    out.indices.push_back(startVertex + 2);
                    out.indices.push_back(startVertex + 3);
                    ++out.quads;

                    for (int l = 0; l < h; ++l)
                        for (int k = 0; k < w; ++k) mask[n + k + l * du].solid = 0;

                    i += w;
                    n += w;
                }
            }
        }
    }
}
