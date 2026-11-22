// CraftEngine - Minecraft-value player physics + voxel interaction.
#pragma once
#include <cstdint>

#include "vmath.h"
#include "world.h"

struct Player {
    Vec3 pos{128.5f, 90.0f, 128.5f}; // feet centre
    Vec3 prevPos{128.5f, 90.0f, 128.5f}; // for render interpolation
    Vec3 vel{};
    float yaw = 0.0f;
    float pitch = 0.0f;
    bool onGround = false;
    bool flying = false;

    static constexpr float kHalfW = 0.3f;   // AABB 0.6 x 1.8
    static constexpr float kHeight = 1.8f;
    static constexpr float kEye = 1.62f;

    Vec3 eye() const { return {pos.x, pos.y + kEye, pos.z}; }
    Vec3 forward() const;
};

struct PlayerInput {
    float forward = 0; // -1..1
    float strafe = 0;  // -1..1
    bool jump = false;
    bool sneak = false;
    bool sprint = false;
};

// Fixed 1/20 s tick with vanilla constants (gravity 0.08, jump 0.42).
void tickPlayer(Player& p, World& world, const PlayerInput& in);

struct RayHit {
    bool hit = false;
    int x = 0, y = 0, z = 0;
    int nx = 0, ny = 0, nz = 0; // face normal
};

RayHit raycast(const World& world, const Vec3& origin, const Vec3& dir, float maxDist);
