#pragma once

// Advance game logic at 60 Hz independently of display refresh and rendering cost.
class FixedStepClock {
    long long lastMicroseconds_ = 0;
    long long pending_ = 0;
    bool initialized_ = false;
public:
    static constexpr int Frequency = 60;
    void Reset(long long now) { lastMicroseconds_ = now; pending_ = 0; initialized_ = true; }
    int Advance(long long now) {
        if (!initialized_) { Reset(now); return 0; }
        long long elapsed = now - lastMicroseconds_;
        lastMicroseconds_ = now;
        if (elapsed < 0) elapsed = 0;
        // A debugger pause must not cause seconds of blocked input on resuming.
        if (elapsed > 250000) elapsed = 250000;
        pending_ += elapsed * Frequency;
        int steps = static_cast<int>(pending_ / 1000000);
        if (steps > 8) steps = 8;
        pending_ -= steps * 1000000LL;
        return steps;
    }
};

// Animation units per second, converted to the fixed simulation frequency.
inline constexpr float AnimationStep(float unitsPerSecond) { return unitsPerSecond / FixedStepClock::Frequency; }
inline constexpr float PlayerAnimationStep = AnimationStep(60.0f);
inline constexpr float LegacyPlayerAnimationStep = AnimationStep(36.0f);
inline constexpr float EnemyAnimationStep = AnimationStep(24.0f);
