#pragma once
#include "farm/ui/FarmRuntimeLabels.h"
#include "2d/Sprite.h"
#include <array>
#include <string>

namespace farmui {
enum class Action {
    None, Menu, Close, Tab, Tool, Crop, Buy, Sell, SellAll,
    Raise, Lower, Canal, Source, Path, RemovePath, Confirm, Cancel, Undo, Redo,
    Save, SaveCopy, Load, Previous, Next, Restart, Accept,
    PinA, PinB, Start, Stop, Clear, Pause, Speed, Follow, Overview, Exit, LayoutLibrary
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
};
struct View {
    static constexpr std::size_t kCapacity = 40;
    std::array<Item, kCapacity> items{};
    std::size_t count = 0;
    bool modal = false;
    std::array<std::string, 2> recordNames{};
    void Add(Label label, Rect rect, Request request = {}, bool enabled = true, bool selected = false) {
        if (count < items.size()) items[count++] = {label, rect, request, enabled, selected, {}};
    }
    void Value(Label label, float y, std::string value) {
        if (count < items.size()) items[count++] = {label, {176, y, 900, 38}, {}, true, false, std::move(value)};
    }
    [[nodiscard]] Request Hit(Vector2 point) const noexcept {
        for (std::size_t i = count < items.size() ? count : items.size(); i > 0; --i) {
            const auto& item = items[i - 1];
            if (item.enabled && item.request.action != Action::None && item.rect.Contains(point)) return item.request;
        }
        return {};
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
    void ValueText(const std::string& value, Vector2 position);
    std::array<Sprite, 42> panels_{};
    std::array<Sprite, 40> labels_{};
    std::array<Sprite, 160> digits_{};
    Sprite names_;
    std::array<std::string, 2> cachedNames_{};
    bool namesReady_ = false;
    std::size_t panelCount_ = 0, labelCount_ = 0, digitCount_ = 0;
    bool ready_ = false;
};
}
