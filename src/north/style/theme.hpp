#pragma once

#include "north/color/scheme.hpp"
#include "north/core/types.hpp"

#include <optional>

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

    [[nodiscard]] constexpr bool operator==(const ColorTokens&) const noexcept = default;
};

class Theme {
public:
    constexpr explicit Theme(ColorTokens tokens, float duration = 0.20f) noexcept
        : colors_(tokens), palettes_(std::nullopt), transition_duration_(duration) {}
    constexpr Theme(ColorTokens tokens, color::M3Palettes palettes, float duration = 0.20f) noexcept
        : colors_(tokens), palettes_(palettes), transition_duration_(duration) {}

    [[nodiscard]] static Theme from_seed(core::Color seed, bool dark, float duration = 0.20f) noexcept;
    [[nodiscard]] static Theme from_seed(core::Color seed, bool dark,
                                         const color::M3Palettes& palettes, float duration = 0.20f) noexcept;
    [[nodiscard]] static Theme from_hct(float hue, float chroma, bool dark, float duration = 0.20f) noexcept;

    [[nodiscard]] static const Theme& dark() noexcept;
    [[nodiscard]] static const Theme& light() noexcept;

    [[nodiscard]] const ColorTokens& colors() const noexcept { return colors_; }
    [[nodiscard]] bool is_dark() const noexcept { return relative_luminance(colors_.surface) < 0.5f; }
    [[nodiscard]] bool has_palettes() const noexcept { return palettes_.has_value(); }
    [[nodiscard]] const color::M3Palettes& palettes() const noexcept { return *palettes_; }
    [[nodiscard]] float transition_duration() const noexcept { return transition_duration_; }

    [[nodiscard]] core::Color surface_container() const noexcept;
    [[nodiscard]] core::Color surface_container_high() const noexcept;
    [[nodiscard]] core::Color surface_container_highest() const noexcept;

    [[nodiscard]] constexpr bool operator==(const Theme&) const noexcept = default;

    [[nodiscard]] static float relative_luminance(core::Color c) noexcept;
    [[nodiscard]] static float contrast_ratio(core::Color a, core::Color b) noexcept;

private:
    ColorTokens colors_;
    std::optional<color::M3Palettes> palettes_{std::nullopt};
    float transition_duration_{0.20f};
};

} // namespace north::style
