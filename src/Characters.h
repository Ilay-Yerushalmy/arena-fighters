#pragma once

#include <array>
#include <string>

#include "raylib.h"

// What an ability does when it is triggered.
enum class AbilityKind {
    Projectile,  // fires `count` projectiles forward
    Dash,        // moves `speed` pixels forward, hits whoever is in the path
    Teleport,    // appears behind the opponent
    Heal,        // restores `magnitude` HP
    AreaBlast,   // hits everything inside `radius`
    Buff,        // applies a timed buff to the user
    Flurry,      // `count` rapid melee hits within `radius`
};

enum class BuffKind { None, DamageUp, Shield, Haste, Regen };

// Data-driven ability description. New characters need no new code,
// only a new combination of these fields.
struct AbilityDef {
    std::string name;
    AbilityKind kind = AbilityKind::Projectile;
    float cooldown = 0.0f;  // seconds - only used by the time-restricted skill
    float damage = 0.0f;
    float speed = 0.0f;   // projectile speed or dash distance
    float radius = 0.0f;  // projectile / area / flurry radius
    int count = 1;        // projectiles fired or flurry hits
    float stun = 0.0f;    // seconds the target is stunned on hit
    float poisonDps = 0.0f;
    float poisonDuration = 0.0f;
    BuffKind buff = BuffKind::None;
    float buffDuration = 0.0f;
    float magnitude = 0.0f;  // heal amount or buff strength
};

struct CharacterDef {
    int id;  // matches characters.id in the database
    std::string name;
    std::string description;
    Color color;
    float maxHp;
    float moveSpeed;
    float attackDamage;
    AbilityDef skill;     // time-restricted: usable again after `cooldown`
    AbilityDef ultimate;  // resource-based: needs a full energy bar
};

constexpr int kCharacterCount = 8;

const std::array<CharacterDef, kCharacterCount>& characterRoster();
