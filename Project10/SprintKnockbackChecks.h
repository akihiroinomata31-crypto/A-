#pragma once
#include "SprintKnockback.h"
#include <cstdio>

// Exercises the same capture, hit resolution and per-tick movement as gameplay.
inline bool CheckSprintKnockback(FILE* report) {
    using namespace SprintKnockback;
    bool passed = true;
    for (int source = 0; source < PLAYER_COUNT; ++source) {
        SCharaInfo actors[3] = {};
        PlayerRuntimeState states[PLAYER_COUNT];
        const int target = 1 - source;
        for (auto& actor : actors) {
            actor.HP = 6;
            actor.mode = STAND;
            actor.pos = VGet(0, 0, 0);
            actor.move = VGet(0, 0, 0);
            actor.charahitinfo.Height = 180;
            actor.invincibleTimer = 0;
        }
        actors[source].mode = RUN;
        actors[source].move = VGet(11, 0, 0);
        actors[target].pos = VGet(80, 0, 0);
        actors[2].pos = VGet(40, 0, 0); // A mob between both players cannot intercept.
        states[source].sprintHeld = true;
        states[source].sprint.ticks = PlayerSprintState::AccelerationTicks;
        const auto release = Capture(actors[source], states[source], false, true);
        passed = passed && release.active && !Capture(actors[source], states[source], true, true).active &&
            !Capture(actors[source], states[source], false, false).active;
        states[source].sprint.Reset();
        passed = passed && !Capture(actors[source], states[source], false, true).active;
        states[source].sprintHeld = false; // Locomotion consumes the release.
        passed = passed && Resolve(release, source, actors, states, -1) == Result::Hit &&
            actors[target].HP == 6 && actors[target].mode == DAMAGE && actors[2].pos.x == 40 &&
            actors[2].mode == STAND && actors[2].knockbackFrames == 0 &&
            actors[target].knockbackFrames == DurationTicks;
        float travel = 0;
        for (int tick = 0; tick < DurationTicks; ++tick) {
            ResetMove(actors[target]);
            Step(actors[target]);
            travel += actors[target].move.x;
        }
        passed = passed && travel > 100 && actors[target].knockbackFrames == 0 &&
            actors[target].knockbackVelocity.x == 0;
        actors[target].mode = STAND;
        ResetMove(actors[target]);
        for (const VECTOR miss : { VGet(-40, 0, 0), VGet(Reach + 1, 0, 0),
                VGet(80, 0, HalfWidth + 1), VGet(80, 181, 0) }) {
            actors[target].pos = miss;
            passed = passed && Resolve(release, source, actors, states, -1) == Result::Miss;
        }
        actors[target].pos = VGet(80, 0, 0);
        actors[target].invincibleTimer = 60;
        passed = passed && Resolve(release, source, actors, states, -1) == Result::Miss;
        actors[target].knockbackFrames = 5;
        Step(actors[target]);
        passed = passed && actors[target].knockbackFrames == 0;
        actors[target].invincibleTimer = 0;
        actors[target].mode = DOWNMODE;
        passed = passed && Resolve(release, source, actors, states, -1) == Result::Miss &&
            Resolve(release, TEST_ENEMY_INDEX, actors, states, -1) == Result::Miss;
        if (report) fprintf(report, "P%d release knockback, players only, momentum, range/height, respawn and down: %s\n", source + 1, passed ? "PASS" : "FAIL");
    }
    SCharaInfo duel[PLAYER_COUNT] = {};
    PlayerRuntimeState duelStates[PLAYER_COUNT];
    Release releases[PLAYER_COUNT];
    for (int p = 0; p < PLAYER_COUNT; ++p) {
        duel[p].HP = 6; duel[p].mode = RUN;
        duel[p].pos = VGet(p * 80.0f, 0, 0);
        duel[p].move = VGet(p == 0 ? 11.0f : -11.0f, 0, 0);
        duel[p].charahitinfo.Height = 180;
        duel[p].invincibleTimer = 0;
        duelStates[p].sprintHeld = true;
        duelStates[p].sprint.ticks = 30;
        releases[p] = Capture(duel[p], duelStates[p], false, true);
    }
    const bool simultaneous = Resolve(releases[0], 0, duel, duelStates, -1) == Result::Hit &&
        Resolve(releases[1], 1, duel, duelStates, -1) == Result::Hit &&
        duel[0].knockbackVelocity.x < 0 && duel[1].knockbackVelocity.x > 0;
    passed = passed && simultaneous;
    if (report) fprintf(report, "simultaneous releases: %s\n", simultaneous ? "PASS" : "FAIL");
    return passed;
}
