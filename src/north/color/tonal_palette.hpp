#pragma once

#include "north/core/types.hpp"

#include <array>

namespace north::color {

inline constexpr std::array<float, 13> kToneStops = {
    0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 95.0f, 99.0f, 100.0f
};

struct TonalPalette {
    float hue{0.0f};
    float chroma{0.0f};
    std::array<core::Color, 13> tones{};

    [[nodiscard]] constexpr bool operator==(const TonalPalette&) const noexcept = default;
    [[nodiscard]] core::Color at_tone(float tone) const noexcept;
    [[nodiscard]] core::Color operator[](float tone) const noexcept { return at_tone(tone); }
};

[[nodiscard]] TonalPalette make_tonal_palette(float hue, float chroma) noexcept;

} // namespace north::color
