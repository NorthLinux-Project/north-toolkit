#include "north/color/tonal_palette.hpp"

#include "north/color/hct.hpp"

#include <algorithm>

namespace north::color {

TonalPalette make_tonal_palette(float hue, float chroma) noexcept {
    TonalPalette palette;
    palette.hue = hue;
    palette.chroma = chroma;
    palette.tones[0] = core::Color{0.0f, 0.0f, 0.0f, 1.0f};
    palette.tones[12] = core::Color{1.0f, 1.0f, 1.0f, 1.0f};
    for (size_t i = 1; i < 12; ++i) {
        palette.tones[i] = hct_to_srgb(Hct{hue, chroma, kToneStops[i]});
    }
    return palette;
}

core::Color TonalPalette::at_tone(float tone) const noexcept {
    if (tone <= kToneStops.front()) {
        return tones.front();
    }
    if (tone >= kToneStops.back()) {
        return tones.back();
    }

    for (size_t i = 0; i + 1 < kToneStops.size(); ++i) {
        if (tone >= kToneStops[i] && tone <= kToneStops[i + 1]) {
            if (tone == kToneStops[i]) {
                return tones[i];
            }
            if (tone == kToneStops[i + 1]) {
                return tones[i + 1];
            }
            float factor = (tone - kToneStops[i]) / (kToneStops[i + 1] - kToneStops[i]);
            const auto& c0 = tones[i];
            const auto& c1 = tones[i + 1];
            return core::Color{
                std::clamp((1.0f - factor) * c0.r + factor * c1.r, 0.0f, 1.0f),
                std::clamp((1.0f - factor) * c0.g + factor * c1.g, 0.0f, 1.0f),
                std::clamp((1.0f - factor) * c0.b + factor * c1.b, 0.0f, 1.0f),
                1.0f,
            };
        }
    }

    return tones.back();
}

} // namespace north::color
