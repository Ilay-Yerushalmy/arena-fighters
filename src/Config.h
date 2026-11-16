#pragma once

#include <cstdlib>
#include <string>

// Global tuning values. Keeping them in one place makes balancing easy.
namespace config {

constexpr int kScreenWidth = 1280;
constexpr int kScreenHeight = 720;
constexpr float kGroundY = 600.0f;

constexpr float kGravity = 2200.0f;
constexpr float kJumpVelocity = -850.0f;

constexpr float kMatchTimeSeconds = 99.0f;

// Combat resource ("energy") - fills up during the fight, a full bar unlocks the ultimate.
constexpr float kMaxEnergy = 100.0f;
constexpr float kEnergyOnHitDealt = 12.0f;
constexpr float kEnergyOnHitTaken = 8.0f;
constexpr float kEnergyPerSecond = 1.5f;

constexpr float kBlockDamageMultiplier = 0.25f;

// Root folder that contains db/ and ranking/.
inline std::string projectRoot() {
    if (const char* env = std::getenv("ARENA_ROOT")) {
        return env;
    }
#ifdef ARENA_DEFAULT_ROOT
    return ARENA_DEFAULT_ROOT;
#else
    return ".";
#endif
}

}  // namespace config
