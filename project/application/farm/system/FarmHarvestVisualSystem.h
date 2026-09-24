#pragma once
#include "farm/render/FarmMeshLayout.h"
#include <array>

namespace farm {
// Transient presentation only. Inventory is already updated before Start is called.
class FarmHarvestVisualSystem final {
public:
    static constexpr std::size_t kCapacity = 8;
    static constexpr float kPullSeconds = 0.30f;
    static constexpr float kHoldSeconds = 0.90f;
    static constexpr float kShrinkSeconds = 0.40f;
    static constexpr float kDuration = kPullSeconds + kHoldSeconds + kShrinkSeconds;

    bool Start(const FarmToolActionResult& result, const FarmGrid& grid, const FarmVisualSystem& visualSystem) noexcept {
        if (result.status != FarmToolActionStatus::Harvested || !result.harvestedTile ||
            !IsHarvestReady(*result.harvestedTile)) return false;
        const auto* current = grid.GetTile(result.tileIndex);
        const auto& before = *result.harvestedTile;
        if (!current || current->state != FarmTileState::Tilled || current->crop != CropType::None ||
            current->feature != FarmTileFeature::None || current->heightLevel != before.heightLevel) return false;
        auto visual = visualSystem.GetTileVisualData(grid, result.tileIndex);
        visual.crop = before.crop;
        visual.cropStage = FarmCropGrowthStage::Ready;
        visual.cropScale = visualSystem.GetCropRenderScale(before);
        auto meshes = BuildFarmCropMeshParts(visual, before.growth);
        if (meshes.count != 2) return false;
        for (const auto& mesh : meshes.parts) if (!IsValidPart(mesh)) return false;
        // Fruit crops show only the picked fruit; roots retain their attached foliage.
        if (before.crop == CropType::Tomato || before.crop == CropType::Pumpkin) meshes.count = 1;
        std::size_t slot = kCapacity;
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            if (Matches(entries_[i], grid) && entries_[i].tileIndex == result.tileIndex) return false;
            if (slot == kCapacity && !Matches(entries_[i], grid)) slot = i;
        }
        if (slot == kCapacity) {
            slot = 0;
            for (std::size_t i = 1; i < entries_.size(); ++i)
                if (entries_[i].age > entries_[slot].age) slot = i;
        }
        entries_[slot] = {meshes, grid.GetGeneration(), result.tileIndex, before.heightLevel, 0.0f,
            (std::max)(0.16f, visual.center.y - meshes.parts[0].position.y + 0.16f), true};
        return true;
    }

    void Clear() noexcept { entries_ = {}; }
    void Update(const FarmGrid& grid, float deltaTime) noexcept {
        const float step = std::isfinite(deltaTime) && deltaTime > 0 ? (std::min)(deltaTime, kDuration) : 0;
        for (auto& entry : entries_) {
            if (!Matches(entry, grid)) { entry.active = false; continue; }
            entry.age += step;
            if (entry.age >= kDuration) entry.active = false;
        }
    }
    [[nodiscard]] std::size_t GetActiveCount(const FarmGrid& grid) const noexcept {
        std::size_t count = 0;
        for (const auto& entry : entries_) count += Matches(entry, grid) ? 1 : 0;
        return count;
    }
    [[nodiscard]] FarmCropMeshParts GetParts(const FarmGrid& grid, std::size_t index) const noexcept {
        if (index >= entries_.size() || !Matches(entries_[index], grid)) return {};
        const auto& entry = entries_[index];
        auto parts = entry.meshes;
        const float t = std::clamp(entry.age / kPullSeconds, 0.0f, 1.0f);
        const float lift = entry.lift * (1.0f - (1.0f - t) * (1.0f - t));
        const float shrink = 1.0f - std::clamp((entry.age - kPullSeconds - kHoldSeconds) / kShrinkSeconds, 0.0f, 1.0f);
        const auto pivot = entry.meshes.parts[0].position;
        for (std::size_t i=0; i<parts.count; ++i) {
            auto& part = parts.parts[i];
            part.position = {pivot.x + (part.position.x - pivot.x) * shrink,
                pivot.y + lift + (part.position.y - pivot.y) * shrink,
                pivot.z + (part.position.z - pivot.z) * shrink};
            part.scale = {part.scale.x * shrink, part.scale.y * shrink, part.scale.z * shrink};
            if (!IsValidPart(part)) return {}; // Never pass a singular scale to the renderer.
        }
        return parts;
    }
private:
    struct Entry {
        FarmCropMeshParts meshes{};
        uint64_t generation = 0;
        int tileIndex = -1, height = 0;
        float age = 0, lift = 0;
        bool active = false;
    };
    static bool IsValidPart(const FarmMeshPart& part) noexcept {
        return std::isfinite(part.position.x) && std::isfinite(part.position.y) && std::isfinite(part.position.z) &&
            std::isfinite(part.scale.x) && std::isfinite(part.scale.y) && std::isfinite(part.scale.z) &&
            part.scale.x >= 0.0001f && part.scale.y >= 0.0001f && part.scale.z >= 0.0001f;
    }
    static bool Matches(const Entry& entry, const FarmGrid& grid) noexcept {
        if (!entry.active || entry.generation != grid.GetGeneration()) return false;
        const auto* tile = grid.GetTile(entry.tileIndex);
        return tile && tile->crop == CropType::None && tile->feature == FarmTileFeature::None &&
            tile->state == FarmTileState::Tilled && tile->heightLevel == entry.height;
    }
    std::array<Entry, kCapacity> entries_{};
};
} // namespace farm
