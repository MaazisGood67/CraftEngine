// CraftEngine - vanilla combat: damage, armour, knockback, crits, cooldown.
#pragma once
#include <cstdint>

#include "inventory.h"
#include "vmath.h"

enum class DamageSource : uint8_t {
    Generic, Player, Mob, Fall, Drown, Burn, Explosion, Projectile,
    Starve, Void, Magic, Wither, Thorns, Cactus, Lightning, Suicide
};

struct CombatState {
    float health = 20.0f;
    float maxHealth = 20.0f;
    float absorption = 0.0f;
    float invulnerableTicks = 0.0f;   // 1 s = 20 ticks
    float attackCooldown = 0.0f;      // ticks until next attack
    float hurtTime = 0.0f;
    float lastDamage = 0.0f;
    float fallDistance = 0.0f;
    int   deathCount = 0;
    bool  dead = false;
};

// Vanilla armour points -> damage reduction: 4 points = 1 icon = 4% reduction.
// Formula: damage *= 1 - min(20, (armourPoints * 4)) / 25 ... then * (1 - toughness)
float applyArmor(float damage, int armorPoints, float toughness, DamageSource src);

// Applies damage to `target`, returns the amount actually taken.
float damageEntity(CombatState& target, float amount, DamageSource src,
                   int armorPoints = 0, float toughness = 0.0f);

// Player melee attack against an entity: returns damage dealt (-1 on cooldown).
float playerAttack(CombatState& attackerState, const Inventory& inv,
                   CombatState& target, bool sprinting, bool onGround,
                   int targetArmor = 0, float targetToughness = 0.0f);

// Knockback velocity to apply to the target (vanilla: 0.4 per level + 0.35 up).
Vec3 knockbackVelocity(const Vec3& from, const Vec3& to, float strength = 1.0f);

// Fall damage: damage = floor(fallDistance - 3), capped by vanilla rules.
float fallDamage(float fallDistance);

void tickCombat(CombatState& s);

// Armour points / toughness from 4 equipped pieces (diamond = 3 each).
void armorFromInventory(const Inventory& inv, int& points, float& toughness);
