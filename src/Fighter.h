#pragma once

#include <vector>

#include "Characters.h"
#include "raylib.h"

struct Controls {
    KeyboardKey left, right, jump, block, attack, skill, ultimate;
};

struct ActiveBuff {
    BuffKind kind;
    float remaining;
    float magnitude;
};

// Runtime state of one player during a match.
struct Fighter {
    static constexpr float kWidth = 60.0f;
    static constexpr float kHeight = 120.0f;

    const CharacterDef* def;
    int playerIndex;
    Controls controls;

    Vector2 pos;  // feet position (bottom-center)
    Vector2 vel{0.0f, 0.0f};
    int facing;
    bool onGround = true;
    bool blocking = false;

    float hp;
    float energy = 0.0f;
    float skillCooldown = 0.0f;
    float attackCooldown = 0.0f;
    float attackFlash = 0.0f;  // visual timer for the melee swing
    float stunTimer = 0.0f;
    float poisonDps = 0.0f;
    float poisonTimer = 0.0f;
    float damageDealt = 0.0f;
    std::vector<ActiveBuff> buffs;

    Fighter(const CharacterDef& character, int index, float startX, int startFacing, Controls keys);

    Rectangle hurtbox() const;
    Rectangle meleeHitbox() const;

    bool isAlive() const { return hp > 0.0f; }
    bool isStunned() const { return stunTimer > 0.0f; }
    bool hasBuff(BuffKind kind) const;
    float buffMagnitude(BuffKind kind) const;
    float damageMultiplier() const;
    float speedMultiplier() const;

    void addEnergy(float amount);
    float takeDamage(float amount);  // returns the damage actually applied
    void applyStatus(float stun, float poisonPerSecond, float poisonDuration);
    void heal(float amount);
    void addBuff(BuffKind kind, float duration, float magnitude);

    void updateTimers(float dt);
    void updatePhysics(float dt);
};
