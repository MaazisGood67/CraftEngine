#include "mobs.h"

#include <algorithm>
#include <cmath>

#include "vanilla.h"

namespace {

constexpr float kPi = 3.14159265358979323846f;

float dist2D(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

float hash01(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15;
    x *= 0x846ca68bu; x ^= x >> 16;
    return static_cast<float>(x & 0xFFFFFF) / 16777216.0f;
}

} // namespace

MobStats mobStats(MobType t) {
    switch (t) {
        case MobType::Pig:       return {10, 0.20f / Vanilla::TPS, 0, 0.9f, 1.1f, false};
        case MobType::Cow:       return {10, 0.20f / Vanilla::TPS, 0, 0.9f, 1.3f, false};
        case MobType::Sheep:     return {8,  0.20f / Vanilla::TPS, 0, 0.9f, 1.3f, false};
        case MobType::Chicken:   return {4,  0.25f / Vanilla::TPS, 0, 0.4f, 0.7f, false};
        case MobType::Horse:     return {30, 0.25f / Vanilla::TPS, 0, 1.4f, 1.6f, false};
        case MobType::Wolf:      return {8,  0.30f / Vanilla::TPS, 3, 0.6f, 0.85f, true};
        case MobType::Cat:       return {10, 0.30f / Vanilla::TPS, 2, 0.6f, 0.8f, false};
        case MobType::Zombie:    return {20, 0.23f / Vanilla::TPS, 3, 0.6f, 1.95f, true};
        case MobType::Skeleton:  return {20, 0.25f / Vanilla::TPS, 2, 0.6f, 1.99f, true};
        case MobType::Spider:    return {16, 0.30f / Vanilla::TPS, 2, 1.4f, 0.9f,  true};
        case MobType::Creeper:   return {20, 0.25f / Vanilla::TPS, 0, 0.6f, 1.7f,  true};
        case MobType::Enderman:  return {40, 0.30f / Vanilla::TPS, 7, 0.6f, 2.9f,  true};
        case MobType::Witch:     return {26, 0.25f / Vanilla::TPS, 2, 0.6f, 1.95f, true};
        case MobType::Blaze:     return {20, 0.23f / Vanilla::TPS, 6, 0.6f, 1.8f,  true};
        case MobType::Ghast:     return {10, 0.20f / Vanilla::TPS, 4, 4.0f, 4.0f,  true};
        case MobType::Slime:     return {16, 0.25f / Vanilla::TPS, 3, 1.0f, 1.0f,  true};
        case MobType::Guardian:  return {30, 0.25f / Vanilla::TPS, 6, 0.85f, 0.85f,true};
        case MobType::EnderDragon:return {200, 0.35f / Vanilla::TPS, 15, 8.0f, 8.0f,true};
        case MobType::Wither:    return {300, 0.25f / Vanilla::TPS, 5, 3.0f, 4.0f, true};
        default: return {10, 0.2f / Vanilla::TPS, 0, 0.6f, 1.8f, false};
    }
}

const char* mobName(MobType t) {
    switch (t) {
        case MobType::Pig: return "Pig"; case MobType::Cow: return "Cow";
        case MobType::Sheep: return "Sheep"; case MobType::Chicken: return "Chicken";
        case MobType::Horse: return "Horse"; case MobType::Wolf: return "Wolf";
        case MobType::Cat: return "Cat"; case MobType::Zombie: return "Zombie";
        case MobType::Skeleton: return "Skeleton"; case MobType::Spider: return "Spider";
        case MobType::Creeper: return "Creeper"; case MobType::Enderman: return "Enderman";
        case MobType::Witch: return "Witch"; case MobType::Blaze: return "Blaze";
        case MobType::Ghast: return "Ghast"; case MobType::Slime: return "Slime";
        case MobType::Guardian: return "Guardian";
        case MobType::EnderDragon: return "Ender Dragon";
        case MobType::Wither: return "Wither";
        default: return "Entity";
    }
}

uint32_t MobManager::spawn(MobType t, const Vec3& at) {
    const MobStats s = mobStats(t);
    Mob m;
    m.id = nextId++;
    m.type = t;
    m.pos = at;
    m.prevPos = at;
    m.health = m.maxHealth = s.hp;
    m.speed = s.speed;
    m.damage = s.dmg;
    m.width = s.width;
    m.height = s.height;
    m.hostile = s.hostile;
    m.state = MobState::Idle;
    mobs.push_back(m);
    return m.id;
}

void MobManager::tick(float /*dt*/, const Vec3& playerPos, CombatState& player,
                      bool playerSneaking) {
    for (size_t i = 0; i < mobs.size(); ++i) {
        Mob& m = mobs[i];
        m.prevPos = m.pos;
        m.age += Vanilla::TICK;
        if (m.attackCooldown > 0.0f) m.attackCooldown -= 1.0f;
        if (m.hurtTime > 0.0f) m.hurtTime -= 1.0f;

        const float d = dist2D(m.pos, playerPos);
        const float detect = playerSneaking ? 8.0f : 16.0f;

        if (m.state == MobState::Dead) continue;

        // Hostile mobs detect and chase the player within range.
        if (m.hostile && d < detect && m.hurtTime <= 0.0f) {
            m.state = MobState::Chase;
            m.target = playerPos;
        } else if (m.state == MobState::Chase && d > detect * 1.5f) {
            m.state = MobState::Wander;
        }

        // Creeper fuses when very close (vanilla: 1.5 blocks detonation radius).
        if (m.type == MobType::Creeper && d < 2.0f && m.state == MobState::Chase) {
            m.state = MobState::Attack;
        }

        Vec3 wish{};
        if (m.state == MobState::Chase || m.state == MobState::Attack) {
            const Vec3 delta = m.target - m.pos;
            wish = normalize(Vec3{delta.x, 0, delta.z});
        } else if (m.state == MobState::Wander || m.state == MobState::Idle) {
            m.aiTick -= 1.0f;
            if (m.aiTick <= 0.0f) {
                m.aiTick = 40.0f + hash01(m.id * 7 + static_cast<uint32_t>(m.age)) * 100.0f;
                const float a = hash01(m.id + static_cast<uint32_t>(m.age) * 13) * 2.0f * kPi;
                m.target = {m.pos.x + std::cos(a) * 4.0f, m.pos.y, m.pos.z + std::sin(a) * 4.0f};
                m.state = MobState::Wander;
            }
            const Vec3 delta = m.target - m.pos;
            if (length(delta) > 0.4f) wish = normalize(Vec3{delta.x, 0, delta.z});
            else m.state = MobState::Idle;
        }

        if (length(wish) > 0.0f) {
            m.vel.x = wish.x * m.speed * 20.0f;
            m.vel.z = wish.z * m.speed * 20.0f;
            m.pos.x += m.vel.x;
            m.pos.z += m.vel.z;
            m.yaw = std::atan2(wish.x, wish.z);
        }

        // Melee attack: vanilla reach ~2 blocks for most hostiles.
        if (m.damage > 0.0f && m.attackCooldown <= 0.0f && d < 2.0f &&
            (m.state == MobState::Chase || m.state == MobState::Attack)) {
            damageEntity(player, m.damage, DamageSource::Mob);
            m.attackCooldown = 20.0f; // 1 s attack interval
            m.state = MobState::Attack;
        }

        // Creeper explodes (damage roughly 12-22 depending on difficulty).
        if (m.type == MobType::Creeper && m.state == MobState::Attack && d < 2.5f) {
            damageEntity(player, 16.0f, DamageSource::Explosion);
            m.health = 0.0f;
            m.state = MobState::Dead;
        }

        if (m.health <= 0.0f) m.state = MobState::Dead;
    }

    // Remove dead mobs after their death animation tick.
    mobs.erase(std::remove_if(mobs.begin(), mobs.end(), [](const Mob& m) {
        return m.state == MobState::Dead && m.hurtTime <= 0.0f;
    }), mobs.end());
}

int MobManager::pick(const Vec3& eye, const Vec3& dir, float reach) const {
    int best = -1;
    float bestT = reach;
    for (size_t i = 0; i < mobs.size(); ++i) {
        const Mob& m = mobs[i];
        if (m.state == MobState::Dead) continue;
        const Vec3 centre{m.pos.x, m.pos.y + m.height * 0.5f, m.pos.z};
        const Vec3 oc = centre - eye;
        const float proj = dot(oc, dir);
        if (proj < 0.0f || proj > bestT) continue;
        const Vec3 closest = eye + dir * proj;
        const float perp = length(centre - closest);
        if (perp < std::max(m.width, m.height * 0.5f) * 0.75f) {
            bestT = proj;
            best = static_cast<int>(i);
        }
    }
    return best;
}

void MobManager::damage(int index, float amount, DamageSource src) {
    if (index < 0 || index >= static_cast<int>(mobs.size())) return;
    Mob& m = mobs[index];
    CombatState cs;
    cs.health = m.health;
    damageEntity(cs, amount, src);
    m.health = cs.health;
    m.hurtTime = cs.hurtTime;
    if (m.health <= 0.0f) m.state = MobState::Dead;
}
