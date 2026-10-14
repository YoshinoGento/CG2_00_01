#pragma once
#include <array>
#include <cstdint>

namespace scene {
enum class TransitionSoundTheme { Water, SeedShop, HarvestDisplay, ReturnToFarm, Ledger };
enum class TransitionSoundCue : std::uint32_t { WaterDrop, WaterSweep, WaterReveal, ShopEnter, DisplayEnter, FarmReturn, Ledger, Count };
using TransitionSoundMask = std::uint32_t;
inline constexpr std::size_t kTransitionSoundCount = static_cast<std::size_t>(TransitionSoundCue::Count);
static_assert(kTransitionSoundCount < 32);
inline constexpr TransitionSoundMask SoundBit(TransitionSoundCue cue) noexcept {
    const auto index = static_cast<std::uint32_t>(cue);
    return index < kTransitionSoundCount ? (TransitionSoundMask{1} << index) : 0;
}
inline constexpr std::array<const char*, kTransitionSoundCount> kTransitionSoundFiles{
    "water_drop.wav", "water_sweep.wav", "water_reveal.wav", "shop_enter.wav",
    "display_enter.wav", "farm_return.wav", "ledger.wav"};
inline constexpr float kTransitionSoundGain = .30f;
}
