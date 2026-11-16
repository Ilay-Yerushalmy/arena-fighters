#include "Match.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "Config.h"
#include "UI.h"

namespace {

constexpr Controls kP1Controls{KEY_A, KEY_D, KEY_W, KEY_S, KEY_F, KEY_G, KEY_H};
constexpr Controls kP2Controls{KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_I, KEY_O, KEY_P};
constexpr const char* kSkillKeyLabel[2] = {"G", "O"};
constexpr const char* kUltKeyLabel[2] = {"H", "P"};

constexpr float kMeleeCooldown = 0.4f;
constexpr float kMeleeHitstun = 0.15f;
constexpr float kEndDelay = 2.0f;

Vector2 centerOf(const Fighter& f) {
    return {f.pos.x, f.pos.y - Fighter::kHeight / 2.0f};
}

float distanceBetween(Vector2 a, Vector2 b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

float clampX(float x) {
    return std::clamp(x, Fighter::kWidth / 2.0f, config::kScreenWidth - Fighter::kWidth / 2.0f);
}

}  // namespace

Match::Match(const CharacterDef& p1, const CharacterDef& p2, std::array<std::string, 2> playerNames)
    : fighters_{{Fighter(p1, 0, 320.0f, 1, kP1Controls), Fighter(p2, 1, 960.0f, -1, kP2Controls)}},
      names_(std::move(playerNames)),
      timeLeft_(config::kMatchTimeSeconds) {}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void Match::update(float dt) {
    updateEffects(dt);
    if (over_) {
        endDelay_ -= dt;
        return;
    }

    elapsed_ += dt;
    timeLeft_ = std::max(0.0f, timeLeft_ - dt);

    for (int i = 0; i < 2; ++i) {
        Fighter& self = fighters_[i];
        Fighter& other = fighters_[1 - i];
        self.facing = other.pos.x >= self.pos.x ? 1 : -1;
        handleInput(self, other);
    }
    for (Fighter& f : fighters_) {
        f.updateTimers(dt);
        f.updatePhysics(dt);
    }
    separateFighters();
    updateProjectiles(dt);
    checkEnd();
}

void Match::handleInput(Fighter& self, Fighter& opponent) {
    const Controls& c = self.controls;
    self.blocking = false;

    if (!self.isAlive() || self.isStunned()) {
        self.vel.x = 0.0f;
        return;
    }
    if (IsKeyDown(c.block) && self.onGround) {
        self.blocking = true;
        self.vel.x = 0.0f;
        return;
    }

    float dir = 0.0f;
    if (IsKeyDown(c.left)) dir -= 1.0f;
    if (IsKeyDown(c.right)) dir += 1.0f;
    self.vel.x = dir * self.def->moveSpeed * self.speedMultiplier();

    if (IsKeyPressed(c.jump) && self.onGround) {
        self.vel.y = config::kJumpVelocity;
        self.onGround = false;
    }

    if (IsKeyPressed(c.attack) && self.attackCooldown <= 0.0f) {
        meleeAttack(self, opponent);
    }

    // Time-restricted skill: gated by a cooldown timer.
    if (IsKeyPressed(c.skill) && self.skillCooldown <= 0.0f) {
        useAbility(self, opponent, self.def->skill);
        self.skillCooldown = self.def->skill.cooldown;
    }

    // Resource-based ultimate: gated by a full energy bar.
    if (IsKeyPressed(c.ultimate) && self.energy >= config::kMaxEnergy) {
        self.energy = 0.0f;
        useAbility(self, opponent, self.def->ultimate);
    }
}

void Match::meleeAttack(Fighter& attacker, Fighter& target) {
    attacker.attackCooldown = kMeleeCooldown / attacker.speedMultiplier();
    attacker.attackFlash = 0.12f;
    if (CheckCollisionRecs(attacker.meleeHitbox(), target.hurtbox())) {
        dealDamage(attacker, target, attacker.def->attackDamage, kMeleeHitstun, 0.0f, 0.0f);
    }
}

void Match::useAbility(Fighter& user, Fighter& target, const AbilityDef& a) {
    spawnText(a.name, {user.pos.x, user.pos.y - Fighter::kHeight - 60.0f}, GOLD);

    switch (a.kind) {
        case AbilityKind::Projectile: {
            for (int i = 0; i < a.count; ++i) {
                Projectile p{};
                const float spread = (static_cast<float>(i) - (a.count - 1) / 2.0f) * 40.0f;
                p.pos = {user.pos.x + user.facing * 40.0f, user.pos.y - 70.0f + spread};
                p.vel = {user.facing * a.speed, 0.0f};
                p.radius = a.radius;
                p.damage = a.damage;
                p.stun = a.stun;
                p.poisonDps = a.poisonDps;
                p.poisonDuration = a.poisonDuration;
                p.owner = user.playerIndex;
                p.color = user.def->color;
                p.alive = true;
                projectiles_.push_back(p);
            }
            break;
        }
        case AbilityKind::Dash: {
            const float startX = user.pos.x;
            const float endX = clampX(startX + user.facing * a.speed);
            const float minX = std::min(startX, endX) - Fighter::kWidth / 2.0f;
            const float maxX = std::max(startX, endX) + Fighter::kWidth / 2.0f;
            const bool sameHeight = std::fabs(user.pos.y - target.pos.y) < Fighter::kHeight;
            user.pos.x = endX;
            effects_.push_back({{(startX + endX) / 2.0f, user.pos.y - 60.0f},
                                std::fabs(endX - startX) / 2.0f, 0.25f, 0.25f, user.def->color});
            if (sameHeight && target.pos.x >= minX && target.pos.x <= maxX) {
                dealDamage(user, target, a.damage, a.stun, 0.0f, 0.0f);
            }
            break;
        }
        case AbilityKind::Teleport: {
            // "Behind" is the side the target is not facing.
            user.pos.x = clampX(target.pos.x - target.facing * 90.0f);
            user.pos.y = target.pos.y;
            effects_.push_back({centerOf(user), 70.0f, 0.3f, 0.3f, user.def->color});
            break;
        }
        case AbilityKind::Heal: {
            user.heal(a.magnitude);
            spawnText(TextFormat("+%d", static_cast<int>(a.magnitude)), centerOf(user), GREEN);
            break;
        }
        case AbilityKind::AreaBlast: {
            effects_.push_back({centerOf(user), std::min(a.radius, 700.0f), 0.5f, 0.5f, user.def->color});
            if (distanceBetween(centerOf(user), centerOf(target)) <= a.radius + Fighter::kWidth / 2.0f) {
                dealDamage(user, target, a.damage, a.stun, a.poisonDps, a.poisonDuration);
            }
            break;
        }
        case AbilityKind::Buff: {
            user.addBuff(a.buff, a.buffDuration, a.magnitude);
            break;
        }
        case AbilityKind::Flurry: {
            const bool inFront = (target.pos.x - user.pos.x) * user.facing >= 0.0f;
            if (inFront && distanceBetween(centerOf(user), centerOf(target)) <= a.radius) {
                for (int i = 0; i < a.count; ++i) {
                    dealDamage(user, target, a.damage, 0.1f, 0.0f, 0.0f);
                }
            }
            effects_.push_back({centerOf(user), a.radius, 0.35f, 0.35f, user.def->color});
            break;
        }
    }
}

void Match::dealDamage(Fighter& attacker, Fighter& target, float amount, float stun, float poisonDps,
                       float poisonDuration) {
    const float dealt = target.takeDamage(amount * attacker.damageMultiplier());
    if (dealt > 0.0f) {
        attacker.addEnergy(config::kEnergyOnHitDealt);
        attacker.damageDealt += dealt;
        spawnText(TextFormat("-%d", static_cast<int>(std::ceil(dealt))), centerOf(target), RED);
    } else {
        spawnText("BLOCKED", centerOf(target), SKYBLUE);
    }
    target.applyStatus(stun, poisonDps, poisonDuration);
}

void Match::separateFighters() {
    Fighter& a = fighters_[0];
    Fighter& b = fighters_[1];
    const float dx = b.pos.x - a.pos.x;
    const float overlap = Fighter::kWidth - std::fabs(dx);
    const bool verticalOverlap = std::fabs(a.pos.y - b.pos.y) < Fighter::kHeight * 0.8f;
    if (overlap > 0.0f && verticalOverlap) {
        const float push = overlap / 2.0f * (dx >= 0.0f ? 1.0f : -1.0f);
        a.pos.x = clampX(a.pos.x - push);
        b.pos.x = clampX(b.pos.x + push);
    }
}

void Match::updateProjectiles(float dt) {
    for (Projectile& p : projectiles_) {
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;

        Fighter& target = fighters_[1 - p.owner];
        if (CheckCollisionCircleRec(p.pos, p.radius, target.hurtbox())) {
            dealDamage(fighters_[p.owner], target, p.damage, p.stun, p.poisonDps, p.poisonDuration);
            p.alive = false;
        }
        if (p.pos.x < -50.0f || p.pos.x > config::kScreenWidth + 50.0f) p.alive = false;
    }
    std::erase_if(projectiles_, [](const Projectile& p) { return !p.alive; });
}

void Match::updateEffects(float dt) {
    for (FloatingText& t : texts_) {
        t.pos.y -= 40.0f * dt;
        t.ttl -= dt;
    }
    std::erase_if(texts_, [](const FloatingText& t) { return t.ttl <= 0.0f; });

    for (AreaEffect& e : effects_) e.ttl -= dt;
    std::erase_if(effects_, [](const AreaEffect& e) { return e.ttl <= 0.0f; });
}

void Match::checkEnd() {
    const Fighter& a = fighters_[0];
    const Fighter& b = fighters_[1];
    const bool knockout = !a.isAlive() || !b.isAlive();
    if (!knockout && timeLeft_ > 0.0f) return;

    over_ = true;
    endDelay_ = kEndDelay;
    banner_ = knockout ? "K.O.!" : "TIME!";

    const float pa = a.hp / a.def->maxHp;
    const float pb = b.hp / b.def->maxHp;
    if (std::fabs(pa - pb) < 1e-4f) {
        winner_ = -1;
    } else {
        winner_ = pa > pb ? 0 : 1;
    }
}

void Match::spawnText(const std::string& text, Vector2 at, Color color) {
    texts_.push_back({text, at, 0.9f, color});
}

MatchResult Match::result() const {
    MatchResult r{};
    r.winnerIndex = winner_;
    r.durationSec = elapsed_;
    for (int i = 0; i < 2; ++i) {
        r.hpPercentLeft[i] = 100.0f * fighters_[i].hp / fighters_[i].def->maxHp;
        r.damageDealt[i] = fighters_[i].damageDealt;
    }
    return r;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void Match::draw() const {
    const int ground = static_cast<int>(config::kGroundY);
    DrawRectangleGradientV(0, 0, config::kScreenWidth, ground, Color{30, 34, 60, 255}, Color{90, 60, 110, 255});
    DrawRectangle(0, ground, config::kScreenWidth, config::kScreenHeight - ground, Color{40, 40, 46, 255});
    DrawLine(0, ground, config::kScreenWidth, ground, Color{120, 120, 140, 255});

    for (const AreaEffect& e : effects_) {
        const float alpha = e.ttl / e.maxTtl;
        DrawCircleV(e.center, e.radius, Fade(e.color, 0.25f * alpha));
        DrawCircleLines(static_cast<int>(e.center.x), static_cast<int>(e.center.y), e.radius, Fade(e.color, alpha));
    }

    for (const Fighter& f : fighters_) drawFighter(f);

    for (const Projectile& p : projectiles_) {
        DrawCircleV(p.pos, p.radius, p.color);
        DrawCircleLines(static_cast<int>(p.pos.x), static_cast<int>(p.pos.y), p.radius, RAYWHITE);
    }

    for (const FloatingText& t : texts_) {
        const int w = MeasureText(t.text.c_str(), 22);
        DrawText(t.text.c_str(), static_cast<int>(t.pos.x) - w / 2, static_cast<int>(t.pos.y), 22,
                 Fade(t.color, std::min(1.0f, t.ttl * 2.0f)));
    }

    drawHud();

    if (over_) {
        ui::drawCenteredText(banner_, 250, 96, GOLD);
        const std::string sub = winner_ == -1 ? "DRAW" : names_[winner_] + " WINS";
        ui::drawCenteredText(sub, 360, 40, RAYWHITE);
    }
}

void Match::drawFighter(const Fighter& f) const {
    const Rectangle body = f.hurtbox();
    const Color color = f.isStunned() ? Fade(f.def->color, 0.5f) : f.def->color;

    DrawRectangleRec(body, color);
    DrawRectangleLinesEx(body, 2.0f, BLACK);
    DrawCircleV({f.pos.x, body.y - 18.0f}, 18.0f, color);
    DrawCircleV({f.pos.x + f.facing * 7.0f, body.y - 22.0f}, 4.0f, RAYWHITE);

    if (f.attackFlash > 0.0f) DrawRectangleRec(f.meleeHitbox(), Fade(RAYWHITE, 0.5f));

    if (f.blocking) {
        const float x = f.facing > 0 ? body.x + body.width : body.x - 10.0f;
        DrawRectangleRec({x, body.y + 10.0f, 10.0f, body.height - 20.0f}, SKYBLUE);
    }
    if (f.hasBuff(BuffKind::Shield)) {
        const Vector2 c = centerOf(f);
        DrawCircleV(c, 85.0f, Fade(SKYBLUE, 0.2f));
        DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 85.0f, SKYBLUE);
    }
    if (f.hasBuff(BuffKind::DamageUp)) {
        DrawRectangleLinesEx({body.x - 6, body.y - 42, body.width + 12, body.height + 42}, 3.0f, RED);
    }
    if (f.hasBuff(BuffKind::Haste)) {
        for (int i = 0; i < 3; ++i) {
            const float y = body.y + 25.0f + i * 30.0f;
            DrawLineEx({body.x - f.facing * 15.0f, y}, {body.x - f.facing * 45.0f, y}, 3.0f, YELLOW);
        }
    }
    if (f.hasBuff(BuffKind::Regen)) {
        DrawText("+", static_cast<int>(body.x + body.width + 6), static_cast<int>(body.y), 30, GREEN);
    }
    if (f.poisonTimer > 0.0f) {
        for (int i = 0; i < 4; ++i) {
            DrawCircleV({body.x + 10.0f + i * 13.0f, body.y + body.height - 10.0f}, 4.0f, LIME);
        }
    }
    if (f.isStunned()) {
        const int w = MeasureText("STUNNED", 18);
        DrawText("STUNNED", static_cast<int>(f.pos.x) - w / 2, static_cast<int>(body.y) - 70, 18, SKYBLUE);
    }
}

void Match::drawHud() const {
    constexpr int barW = 460;
    constexpr int margin = 30;

    for (int i = 0; i < 2; ++i) {
        const Fighter& f = fighters_[i];
        const int x = i == 0 ? margin : config::kScreenWidth - margin - barW;
        const bool rightAligned = i == 1;

        auto drawLabel = [&](const std::string& text, int y, int size, Color color) {
            const int w = MeasureText(text.c_str(), size);
            DrawText(text.c_str(), rightAligned ? x + barW - w : x, y, size, color);
        };

        drawLabel(names_[i] + "  -  " + f.def->name, 14, 22, RAYWHITE);

        ui::drawBar(x, 42, barW, 26, f.hp / f.def->maxHp, f.hp / f.def->maxHp > 0.3f ? GREEN : RED, DARKGRAY);
        drawLabel(TextFormat("%d / %d", static_cast<int>(std::ceil(f.hp)), static_cast<int>(f.def->maxHp)), 46,
                  18, BLACK);

        const bool ultReady = f.energy >= config::kMaxEnergy;
        ui::drawBar(x, 74, barW, 12, f.energy / config::kMaxEnergy, ultReady ? GOLD : ORANGE, DARKGRAY);

        const std::string skillState =
            f.skillCooldown <= 0.0f ? std::string("READY") : TextFormat("%.1fs", f.skillCooldown);
        drawLabel(f.def->skill.name + " [" + kSkillKeyLabel[i] + "]: " + skillState, 92, 18, LIGHTGRAY);
        drawLabel(f.def->ultimate.name + " [" + kUltKeyLabel[i] + "]: " + (ultReady ? "READY" : "charging"), 114,
                  18, ultReady ? GOLD : GRAY);
    }

    ui::drawCenteredText(TextFormat("%d", static_cast<int>(std::ceil(timeLeft_))), 20, 52, RAYWHITE);
}
