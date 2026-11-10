#pragma once
#include <cmath>
#include "GameTiming.h"

struct OrbitCamera {
    static constexpr float DefaultPitch = 0.306277f;
    float yaw = 0.0f;
    float pitch = DefaultPitch;
    float yawVelocity = 0.0f;
    float pitchVelocity = 0.0f;

    // DxLib normalizes right-stick axes to -1000..1000, with down positive.
    float Update(int rawX, int rawY) {
        float x = rawX / 1000.0f, y = rawY / 1000.0f;
        const float magnitude = std::sqrt(x * x + y * y);
        constexpr float deadzone = 0.18f;
        const bool released = magnitude <= deadzone;
        if (released) {
            x = y = 0.0f;
        } else {
            const float strength = ((magnitude < 1.0f ? magnitude : 1.0f) - deadzone) / (1.0f - deadzone);
            x = x / magnitude * strength;
            y = y / magnitude * strength;
        }
        // Ease into turns; brake faster on release to keep aiming responsive.
        const float responseSeconds = released ? 0.05f : 0.08f;
        const float blend = 1.0f - std::exp(-1.0f / (FixedStepClock::Frequency * responseSeconds));
        yawVelocity += (x * 2.4f - yawVelocity) * blend;
        pitchVelocity += (y * 1.4f - pitchVelocity) * blend;
        if (released && std::fabs(yawVelocity) < 0.001f) yawVelocity = 0.0f;
        if (released && std::fabs(pitchVelocity) < 0.001f) pitchVelocity = 0.0f;
        const float deltaYaw = yawVelocity / FixedStepClock::Frequency;
        yaw = std::remainder(yaw + deltaYaw, 6.283185307f);
        pitch += pitchVelocity / FixedStepClock::Frequency;
        if (pitch < 0.12f) { pitch = 0.12f; pitchVelocity = 0.0f; }
        if (pitch > 1.15f) { pitch = 1.15f; pitchVelocity = 0.0f; }
        return deltaYaw;
    }
};
