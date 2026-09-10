#pragma once
#include <array>
#include <cstddef>

// One frame snapshot: short press/release pulses survive, multiple clicks coalesce.
class MouseButtonEdges final {
public:
    static constexpr int kCount = 5;
    void BeginFrame() noexcept { pressed_ = {}; released_ = {}; }
    void Reset() noexcept { held_ = {}; suppressed_ = {}; BeginFrame(); }
    // A button held during focus recovery must be released before gameplay can use it.
    void Synchronize(const std::array<bool, kCount>& held) noexcept { Reset(); suppressed_ = held; }
    void Apply(int index, bool down) noexcept {
        if (!Valid(index)) return;
        if (suppressed_[index]) { if (!down) suppressed_[index] = false; return; }
        if (held_[index] == down) return;
        held_[index] = down;
        if (down) pressed_[index] = true;
        else released_[index] = true;
    }
    [[nodiscard]] bool Held(int index) const noexcept { return Valid(index) && held_[index]; }
    [[nodiscard]] bool Pressed(int index) const noexcept { return Valid(index) && pressed_[index]; }
    [[nodiscard]] bool Released(int index) const noexcept { return Valid(index) && released_[index]; }
private:
    static bool Valid(int index) noexcept { return index >= 0 && index < kCount; }
    std::array<bool, kCount> held_{}, pressed_{}, released_{}, suppressed_{};
};
