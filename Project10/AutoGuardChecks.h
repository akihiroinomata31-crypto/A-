#pragma once
#include "JustGuard.h"
#include <cstdio>

inline bool CheckAutoGuard(FILE* report) {
    JustGuardState guard;
    guard.Update(true, true);
    bool passed = guard.window == 300 && guard.TryBlock(true) && guard.TryBlock(true) && guard.window == 300;
    for (int tick = 1; tick < JustGuardState::WindowTicks; ++tick) {
        // Release immediately, then press again during the effect: never refresh it.
        guard.Update(tick == 100, true);
        passed = passed && guard.window == JustGuardState::WindowTicks - tick && guard.TryBlock(true);
    }
    guard.Update(false, true);
    passed = passed && guard.window == 0 && !guard.TryBlock(true);
    guard.Update(true, true);
    passed = passed && guard.window == 300;
    for (int tick = 0; tick < JustGuardState::WindowTicks + 10; ++tick) guard.Update(true, true);
    passed = passed && guard.window == 0; // Holding cannot reactivate at expiry.
    guard.Update(false, true);
    guard.Update(true, true);
    guard.Update(false, false);
    passed = passed && guard.window == 299 && !guard.TryBlock(false);
    guard.Update(false, true);
    passed = passed && guard.window == 298 && guard.TryBlock(true);
    guard = {};
    guard.Update(true, false);
    guard.Update(true, true);
    passed = passed && guard.window == 0; // No queued activation from incapacity.
    if (report) fprintf(report, "automatic guard: five seconds, release persistence, repeated blocks, no refresh, expiry, fresh press and unavailable states: %s\n", passed ? "PASS" : "FAIL");
    return passed;
}
