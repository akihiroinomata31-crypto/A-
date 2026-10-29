#pragma once
#include "player.h"
#include "JustGuard.h"
#include <cmath>

namespace SprintKnockback {
constexpr float Reach = 130.0f;
constexpr float HalfWidth = 70.0f;
constexpr int DurationTicks = 18;
constexpr float Drag = 0.88f;

struct Release {
    bool active = false;
    VECTOR origin = {};
    VECTOR direction = {};
    float speed = 0.0f;
};

// Capture before locomotion consumes the button release and resets acceleration.
inline Release Capture(const SCharaInfo& player, const PlayerRuntimeState& state,
    bool held, bool available) {
    Release result;
    if (!available || held || !state.sprintHeld || state.sprint.ticks <= 0 || player.mode != RUN)
        return result;
    const float length = std::sqrt(player.move.x * player.move.x + player.move.z * player.move.z);
    if (length <= 0.001f) return result;
    result.active = true;
    result.origin = player.pos;
    result.direction = VGet(player.move.x / length, 0, player.move.z / length);
    result.speed = 16.0f + 8.0f * state.sprint.ticks / PlayerSprintState::AccelerationTicks;
    return result;
}

enum class Result { Miss, Blocked, Hit };

// Only ever selects the other player: mobs and bosses do not participate.
inline Result Resolve(const Release& release, int source, SCharaInfo* players,
    PlayerRuntimeState* states, JustGuardState* guards, bool canGuard, int damageAnimation) {
    if (!release.active || source < 0 || source >= PLAYER_COUNT) return Result::Miss;
    const int targetIndex = 1 - source;
    auto& attacker = players[source];
    auto& target = players[targetIndex];
    if (attacker.HP <= 0 ||
        target.mode == NONE || target.mode == DOWNMODE || target.HP <= 0 ||
        target.invincibleTimer > 0 || target.knockbackFrames > 0) return Result::Miss;
    if (target.pos.y > release.origin.y + attacker.charahitinfo.Height ||
        target.pos.y + target.charahitinfo.Height < release.origin.y) return Result::Miss;
    const float dx = target.pos.x - release.origin.x, dz = target.pos.z - release.origin.z;
    const float forward = dx * release.direction.x + dz * release.direction.z;
    const float sideways = dx * release.direction.z - dz * release.direction.x;
    if (forward < 0 || forward > Reach || std::fabs(sideways) > HalfWidth) return Result::Miss;
    if (guards[targetIndex].TryBlock(canGuard)) return Result::Blocked;
    CancelPlayerCombat(states[targetIndex]);
    ResetMove(target);
    SetCharacterAnimation(target, damageAnimation);
    target.mode = DAMAGE;
    target.knockbackVelocity = VScale(release.direction, release.speed);
    target.knockbackFrames = DurationTicks;
    guards[targetIndex].window = 0;
    return Result::Hit;
}

inline void Clear(SCharaInfo& player) {
    player.knockbackFrames = 0;
    player.knockbackVelocity = VGet(0, 0, 0);
}

// Feed horizontal movement through the existing floor/fall pipeline once per tick.
inline void Step(SCharaInfo& player) {
    if (player.mode == NONE || player.mode == DOWNMODE || player.HP <= 0 || player.invincibleTimer > 0) {
        Clear(player);
        return;
    }
    if (player.knockbackFrames <= 0) return;
    player.move.x = player.knockbackVelocity.x;
    player.move.z = player.knockbackVelocity.z;
    player.knockbackVelocity = VScale(player.knockbackVelocity, Drag);
    if (--player.knockbackFrames == 0) player.knockbackVelocity = VGet(0, 0, 0);
}
}
