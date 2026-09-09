#include "farm/ui/FarmRuntimeUI.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

int main() {
    using namespace farmui;
    View view;
    assert(view.Hit({0, 0}).action == Action::None);
    assert(view.NextActionable(-1, 1) == -1);
    view.Add(Label::Menu, {10, 20, 100, 40}, {Action::Menu});
    view.Add(Label::Save, {120, 20, 100, 40}, {Action::Save}, false);
    view.Add(Label::Ready, {10, 80, 100, 40});
    view.Add(Label::Cancel, {120, 80, 100, 40}, {Action::Cancel});
    assert(view.Hit({10, 20}).action == Action::Menu);
    assert(view.Hit({110, 20}).action == Action::None);
    assert(view.Hit({10, 60}).action == Action::None);
    assert(view.Hit({130, 30}).action == Action::None);
    assert(view.Hit({std::numeric_limits<float>::quiet_NaN(), 20}).action == Action::None);
    assert(view.Hit({std::numeric_limits<float>::infinity(), 20}).action == Action::None);
    assert(view.NextActionable(-1, 1) == 0);
    assert(view.NextActionable(0, 1) == 3);
    assert(view.NextActionable(3, 1) == 0);
    assert(view.NextActionable(0, -1) == 3);
    view.Add(Label::Accept, {10, 20, 100, 40}, {Action::Accept});
    assert(view.Hit({20, 30}).action == Action::Accept);
    for (int i = 0; i < 100; ++i) view.Add(Label::Menu, {}, {Action::Menu});
    assert(view.count == View::kCapacity);
    view.count = 1000;
    assert(view.Hit({20, 30}).action == Action::Accept);
    for (auto& item : view.items) item.enabled = false;
    assert(view.NextActionable(1, 1) == -1);
    assert(view.Hit({20, 30}).action == Action::None);
    for (const auto& uv : kLabels) {
        assert(std::isfinite(uv.x) && std::isfinite(uv.y));
        assert(uv.width > 0 && uv.height > 0);
        assert(uv.x >= 0 && uv.x + uv.width <= 1280);
        assert(uv.y >= 0 && uv.y + uv.height <= kAsciiY);
    }
    std::cout << "PASS: hit boundaries, disabled actions, navigation, capacity, atlas bounds\n";
}
