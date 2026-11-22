// CraftEngine - vanilla Minecraft 26.3 gameplay values, centralised.
//
// Every gameplay constant lives here so the simulation can be verified against
// the vanilla values instead of being sprinkled through the code as magic
// numbers. Units are Minecraft's: 1 block = 1 metre, 1 tick = 1/20 second.
//
// Sources: observed vanilla behaviour and the public Minecraft Wiki values
// (movement, combat, block hardness, tool tiers, mob stats). No Mojang code or
// decompiled source is included - these are plain gameplay numbers.
#pragma once

#include <cstdint>

namespace Vanilla {

// ---------------------------------------------------------------------------
// Simulation
// ---------------------------------------------------------------------------
constexpr int     TPS          = 20;          // ticks per second
constexpr float   TICK         = 1.0f / TPS;  // 0.05 s
constexpr float   GRAVITY      = 0.08f;       // blocks/tick^2
constexpr float   GRAVITY_DRAG = 0.98f;       // vertical velocity damping
constexpr float   TERMINAL_VY  = 3.92f;       // max fall speed, blocks/tick
constexpr float   STEP_HEIGHT  = 0.6f;        // blocks
constexpr float   GROUND_FRICTION = 0.6f;     // horizontal damping on ground
constexpr float   AIR_FRICTION    = 0.91f;    // horizontal damping in air
constexpr float   WATER_DRAG   = 0.8f;
constexpr float   SLIPPERY_DEFAULT = 0.6f;

// ---------------------------------------------------------------------------
// Player geometry (Java Edition)
// ---------------------------------------------------------------------------
constexpr float   PLAYER_WIDTH   = 0.6f;
constexpr float   PLAYER_HEIGHT  = 1.8f;
constexpr float   PLAYER_EYE     = 1.62f;
constexpr float   SNEAK_EYE      = 1.27f;
constexpr float   PLAYER_HALF_W  = PLAYER_WIDTH * 0.5f; // 0.3

// ---------------------------------------------------------------------------
// Player movement, blocks/tick (multiply by TPS for blocks/second)
// ---------------------------------------------------------------------------
constexpr float   WALK_SPEED    = 4.317f / TPS; // 0.21585
constexpr float   SPRINT_SPEED  = 5.612f / TPS; // 0.2806
constexpr float   SNEAK_SPEED   = 1.295f / TPS; // 0.06475
constexpr float   SWIM_SPEED    = 2.2f   / TPS;
constexpr float   FLY_SPEED     = 10.89f / TPS;
constexpr float   FLY_SPRINT    = 21.89f / TPS;
constexpr float   JUMP_SPEED    = 0.42f;        // blocks/tick => 1.25 block jump
constexpr float   JUMP_BOOST    = 0.1f;         // per level of Jump Boost

// ---------------------------------------------------------------------------
// Interaction / combat
// ---------------------------------------------------------------------------
constexpr float   REACH_CREATIVE   = 5.0f;
constexpr float   REACH_SURVIVAL   = 4.5f;
constexpr float   ATTACK_COOLDOWN  = 0.625f;    // seconds (1.6 attacks/s)
constexpr float   ATTACK_DAMAGE_BASE = 1.0f;    // bare hand, 0.5 heart
constexpr float   CRIT_MULTIPLIER  = 1.5f;
constexpr float   KNOCKBACK_STENGTH = 0.4f;     // per knockback level
constexpr float   KNOCKBACK_UP     = 0.35f;
constexpr float   INVULN_TICKS     = 20.0f;     // 1 s after being hit
constexpr float   EXHAUSTION_JUMP  = 0.2f;
constexpr float   EXHAUSTION_SPRINT = 0.1f;     // per block sprinted
constexpr float   EXHAUSTION_ATTACK = 0.1f;

// ---------------------------------------------------------------------------
// Vitals
// ---------------------------------------------------------------------------
constexpr float   MAX_HEALTH   = 20.0f;  // 10 hearts
constexpr float   MAX_HUNGER   = 20.0f;
constexpr float   MAX_SATURATION = 5.0f;
constexpr float   SATURATION_HEAL_MIN = 18.0f;
constexpr float   HUNGER_STARVE_DAMAGE = 1.0f;

// ---------------------------------------------------------------------------
// World
// ---------------------------------------------------------------------------
constexpr int     SEA_LEVEL      = 63;     // vanilla; this engine uses 24 for its compact 128-high world
constexpr int     WORLD_HEIGHT   = 128;    // compact (vanilla is 384)
constexpr int     CHUNK_SIZE     = 16;
constexpr int     CHUNK_SECTION  = 16;
constexpr int     VIEW_DISTANCE_MIN = 2;
constexpr int     VIEW_DISTANCE_MAX = 32;

// ---------------------------------------------------------------------------
// Block properties (hardness in seconds with bare hand; -1 = unbreakable)
// ---------------------------------------------------------------------------
constexpr float HARDNESS_AIR      = -1.0f;
constexpr float HARDNESS_STONE    = 1.5f;
constexpr float HARDNESS_DIRT     = 0.5f;
constexpr float HARDNESS_GRASS    = 0.6f;
constexpr float HARDNESS_SAND     = 0.5f;
constexpr float HARDNESS_WATER    = -1.0f;
constexpr float HARDNESS_LOG      = 2.0f;
constexpr float HARDNESS_LEAVES   = 0.2f;
constexpr float HARDNESS_PLANKS   = 2.0f;
constexpr float HARDNESS_COBBLE   = 2.0f;
constexpr float HARDNESS_BEDROCK  = -1.0f;

// Mining speed multipliers by tool tier (1.0 = bare hand / wrong tool).
constexpr float TOOL_WOOD  = 2.0f;
constexpr float TOOL_STONE = 4.0f;
constexpr float TOOL_IRON  = 6.0f;
constexpr float TOOL_DIAMOND = 8.0f;
constexpr float TOOL_NETHERITE = 9.0f;

// Break time in ticks: hardness * 30 / toolSpeed (with the correct tool).
inline float breakTicks(float hardness, float toolSpeed) {
    if (hardness < 0.0f) return -1.0f;
    return hardness * 30.0f / (toolSpeed > 0.0f ? toolSpeed : 1.0f);
}

// ---------------------------------------------------------------------------
// Mob base stats (used by the mob/combat phases)
// ---------------------------------------------------------------------------
struct MobStats {
    float health;
    float speed;      // blocks/tick
    float damage;
    float width;
    float height;
    bool  hostile;
};

constexpr MobStats MOB_ZOMBIE  {20.0f, 0.23f / TPS, 3.0f, 0.6f, 1.95f, true};
constexpr MobStats MOB_SKELETON{20.0f, 0.25f / TPS, 2.0f, 0.6f, 1.99f, true};
constexpr MobStats MOB_SPIDER  {16.0f, 0.3f  / TPS, 2.0f, 1.4f, 0.9f,  true};
constexpr MobStats MOB_CREEPER {20.0f, 0.25f / TPS, 0.0f, 0.6f, 1.7f,  true};
constexpr MobStats MOB_COW     {10.0f, 0.2f  / TPS, 0.0f, 0.9f, 1.3f,  false};
constexpr MobStats MOB_PIG     {10.0f, 0.2f  / TPS, 0.0f, 0.9f, 1.1f,  false};
constexpr MobStats MOB_SHEEP   {8.0f,  0.2f  / TPS, 0.0f, 0.9f, 1.3f,  false};
constexpr MobStats MOB_ENDERMAN{40.0f, 0.3f  / TPS, 7.0f, 0.6f, 2.9f,  true};
constexpr MobStats MOB_DRAGON  {200.0f, 0.35f / TPS, 15.0f, 8.0f, 8.0f, true};

// Ender Dragon: 200 HP = 10 hearts (vanilla 26.3 uses 200 HP).
constexpr int     DRAGON_HEALTH = 200;
constexpr int     END_CRYSTALS  = 10;   // vanilla obsidian crystal count
constexpr int     DRAGON_FIREBALL_DAMAGE = 12;
constexpr float   DRAGON_ROAM_RADIUS = 96.0f;

} // namespace Vanilla
