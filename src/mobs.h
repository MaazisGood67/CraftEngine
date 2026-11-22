// CraftEngine - mob entities with vanilla stats and goal-based AI.
#pragma once
#include <cstdint>
#include <vector>

#include "combat.h"
#include "vmath.h"

enum class MobType : uint8_t {
    Pig, Cow, Sheep, Chicken, Horse, Wolf, Cat,
    Zombie, Skeleton, Spider, Creeper, Enderman, Witch,
    Blaze, Ghast, Slime, Guardian,
    EnderDragon, Wither,
    MOB_COUNT
};

enum class MobState : uint8_t { Idle, Wander, Chase, Attack, Flee, Hurt, Dead };

struct Mob {
    uint32_t   id = 0;
    MobType    type = MobType::Pig;
    Vec3       pos{};
    Vec3       prevPos{};
    Vec3       vel{};
    float      yaw = 0.0f;
    float      pitch = 0.0f;
    float      health = 10.0f;
    float      maxHealth = 10.0f;
    float      speed = 0.2f;
    float      damage = 0.0f;
    float      width = 0.6f;
    float      height = 1.8f;
    float      attackCooldown = 0.0f;
    float      aiTick = 0.0f;
    float      hurtTime = 0.0f;
    float      age = 0.0f;
    bool       hostile = false;
    bool       onGround = false;
    bool       burning = false;
    MobState   state = MobState::Idle;
    Vec3       target{};
    int        targetEntity = -1; // index into MobManager, -1 = player
};

struct MobStats { float hp, speed, dmg, width, height; bool hostile; };
MobStats mobStats(MobType t);
const char* mobName(MobType t);

class MobManager {
public:
    std::vector<Mob> mobs;

    uint32_t spawn(MobType t, const Vec3& at);
    void tick(float dt, const Vec3& playerPos, CombatState& player,
              bool playerSneaking);
    // Returns index of mob the player is looking at within reach, or -1.
    int pick(const Vec3& eye, const Vec3& dir, float reach) const;
    void damage(int index, float amount, DamageSource src);
    void clear() { mobs.clear(); }

    uint32_t nextId = 1;
};
