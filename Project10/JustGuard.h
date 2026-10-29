#pragma once

// Fixed 60 Hz simulation. A fresh press opens a short, single-hit window.
struct JustGuardState {
    static constexpr int WindowTicks = 24;
    static constexpr int CooldownTicks = 30;
    static constexpr int FeedbackTicks = 30;
    static constexpr int SuccessPoseTicks = 18;
    int window = 0;
    int cooldown = 0;
    int feedback = 0;
    bool held = false;

    bool PoseActive() const {
        return window > 0 || feedback > FeedbackTicks - SuccessPoseTicks;
    }

    void Update(bool down, bool available) {
        if (window > 0) --window;
        if (cooldown > 0) --cooldown;
        if (feedback > 0) --feedback;
        if (!available || !down) window = 0;
        if (available && down && !held && cooldown == 0) {
            window = WindowTicks;
            cooldown = CooldownTicks;
        }
        held = down;
    }

    bool TryBlock(bool available) {
        if (!available || window <= 0) return false;
        window = 0;
        feedback = FeedbackTicks;
        return true;
    }
};
