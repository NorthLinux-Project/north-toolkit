#pragma once

#include "north/core/types.hpp"

namespace north::style {

struct ColorTokens {
    core::Color primary;
    core::Color on_primary;
    core::Color surface;
    core::Color surface_variant;
    core::Color on_surface;
    core::Color on_surface_variant;
    core::Color background;
    core::Color on_background;
    core::Color outline;
    core::Color outline_variant;
    core::Color error;
    core::Color on_error;
};

class Theme {
public:
    constexpr explicit Theme(ColorTokens tokens) noexcept : colors_(tokens) {}

    [[nodiscard]] static const Theme& dark() noexcept;
    [[nodiscard]] static const Theme& light() noexcept;

    [[nodiscard]] const ColorTokens& colors() const noexcept { return colors_; }

    [[nodiscard]] static float relative_luminance(core::Color c) noexcept;
    [[nodiscard]] static float contrast_ratio(core::Color a, core::Color b) noexcept;

private:
    ColorTokens colors_;
};

} // namespace north::style
