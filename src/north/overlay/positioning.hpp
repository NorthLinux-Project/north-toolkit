#pragma once

#include "north/core/types.hpp"
#include "north/overlay/overlay_types.hpp"

namespace north::overlay {

[[nodiscard]] core::Point solve_overlay_position(core::Rect anchor, core::Size overlay_size,
                                                 core::Size viewport_size, Placement placement,
                                                 float offset = 8.0f,
                                                 bool flip_on_collision = true,
                                                 bool clamp = true) noexcept;

} // namespace north::overlay
