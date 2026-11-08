#pragma once

// Fixed 60 Hz simulation. One press grants five seconds of automatic blocking.
struct JustGuardState {
    static constexpr int WindowTicks = 5 * 60;
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
        if (available && down && !held && window == 0 && cooldown == 0) {
            window = WindowTicks;
            cooldown = CooldownTicks;
        }
        held = down;
    }

    bool TryBlock(bool available) {
        if (!available || window <= 0) return false;
        feedback = FeedbackTicks;
        return true;
    }
};
