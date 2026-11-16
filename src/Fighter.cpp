#include "Fighter.h"

#include <algorithm>

#include "Config.h"

Fighter::Fighter(const CharacterDef& character, int index, float startX, int startFacing, Controls keys)
    : def(&character),
      playerIndex(index),
      controls(keys),
      pos{startX, config::kGroundY},
      facing(startFacing),
      hp(character.maxHp) {}

Rectangle Fighter::hurtbox() const {
    return {pos.x - kWidth / 2.0f, pos.y - kHeight, kWidth, kHeight};
}

Rectangle Fighter::meleeHitbox() const {
    constexpr float reach = 70.0f;
    const float x = facing > 0 ? pos.x + kWidth / 2.0f : pos.x - kWidth / 2.0f - reach;
    return {x, pos.y - kHeight * 0.8f, reach, kHeight * 0.5f};
}

bool Fighter::hasBuff(BuffKind kind) const {
    return std::any_of(buffs.begin(), buffs.end(), [kind](const ActiveBuff& b) { return b.kind == kind; });
}

float Fighter::buffMagnitude(BuffKind kind) const {
    for (const ActiveBuff& b : buffs) {
        if (b.kind == kind) return b.magnitude;
    }
    return 0.0f;
}

float Fighter::damageMultiplier() const {
    const float m = buffMagnitude(BuffKind::DamageUp);
    return m > 0.0f ? m : 1.0f;
}

float Fighter::speedMultiplier() const {
    const float m = buffMagnitude(BuffKind::Haste);
    return m > 0.0f ? m : 1.0f;
}

void Fighter::addEnergy(float amount) {
    energy = std::clamp(energy + amount, 0.0f, config::kMaxEnergy);
}

float Fighter::takeDamage(float amount) {
    if (hasBuff(BuffKind::Shield)) return 0.0f;
    if (blocking) amount *= config::kBlockDamageMultiplier;
    amount = std::min(amount, hp);
    hp -= amount;
    if (amount > 0.0f) addEnergy(config::kEnergyOnHitTaken);
    return amount;
}

void Fighter::applyStatus(float stun, float poisonPerSecond, float poisonDuration) {
    if (hasBuff(BuffKind::Shield) || blocking) return;
    stunTimer = std::max(stunTimer, stun);
    if (poisonPerSecond > 0.0f) {
        poisonDps = std::max(poisonDps, poisonPerSecond);
        poisonTimer = std::max(poisonTimer, poisonDuration);
    }
}

void Fighter::heal(float amount) {
    if (!isAlive()) return;
    hp = std::min(def->maxHp, hp + amount);
}

void Fighter::addBuff(BuffKind kind, float duration, float magnitude) {
    std::erase_if(buffs, [kind](const ActiveBuff& b) { return b.kind == kind; });
    buffs.push_back({kind, duration, magnitude});
}

void Fighter::updateTimers(float dt) {
    skillCooldown = std::max(0.0f, skillCooldown - dt);
    attackCooldown = std::max(0.0f, attackCooldown - dt);
    attackFlash = std::max(0.0f, attackFlash - dt);
    stunTimer = std::max(0.0f, stunTimer - dt);

    if (poisonTimer > 0.0f) {
        hp = std::max(0.0f, hp - poisonDps * dt);
        poisonTimer -= dt;
        if (poisonTimer <= 0.0f) {
            poisonTimer = 0.0f;
            poisonDps = 0.0f;
        }
    }

    heal(buffMagnitude(BuffKind::Regen) * dt);

    for (ActiveBuff& b : buffs) b.remaining -= dt;
    std::erase_if(buffs, [](const ActiveBuff& b) { return b.remaining <= 0.0f; });

    if (isAlive()) addEnergy(config::kEnergyPerSecond * dt);
}

void Fighter::updatePhysics(float dt) {
    vel.y += config::kGravity * dt;
    pos.x += vel.x * dt;
    pos.y += vel.y * dt;

    if (pos.y >= config::kGroundY) {
        pos.y = config::kGroundY;
        vel.y = 0.0f;
        onGround = true;
    }
    pos.x = std::clamp(pos.x, kWidth / 2.0f, config::kScreenWidth - kWidth / 2.0f);
}
