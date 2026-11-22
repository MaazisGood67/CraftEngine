#include "combat.h"

#include <algorithm>
#include <cmath>

#include "vanilla.h"

namespace {

bool bypassesArmor(DamageSource s) {
    switch (s) {
        case DamageSource::Fall:
        case DamageSource::Drown:
        case DamageSource::Burn:
        case DamageSource::Starve:
        case DamageSource::Void:
        case DamageSource::Magic:
        case DamageSource::Suicide:
            return true;
        default:
            return false;
    }
}

} // namespace

float applyArmor(float damage, int armorPoints, float toughness, DamageSource src) {
    if (bypassesArmor(src)) return damage;
    if (armorPoints <= 0) return damage;

    // Vanilla 1.9+ armour formula:
    //  Reduction = min(20, armour * 4) / 25  (with a toughness term for high values)
    float reduction = std::min(20.0f, static_cast<float>(armorPoints) * 4.0f) / 25.0f;
    reduction = std::min(0.8f, reduction); // armour can reduce at most 80%
    float result = damage * (1.0f - reduction);

    // Diamond/netherite toughness blunts the 80% cap for heavy hits.
    if (damage > 4.0f && toughness > 0.0f) {
        const float extra = std::min(1.0f, toughness * 4.0f / (damage + 4.0f));
        result = std::max(1.0f, result - extra * damage * 0.04f);
    }
    return result;
}

float damageEntity(CombatState& t, float amount, DamageSource src,
                   int armorPoints, float toughness) {
    if (t.dead || t.invulnerableTicks > 0.0f) return 0.0f;
    if (amount <= 0.0f) return 0.0f;

    amount = applyArmor(amount, armorPoints, toughness, src);

    // Absorption shields absorb damage before health.
    if (t.absorption > 0.0f) {
        const float absorbed = std::min(t.absorption, amount);
        t.absorption -= absorbed;
        amount -= absorbed;
    }
    t.health -= amount;
    t.lastDamage = amount;
    t.hurtTime = 10.0f;
    t.invulnerableTicks = Vanilla::INVULN_TICKS;

    if (t.health <= 0.0f) {
        t.health = 0.0f;
        t.dead = true;
        ++t.deathCount;
    }
    return amount;
}

float playerAttack(CombatState& atk, const Inventory& inv, CombatState& target,
                   bool sprinting, bool onGround, int targetArmor, float targetToughness) {
    if (atk.attackCooldown > 0.0f || atk.dead) return -1.0f;
    const ItemStack& held = inv.selectedStack();
    const ItemDef& def = itemDef(held.id);

    float dmg = def.attackDamage > 0.0f ? def.attackDamage : Vanilla::ATTACK_DAMAGE_BASE;
    // Critical hit: falling while not sprinting, not on ground, not attacking from a bow.
    const bool critical = !sprinting && !onGround && atk.fallDistance > 0.0f;
    if (critical) dmg *= Vanilla::CRIT_MULTIPLIER;

    const float dealt = damageEntity(target, dmg, DamageSource::Player,
                                     targetArmor, targetToughness);

    // Vanilla attack cooldown: 1 / attackSpeed in ticks*20.
    if (def.attackSpeed > 0.0f) {
        atk.attackCooldown = std::max(0.0f, 20.0f / def.attackSpeed);
    } else {
        atk.attackCooldown = Vanilla::ATTACK_COOLDOWN * Vanilla::TPS;
    }
    if (!held.empty() && def.maxDurability > 0) {
        // durability handled by caller with inventory.damageItem(slot, 1)
    }
    return dealt;
}

Vec3 knockbackVelocity(const Vec3& from, const Vec3& to, float strength) {
    Vec3 dir = to - from;
    dir.y = 0.0f;
    const float l = length(dir);
    if (l < 1e-4f) return {0, Vanilla::KNOCKBACK_UP * strength, 0};
    dir = dir * (1.0f / l);
    return {dir.x * Vanilla::KNOCKBACK_STENGTH * strength,
            Vanilla::KNOCKBACK_UP * strength,
            dir.z * Vanilla::KNOCKBACK_STENGTH * strength};
}

float fallDamage(float fallDistance) {
    // Vanilla: damage = floor(distance - 3), 0 if distance <= 3.
    if (fallDistance <= 3.0f) return 0.0f;
    return static_cast<float>(static_cast<int>(std::floor(fallDistance - 3.0f)));
}

void tickCombat(CombatState& s) {
    if (s.invulnerableTicks > 0.0f) s.invulnerableTicks -= 1.0f;
    if (s.attackCooldown > 0.0f) s.attackCooldown -= 1.0f;
    if (s.hurtTime > 0.0f) s.hurtTime -= 1.0f;
}

void armorFromInventory(const Inventory& inv, int& points, float& toughness) {
    points = 0;
    toughness = 0.0f;
    // Diamond armour: helmet 2, chest 8, legs 6, boots 2 (total 18) + toughness 2 each.
    static const int armorPoints[4] = {2, 8, 6, 2};
    for (int i = 0; i < 4; ++i) {
        const ItemStack& s = inv.armor(i);
        if (s.empty()) continue;
        if (s.id >= ITEM_ARMOR_HELMET && s.id <= ITEM_ARMOR_BOOTS) {
            points += armorPoints[i];
            toughness += 2.0f;
        }
    }
}
