#pragma once

// Fixed 60 Hz simulation: hold while moving to reach full speed in half a second.
struct PlayerSprintState {
    static constexpr int AccelerationTicks = 30;
    static constexpr float MaxMultiplier = 2.2f;
    int ticks = 0;
    void Reset() { ticks = 0; }
    float Multiplier() const {
        return 1.0f + (MaxMultiplier - 1.0f) * ticks / AccelerationTicks;
    }
    float Update(bool held, bool moving) {
        if (!held || !moving) Reset();
        else if (ticks < AccelerationTicks) ++ticks;
        return Multiplier();
    }
};
