#pragma once
#include "farm/system/FarmContestJudgeSystem.h"
#include "farm/system/FarmContestEntrySystem.h"

// Temporary browsing state only. Record pointers are never retained across inventory changes.
class FarmHarvestDisplaySystem final {
public:
    static constexpr int kSlots = 2;
    struct Snapshot {
        std::array<FarmEconomySystem::HarvestRecord, kSlots> records{};
        int count = 0, page = 0, pages = 1, selectedSlot = -1, unknownCount = 0;
        int reservationId = 0;
        std::uint64_t generation = 0;
        FarmContestJudgeResult judge{};
        FarmContestEntryResult entry{};
    };
    void Reset() noexcept { page_ = 0; selectedId_ = 0; }
    void MovePage(const FarmEconomySystem& economy, int step) noexcept {
        if (step != -1 && step != 1) return;
        const int next = std::clamp(page_ + step, 0, PageCount(economy) - 1);
        if (next != page_) { page_ = next; selectedId_ = 0; }
    }
    bool Select(const FarmEconomySystem& economy, int id, std::uint64_t generation) noexcept {
        if (id <= 0 || generation != economy.GetInventoryGeneration()) return false;
        for (int i = 0; i < kSlots; ++i) {
            const auto* record = economy.GetHarvestRecord(page_ * kSlots + i);
            if (record && record->id == id) { selectedId_ = id; return true; }
        }
        return false;
    }
    [[nodiscard]] Snapshot Observe(const FarmEconomySystem& economy, int day) noexcept {
        Snapshot result;
        result.pages = PageCount(economy);
        result.page = page_ = std::clamp(page_, 0, result.pages - 1);
        result.generation = economy.GetInventoryGeneration();
        result.reservationId = economy.GetContestReservationId();
        result.unknownCount = economy.GetUnrecordedCropCount();
        for (int i = 0; i < kSlots; ++i) {
            if (const auto* record = economy.GetHarvestRecord(page_ * kSlots + i)) {
                result.records[result.count++] = *record;
                if (record->id == selectedId_) result.selectedSlot = i;
            }
        }
        if (result.selectedSlot < 0 && result.count > 0) result.selectedSlot = 0;
        const auto* selected = result.selectedSlot >= 0 ? &result.records[result.selectedSlot] : nullptr;
        selectedId_ = selected ? selected->id : 0;
        result.judge = FarmContestJudgeSystem::Evaluate(selected);
        result.entry = FarmContestEntrySystem::Evaluate(day, selected);
        return result;
    }
private:
    static int PageCount(const FarmEconomySystem& economy) noexcept {
        return (std::max)(1, (static_cast<int>(economy.GetHarvestRecordCount()) + kSlots - 1) / kSlots);
    }
    int page_ = 0, selectedId_ = 0;
};
