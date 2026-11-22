#include "player.h"

#include <algorithm>
#include <cmath>

Vec3 Player::forward() const {
    return {std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
}

namespace {

bool solidAt(const World& world, int x, int y, int z) {
    return isOpaque(world.getBlock(x, y, z));
}

bool collides(const World& world, const Vec3& p) {
    const float minX = p.x - Player::kHalfW;
    const float maxX = p.x + Player::kHalfW;
    const float minY = p.y;
    const float maxY = p.y + Player::kHeight;
    const float minZ = p.z - Player::kHalfW;
    const float maxZ = p.z + Player::kHalfW;

    if (minX < 0.0f || maxX > static_cast<float>(World::worldSizeX())) return true;
    if (minZ < 0.0f || maxZ > static_cast<float>(World::worldSizeZ())) return true;
    if (minY < 0.0f) return true;

    const int x0 = static_cast<int>(std::floor(minX));
    const int x1 = static_cast<int>(std::floor(maxX - 1e-4f));
    const int y0 = static_cast<int>(std::floor(minY));
    const int y1 = static_cast<int>(std::floor(maxY - 1e-4f));
    const int z0 = static_cast<int>(std::floor(minZ));
    const int z1 = static_cast<int>(std::floor(maxZ - 1e-4f));

    for (int y = y0; y <= y1; ++y) {
        for (int z = z0; z <= z1; ++z) {
            for (int x = x0; x <= x1; ++x) {
                if (solidAt(world, x, y, z)) return true;
            }
        }
    }
    return false;
}

} // namespace

void tickPlayer(Player& p, World& world, const PlayerInput& in) {
    p.prevPos = p.pos;
    const float sprintMul = in.sprint ? 1.3f : 1.0f;

    Vec3 wish{};
    {
        const Vec3 fwd{std::sin(p.yaw), 0.0f, std::cos(p.yaw)};
        const Vec3 right{std::cos(p.yaw), 0.0f, -std::sin(p.yaw)};
        wish = fwd * in.forward + right * in.strafe;
        const float len = length(wish);
        if (len > 1.0f) wish = wish * (1.0f / len);
    }

    if (p.flying) {
        const float accel = 0.05f * (in.sprint ? 2.0f : 1.0f);
        p.vel.x += wish.x * accel;
        p.vel.z += wish.z * accel;
        p.vel.y += in.jump ? 0.15f : (in.sneak ? -0.15f : 0.0f);
        p.vel.x *= 0.91f;
        p.vel.z *= 0.91f;
        p.vel.y *= 0.75f;
    } else {
        float accel = p.onGround ? 0.1f : 0.02f;
        accel *= sprintMul;
        if (in.sneak) accel *= 0.3f;
        p.vel.x += wish.x * accel;
        p.vel.z += wish.z * accel;

        if (in.jump && p.onGround) p.vel.y = 0.42f;

        p.vel.y -= 0.08f;
        p.vel.y *= 0.98f;

        const float drag = p.onGround ? 0.546f : 0.91f;
        p.vel.x *= drag;
        p.vel.z *= drag;
    }

    // Axis-separated movement with sub-steps (no tunnelling at sprint speeds).
    const float maxStep = 0.2f;
    const float dx = p.vel.x, dy = p.vel.y, dz = p.vel.z;
    const float dist = std::max({std::fabs(dx), std::fabs(dy), std::fabs(dz)});
    const int steps = std::max(1, static_cast<int>(std::ceil(dist / maxStep)));
    const float sx = dx / static_cast<float>(steps);
    const float sy = dy / static_cast<float>(steps);
    const float sz = dz / static_cast<float>(steps);

    for (int i = 0; i < steps; ++i) {
        if (sx != 0.0f) {
            p.pos.x += sx;
            if (collides(world, p.pos)) {
                p.pos.x -= sx;
                p.vel.x = 0.0f;
            }
        }
        if (sz != 0.0f) {
            p.pos.z += sz;
            if (collides(world, p.pos)) {
                p.pos.z -= sz;
                p.vel.z = 0.0f;
            }
        }
        if (sy != 0.0f) {
            p.pos.y += sy;
            if (collides(world, p.pos)) {
                p.pos.y -= sy;
                if (sy < 0.0f) p.onGround = true;
                p.vel.y = 0.0f;
            }
        }
    }

    if (p.vel.y <= 0.0f && p.onGround && p.flying == false) {
        // keep onGround until next downward probe fails
        const Vec3 probe{p.pos.x, p.pos.y - 0.05f, p.pos.z};
        if (!collides(world, probe)) p.onGround = false;
    } else if (!p.flying) {
        p.onGround = false;
    }
}

RayHit raycast(const World& world, const Vec3& origin, const Vec3& dirIn, float maxDist) {
    RayHit hit;
    const Vec3 dir = normalize(dirIn);
    if (length(dir) < 0.5f) return hit;

    int x = static_cast<int>(std::floor(origin.x));
    int y = static_cast<int>(std::floor(origin.y));
    int z = static_cast<int>(std::floor(origin.z));

    const int stepX = dir.x > 0 ? 1 : (dir.x < 0 ? -1 : 0);
    const int stepY = dir.y > 0 ? 1 : (dir.y < 0 ? -1 : 0);
    const int stepZ = dir.z > 0 ? 1 : (dir.z < 0 ? -1 : 0);

    const float invX = dir.x != 0 ? 1.0f / std::fabs(dir.x) : 1e30f;
    const float invY = dir.y != 0 ? 1.0f / std::fabs(dir.y) : 1e30f;
    const float invZ = dir.z != 0 ? 1.0f / std::fabs(dir.z) : 1e30f;

    float tMaxX = dir.x != 0
        ? ((stepX > 0 ? static_cast<float>(x + 1) - origin.x : origin.x - static_cast<float>(x)) * invX)
        : 1e30f;
    float tMaxY = dir.y != 0
        ? ((stepY > 0 ? static_cast<float>(y + 1) - origin.y : origin.y - static_cast<float>(y)) * invY)
        : 1e30f;
    float tMaxZ = dir.z != 0
        ? ((stepZ > 0 ? static_cast<float>(z + 1) - origin.z : origin.z - static_cast<float>(z)) * invZ)
        : 1e30f;

    float t = 0.0f;
    for (int guard = 0; guard < 256 && t <= maxDist; ++guard) {
        const Block b = world.getBlock(x, y, z);
        if (isOpaque(b)) {
            hit.hit = true;
            hit.x = x;
            hit.y = y;
            hit.z = z;
            return hit;
        }
        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            x += stepX;
            t = tMaxX;
            tMaxX += invX;
            hit.nx = -stepX;
            hit.ny = 0;
            hit.nz = 0;
        } else if (tMaxY < tMaxZ) {
            y += stepY;
            t = tMaxY;
            tMaxY += invY;
            hit.nx = 0;
            hit.ny = -stepY;
            hit.nz = 0;
        } else {
            z += stepZ;
            t = tMaxZ;
            tMaxZ += invZ;
            hit.nx = 0;
            hit.ny = 0;
            hit.nz = -stepZ;
        }
    }
    return RayHit{};
}
