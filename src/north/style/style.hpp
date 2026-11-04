#pragma once

#include "north/core/types.hpp"

namespace north::style {

struct BoxStyle {
    core::Color background{0.0f, 0.0f, 0.0f, 0.0f};
    core::Color border_color{0.0f, 0.0f, 0.0f, 0.0f};
    core::Edges border_width{};
    core::CornerRadii corner_radii{};
    core::Edges padding{};
    core::Color shadow_color{0.0f, 0.0f, 0.0f, 0.0f};
    float shadow_radius{0.0f};
    float shadow_spread{0.0f};
    core::Point shadow_offset{0.0f, 0.0f};
};

} // namespace north::style
