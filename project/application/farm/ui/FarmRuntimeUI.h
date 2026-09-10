#pragma once
#include "farm/ui/FarmRuntimeLabels.h"
#include "2d/Sprite.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <string>

namespace farmui {
enum class Action {
    None, Menu, Close, Tab, Tool, Crop, Buy, Sell, SellAll,
    Raise, Lower, Canal, Source, Path, RemovePath, Confirm, Cancel, Undo, Redo,
    Save, SaveCopy, Load, Previous, Next, Restart, Accept,
    PinA, PinB, Start, Stop, Clear, Pause, Speed, Follow, Overview, Exit, LayoutLibrary,
    ObserveField, ObserveExit, PickSlot, JumpSlot, FlowContinue, FlowRecords,
    ApplyTool, OpenSeedShop, BuyAndReturn, SoilCare, Compost, Quality, TerrainField, TerrainExit
};
struct Request { Action action = Action::None; int argument = 0; };
struct Rect {
    float x = 0, y = 0, width = 0, height = 0;
    [[nodiscard]] bool Contains(Vector2 p) const noexcept {
        return p.x >= x && p.y >= y && p.x < x + width && p.y < y + height;
    }
};
struct Item {
    Label label = Label::Menu;
    Rect rect{};
    Request request{};
    bool enabled = true;
    bool selected = false;
    std::string value;
    bool focused = false;
    float valueOffset = 540;
};
struct QualityRadar {
    bool visible = false;
    std::array<float, 4> values{};
    std::array<bool, 4> known{};
    inline static constexpr Vector2 center{366, 340};
    inline static constexpr float radius = 110;
    inline static constexpr std::array<Vector2, 4> directions{{{0,-1}, {1,0}, {0,1}, {-1,0}}};
    [[nodiscard]] static Vector2 Point(std::size_t axis, float value) noexcept {
        if (axis >= directions.size() || !std::isfinite(value)) return center;
        const float distance = radius * std::clamp(value, 0.0f, 1.0f);
        return {center.x + directions[axis].x * distance, center.y + directions[axis].y * distance};
    }
};
struct View {
    static constexpr std::size_t kCapacity = 40;
    std::array<Item, kCapacity> items{};
    std::size_t count = 0;
    bool modal = false;
    bool observation = false;
    bool terrain = false;
    bool fieldActions = false;
    QualityRadar radar{};
    inline static constexpr Rect kFieldActionsPanel{1040, 532, 216, 112};
    inline static constexpr Rect kObservationTop{16, 16, 1248, 52};
    inline static constexpr Rect kObservationBottom{16, 408, 1248, 280};
    inline static constexpr Rect kTerrainTop{16, 16, 1248, 96};
    inline static constexpr Rect kTerrainBottom{16, 520, 1248, 184};
    inline static constexpr std::array<Rect, 4> kFarmHudPanels{{
        {24, 24, 310, 132}, {876, 24, 380, 228}, {24, 520, 350, 176}, {390, 548, 628, 148}
    }};
    std::array<std::string, 2> recordNames{};
    void Add(Label label, Rect rect, Request request = {}, bool enabled = true, bool selected = false) {
        if (count < items.size()) items[count++] = {label, rect, request, enabled, selected, {}};
    }
    void Value(Label label, float y, std::string value) {
        if (count < items.size()) items[count++] = {label, {176, y, 900, 38}, {}, true, false, std::move(value)};
    }
    void Metric(Label label, Rect rect, float offset, std::string value) {
        if (count >= items.size()) return;
        items[count++] = {label, rect, {}, true, false, std::move(value), false, offset};
    }
    [[nodiscard]] bool Covers(Vector2 point) const noexcept {
        if (modal || (observation && (kObservationTop.Contains(point) || kObservationBottom.Contains(point))) ||
            (terrain && (kTerrainTop.Contains(point) || kTerrainBottom.Contains(point))) ||
            (fieldActions && kFieldActionsPanel.Contains(point))) return true;
        // Disabled controls and labels still own their screen area.
        for (std::size_t i = 0; i < count && i < items.size(); ++i)
            if (items[i].rect.Contains(point)) return true;
        return false;
    }
    [[nodiscard]] Request Hit(Vector2 point) const noexcept {
        for (std::size_t i = count < items.size() ? count : items.size(); i > 0; --i) {
            const auto& item = items[i - 1];
            if (item.enabled && item.request.action != Action::None && item.rect.Contains(point)) return item.request;
        }
        return {};
    }
    [[nodiscard]] static bool FarmHUDCovers(Vector2 point) noexcept {
        for (const auto& panel : kFarmHudPanels) if (panel.Contains(point)) return true;
        return false;
    }
    [[nodiscard]] int NextActionable(int current, int direction) const noexcept {
        const int size = static_cast<int>(count < items.size() ? count : items.size());
        if (size == 0 || direction == 0) return -1;
        int index = current >= 0 && current < size ? current : (direction > 0 ? size - 1 : 0);
        for (int i = 0; i < size; ++i) {
            index = (index + (direction > 0 ? 1 : size - 1)) % size;
            if (items[index].enabled && items[index].request.action != Action::None) return index;
        }
        return -1;
    }
};

// Presentation only. Pools are allocated once; every submitted quad has distinct constants.
class RuntimeUI final {
public:
    bool Initialize(SpriteCommon* common);
    void PrepareNames(const View& view);
    void Draw(const View& view, Vector2 pointer);
private:
    void Panel(Rect rect, Vector4 color);
    void LabelQuad(Label label, Rect rect, Vector4 color);
    void ValueText(const std::string& value, Vector2 position, float right);
    void DrawRadar(const QualityRadar& radar);
    std::array<Sprite, 42> panels_{};
    std::array<Sprite, 40> labels_{};
    std::array<Sprite, 160> digits_{};
    std::array<Sprite, 28> radarLines_{};
    Sprite names_;
    std::array<std::string, 2> cachedNames_{};
    bool namesReady_ = false;
    std::size_t panelCount_ = 0, labelCount_ = 0, digitCount_ = 0;
    bool ready_ = false;
};
}
