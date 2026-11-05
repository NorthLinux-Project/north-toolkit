#pragma once

#include "north/core/types.hpp"

#include <cstdint>

namespace north::overlay {

using OverlayId = uint32_t;
constexpr OverlayId kInvalidOverlayId = 0;

enum class Placement : uint8_t {
    Top,
    Bottom,
    Left,
    Right,
    TopStart,
    TopEnd,
    BottomStart,
    BottomEnd,
    LeftStart,
    LeftEnd,
    RightStart,
    RightEnd,
    Auto,
};

using DismissCallbackFn = void (*)(void* user_data);

struct OverlayConfig {
    Placement placement{Placement::Bottom};
    float offset{8.0f};
    bool flip_on_collision{true};
    bool clamp_to_viewport{true};
    bool modal{false};
    bool barrier_dismissible{true};
    core::Color barrier_color{0.0f, 0.0f, 0.0f, 0.5f};
    DismissCallbackFn on_dismiss{nullptr};
    void* dismiss_user_data{nullptr};
};

} // namespace north::overlay
