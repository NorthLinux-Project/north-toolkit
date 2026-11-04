#include "north/style/theme.hpp"

#include <algorithm>
#include <cmath>

namespace north::style {

namespace {

float srgb_to_linear(float c) noexcept {
    if (c <= 0.04045f) {
        return c / 12.92f;
    }
    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

} // namespace

float Theme::relative_luminance(core::Color c) noexcept {
    float r = srgb_to_linear(std::clamp(c.r, 0.0f, 1.0f));
    float g = srgb_to_linear(std::clamp(c.g, 0.0f, 1.0f));
    float b = srgb_to_linear(std::clamp(c.b, 0.0f, 1.0f));
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

float Theme::contrast_ratio(core::Color a, core::Color b) noexcept {
    float l1 = relative_luminance(a);
    float l2 = relative_luminance(b);
    if (l1 < l2) {
        std::swap(l1, l2);
    }
    return (l1 + 0.05f) / (l2 + 0.05f);
}

const Theme& Theme::dark() noexcept {
    static const Theme instance(
        ColorTokens{.primary = core::Color::from_rgba8(56, 139, 253),
                    .on_primary = core::Color::from_rgba8(0, 27, 63),
                    .surface = core::Color::from_rgba8(26, 28, 32),
                    .surface_variant = core::Color::from_rgba8(36, 39, 45),
                    .on_surface = core::Color::from_rgba8(232, 234, 240),
                    .on_surface_variant = core::Color::from_rgba8(155, 161, 176),
                    .background = core::Color::from_rgba8(18, 19, 22),
                    .on_background = core::Color::from_rgba8(241, 242, 246),
                    .outline = core::Color::from_rgba8(62, 68, 81),
                    .outline_variant = core::Color::from_rgba8(45, 50, 60),
                    .error = core::Color::from_rgba8(248, 81, 73),
                    .on_error = core::Color::from_rgba8(58, 8, 6)});
    return instance;
}

const Theme& Theme::light() noexcept {
    static const Theme instance(
        ColorTokens{.primary = core::Color::from_rgba8(9, 105, 218),
                    .on_primary = core::Color::from_rgba8(255, 255, 255),
                    .surface = core::Color::from_rgba8(255, 255, 255),
                    .surface_variant = core::Color::from_rgba8(240, 242, 246),
                    .on_surface = core::Color::from_rgba8(28, 30, 36),
                    .on_surface_variant = core::Color::from_rgba8(87, 96, 106),
                    .background = core::Color::from_rgba8(252, 252, 253),
                    .on_background = core::Color::from_rgba8(26, 28, 34),
                    .outline = core::Color::from_rgba8(208, 215, 222),
                    .outline_variant = core::Color::from_rgba8(226, 229, 233),
                    .error = core::Color::from_rgba8(207, 34, 46),
                    .on_error = core::Color::from_rgba8(255, 255, 255)});
    return instance;
}

} // namespace north::style
