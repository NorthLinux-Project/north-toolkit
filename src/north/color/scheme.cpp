#include "north/color/scheme.hpp"

#include "north/color/hct.hpp"
#include "north/style/theme.hpp"

#include <algorithm>
#include <cmath>

namespace north::color {

M3Palettes make_m3_palettes(float hue, float chroma) noexcept {
    float primary_chroma = (std::max)(chroma, 48.0f);
    float secondary_chroma = 16.0f;
    float tertiary_chroma = 24.0f;
    float neutral_chroma = 4.0f;
    float neutral_variant_chroma = 8.0f;

    float tert_hue = std::fmod(hue + 60.0f, 360.0f);
    if (tert_hue < 0.0f) {
        tert_hue += 360.0f;
    }

    static const TonalPalette kStaticError = make_tonal_palette(25.0f, 84.0f);

    return M3Palettes{
        .primary = make_tonal_palette(hue, primary_chroma),
        .secondary = make_tonal_palette(hue, secondary_chroma),
        .tertiary = make_tonal_palette(tert_hue, tertiary_chroma),
        .neutral = make_tonal_palette(hue, neutral_chroma),
        .neutral_variant = make_tonal_palette(hue, neutral_variant_chroma),
        .error = kStaticError,
    };
}

M3Palettes make_m3_palettes(core::Color seed) noexcept {
    Hct seed_hct = srgb_to_hct(seed);
    return make_m3_palettes(seed_hct.hue, seed_hct.chroma);
}

style::ColorTokens make_dark_tokens(const M3Palettes& p) noexcept {
    return style::ColorTokens{
        .primary = p.primary[80.0f],
        .on_primary = p.primary[20.0f],
        .surface = p.neutral[10.0f],
        .surface_variant = p.neutral_variant[30.0f],
        .on_surface = p.neutral[90.0f],
        .on_surface_variant = p.neutral_variant[80.0f],
        .background = p.neutral[10.0f],
        .on_background = p.neutral[90.0f],
        .outline = p.neutral_variant[60.0f],
        .outline_variant = p.neutral_variant[30.0f],
        .error = p.error[80.0f],
        .on_error = p.error[20.0f],
    };
}

style::ColorTokens make_light_tokens(const M3Palettes& p) noexcept {
    return style::ColorTokens{
        .primary = p.primary[40.0f],
        .on_primary = p.primary[100.0f],
        .surface = p.neutral[99.0f],
        .surface_variant = p.neutral_variant[90.0f],
        .on_surface = p.neutral[10.0f],
        .on_surface_variant = p.neutral_variant[30.0f],
        .background = p.neutral[99.0f],
        .on_background = p.neutral[10.0f],
        .outline = p.neutral_variant[50.0f],
        .outline_variant = p.neutral_variant[80.0f],
        .error = p.error[40.0f],
        .on_error = p.error[100.0f],
    };
}

} // namespace north::color
