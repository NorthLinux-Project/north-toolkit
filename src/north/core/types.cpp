#include "north/core/types.hpp"

namespace north::core {

Rect Rect::intersect(const Rect& other) const noexcept {
    if (is_empty() || other.is_empty()) {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }

    float new_x = std::max(x, other.x);
    float new_y = std::max(y, other.y);
    float new_r = std::min(x + width, other.x + other.width);
    float new_b = std::min(y + height, other.y + other.height);

    if (new_r <= new_x || new_b <= new_y) {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }

    return {new_x, new_y, new_r - new_x, new_b - new_y};
}

Rect Rect::unite(const Rect& other) const noexcept {
    if (is_empty())
        return other;
    if (other.is_empty())
        return *this;

    float new_x = std::min(x, other.x);
    float new_y = std::min(y, other.y);
    float new_r = std::max(x + width, other.x + other.width);
    float new_b = std::max(y + height, other.y + other.height);

    return {new_x, new_y, new_r - new_x, new_b - new_y};
}

} // namespace north::core
