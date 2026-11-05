#pragma once

#include "north/color/tonal_palette.hpp"
#include "north/core/types.hpp"

namespace north::style {
struct ColorTokens;
}

namespace north::color {

struct M3Palettes {
    TonalPalette primary;
    TonalPalette secondary;
    TonalPalette tertiary;
    TonalPalette neutral;
    TonalPalette neutral_variant;
    TonalPalette error;

    [[nodiscard]] constexpr bool operator==(const M3Palettes&) const noexcept = default;
};

[[nodiscard]] M3Palettes make_m3_palettes(core::Color seed) noexcept;
[[nodiscard]] M3Palettes make_m3_palettes(float hue, float chroma) noexcept;
[[nodiscard]] style::ColorTokens make_dark_tokens(const M3Palettes& p) noexcept;
[[nodiscard]] style::ColorTokens make_light_tokens(const M3Palettes& p) noexcept;

} // namespace north::color
