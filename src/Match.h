#pragma once

#include <array>
#include <string>
#include <vector>

#include "Fighter.h"

struct Projectile {
    Vector2 pos;
    Vector2 vel;
    float radius;
    float damage;
    float stun;
    float poisonDps;
    float poisonDuration;
    int owner;
    Color color;
    bool alive = true;
};

struct FloatingText {
    std::string text;
    Vector2 pos;
    float ttl;
    Color color;
};

struct AreaEffect {
    Vector2 center;
    float radius;
    float ttl;
    float maxTtl;
    Color color;
};

struct MatchResult {
    int winnerIndex;  // 0 or 1, -1 means draw
    float durationSec;
    std::array<float, 2> hpPercentLeft;
    std::array<float, 2> damageDealt;
};

// One 1v1 fight: input, combat rules, abilities, projectiles and rendering.
class Match {
public:
    Match(const CharacterDef& p1, const CharacterDef& p2, std::array<std::string, 2> playerNames);

    void update(float dt);
    void draw() const;

    bool finished() const { return over_ && endDelay_ <= 0.0f; }
    MatchResult result() const;

private:
    void handleInput(Fighter& self, Fighter& opponent);
    void meleeAttack(Fighter& attacker, Fighter& target);
    void useAbility(Fighter& user, Fighter& target, const AbilityDef& ability);
    void dealDamage(Fighter& attacker, Fighter& target, float amount, float stun, float poisonDps,
                    float poisonDuration);
    void separateFighters();
    void updateProjectiles(float dt);
    void updateEffects(float dt);
    void checkEnd();
    void spawnText(const std::string& text, Vector2 at, Color color);

    void drawFighter(const Fighter& f) const;
    void drawHud() const;

    std::array<Fighter, 2> fighters_;
    std::array<std::string, 2> names_;
    std::vector<Projectile> projectiles_;
    std::vector<FloatingText> texts_;
    std::vector<AreaEffect> effects_;

    float timeLeft_;
    float elapsed_ = 0.0f;
    bool over_ = false;
    float endDelay_ = 0.0f;
    int winner_ = -1;
    std::string banner_;
};
