#pragma once
#include "math/Struct.h"
#include <cmath>

namespace farmui {
struct Rect {
    float x = 0, y = 0, width = 0, height = 0;
    [[nodiscard]] bool Contains(Vector2 p) const noexcept {
        return std::isfinite(p.x) && std::isfinite(p.y) &&
            p.x >= x && p.y >= y && p.x < x + width && p.y < y + height;
    }
};
}
