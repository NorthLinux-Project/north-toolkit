#pragma once

#include "north/core/types.hpp"

#include <array>

namespace north::color {

struct Hct {
    float hue{0.0f};
    float chroma{0.0f};
    float tone{0.0f};

    [[nodiscard]] constexpr bool operator==(const Hct&) const noexcept = default;
};

[[nodiscard]] core::Color srgb_to_linear(core::Color c) noexcept;
[[nodiscard]] core::Color linear_to_srgb(core::Color c) noexcept;

[[nodiscard]] std::array<float, 3> linear_rgb_to_xyz(core::Color c) noexcept;
[[nodiscard]] core::Color xyz_to_linear_rgb(std::array<float, 3> xyz) noexcept;

[[nodiscard]] Hct xyz_to_hct(std::array<float, 3> xyz) noexcept;
[[nodiscard]] std::array<float, 3> hct_to_xyz(Hct hct) noexcept;

[[nodiscard]] Hct srgb_to_hct(core::Color c) noexcept;
[[nodiscard]] core::Color hct_to_srgb(Hct hct) noexcept;

} // namespace north::color
