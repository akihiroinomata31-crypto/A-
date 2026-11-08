#pragma once
#include <cmath>
#include "GameTiming.h"

struct OrbitCamera {
    static constexpr float DefaultPitch = 0.306277f;
    float yaw = 0.0f;
    float pitch = DefaultPitch;

    // DxLib normalizes right-stick axes to -1000..1000, with down positive.
    float Update(int rawX, int rawY) {
        float x = rawX / 1000.0f, y = rawY / 1000.0f;
        const float magnitude = std::sqrt(x * x + y * y);
        constexpr float deadzone = 0.18f;
        if (magnitude <= deadzone) return 0.0f;
        const float strength = ((magnitude < 1.0f ? magnitude : 1.0f) - deadzone) / (1.0f - deadzone);
        x = x / magnitude * strength;
        y = y / magnitude * strength;
        const float deltaYaw = x * 2.4f / FixedStepClock::Frequency;
        yaw = std::remainder(yaw + deltaYaw, 6.283185307f);
        pitch += y * 1.4f / FixedStepClock::Frequency;
        if (pitch < 0.12f) pitch = 0.12f;
        if (pitch > 1.15f) pitch = 1.15f;
        return deltaYaw;
    }
};
