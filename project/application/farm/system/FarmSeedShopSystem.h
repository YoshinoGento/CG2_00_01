#pragma once
#include "farm/system/FarmEconomySystem.h"
#include <limits>

// Owns only the shop interaction. Economy remains the sole purchase authority.
class FarmSeedShopSystem final {
public:
    static constexpr int kProductCount = farm::kPlayableCropCount;
    static constexpr int kMaxQuantity = 99;
    enum class Phase { Closed, Browsing, Confirming };
    enum class Notice { None, Purchased, Changed, Failed };
    struct Quote {
        farm::CropType crop = farm::CropType::None;
        int quantity = 0, price = 0, owned = 0, money = 0;
        long long total = 0;
        bool available = false;
        bool operator==(const Quote&) const = default;
    };
    [[nodiscard]] static farm::CropType Product(int index) noexcept {
        return farm::PlayableCrop(index);
    }
    void Open(farm::CropType crop) noexcept {
        selection_ = (std::max)(0, farm::ToCropSlot(crop) - 1);
        quantity_ = 1; phase_ = Phase::Browsing; notice_ = Notice::None; confirmation_ = {};
    }
    void Close() noexcept { phase_ = Phase::Closed; confirmation_ = {}; }
    [[nodiscard]] bool IsOpen() const noexcept { return phase_ != Phase::Closed; }
    [[nodiscard]] Phase GetPhase() const noexcept { return phase_; }
    [[nodiscard]] Notice GetNotice() const noexcept { return notice_; }
    [[nodiscard]] int Selection() const noexcept { return selection_; }
    [[nodiscard]] int Quantity() const noexcept { return quantity_; }
    bool Select(int index) noexcept {
        if (phase_ != Phase::Browsing || index < 0 || index >= kProductCount) return false;
        selection_ = index; notice_ = Notice::None; return true;
    }
    bool ChangeQuantity(int step) noexcept {
        if (phase_ != Phase::Browsing || (step != -1 && step != 1) ||
            quantity_ + step < 1 || quantity_ + step > kMaxQuantity) return false;
        quantity_ += step; notice_ = Notice::None; return true;
    }
    [[nodiscard]] Quote Evaluate(const FarmEconomySystem& economy) const noexcept {
        Quote quote;
        if (!IsOpen()) return quote;
        quote.crop = Product(selection_); quote.quantity = quantity_;
        quote.price = economy.GetSeedPrice(quote.crop); quote.owned = economy.GetSeedCount(quote.crop);
        quote.money = economy.GetMoney(); quote.total = static_cast<long long>(quote.price) * quantity_;
        quote.available = farm::IsPlantableCrop(quote.crop) && quote.price > 0 && quantity_ >= 1 && quantity_ <= kMaxQuantity &&
            quote.total <= (std::numeric_limits<int>::max)() && quote.total <= quote.money &&
            quote.owned <= (std::numeric_limits<int>::max)() - quantity_;
        return quote;
    }
    bool BeginConfirmation(const FarmEconomySystem& economy) noexcept {
        if (phase_ != Phase::Browsing) return false;
        const auto quote = Evaluate(economy);
        if (!quote.available) return false;
        confirmation_ = quote; phase_ = Phase::Confirming; notice_ = Notice::None; return true;
    }
    void CancelConfirmation() noexcept {
        if (phase_ == Phase::Confirming) { phase_ = Phase::Browsing; confirmation_ = {}; }
    }
    [[nodiscard]] FarmSeedPurchaseResult Confirm(FarmEconomySystem& economy) noexcept {
        if (phase_ != Phase::Confirming) return {};
        const auto quote = Evaluate(economy);
        phase_ = Phase::Browsing;
        if (!quote.available || quote != confirmation_) {
            confirmation_ = {}; notice_ = Notice::Changed; return {};
        }
        confirmation_ = {};
        const auto result = economy.BuySeed(quote.crop, quote.quantity);
        notice_ = result.Succeeded() ? Notice::Purchased : Notice::Failed;
        return result;
    }
private:
    Phase phase_ = Phase::Closed;
    Notice notice_ = Notice::None;
    int selection_ = 0, quantity_ = 1;
    Quote confirmation_{};
};
