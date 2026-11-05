#include "north/color/hct.hpp"
#include "north/color/scheme.hpp"
#include "north/color/tonal_palette.hpp"
#include "north/style/theme.hpp"

#include <gtest/gtest.h>

namespace north::color::testing {

TEST(HctTest, PureBlackAndWhiteToneBoundaries) {
    core::Color black{0.0f, 0.0f, 0.0f, 1.0f};
    core::Color white{1.0f, 1.0f, 1.0f, 1.0f};

    Hct hct_black = srgb_to_hct(black);
    EXPECT_NEAR(hct_black.tone, 0.0f, 0.5f);

    Hct hct_white = srgb_to_hct(white);
    EXPECT_NEAR(hct_white.tone, 100.0f, 0.5f);

    core::Color from_tone_0 = hct_to_srgb(Hct{.hue = 0.0f, .chroma = 0.0f, .tone = 0.0f});
    EXPECT_NEAR(from_tone_0.r, 0.0f, 0.01f);
    EXPECT_NEAR(from_tone_0.g, 0.0f, 0.01f);
    EXPECT_NEAR(from_tone_0.b, 0.0f, 0.01f);

    core::Color from_tone_100 = hct_to_srgb(Hct{.hue = 0.0f, .chroma = 0.0f, .tone = 100.0f});
    EXPECT_NEAR(from_tone_100.r, 1.0f, 0.01f);
    EXPECT_NEAR(from_tone_100.g, 1.0f, 0.01f);
    EXPECT_NEAR(from_tone_100.b, 1.0f, 0.01f);
}

TEST(HctTest, SrgbRoundTripPreservesPerceptualAttributes) {
    core::Color seed{0.25f, 0.45f, 0.85f, 1.0f};
    Hct hct = srgb_to_hct(seed);

    EXPECT_GT(hct.chroma, 10.0f);
    EXPECT_GT(hct.tone, 20.0f);
    EXPECT_LT(hct.tone, 80.0f);

    core::Color roundtrip = hct_to_srgb(hct);
    EXPECT_NEAR(roundtrip.r, seed.r, 0.02f);
    EXPECT_NEAR(roundtrip.g, seed.g, 0.02f);
    EXPECT_NEAR(roundtrip.b, seed.b, 0.02f);
}

TEST(TonalPaletteTest, MonotonicLuminanceAcrossStops) {
    Hct seed_hct = srgb_to_hct(core::Color{0.13f, 0.59f, 0.95f, 1.0f});
    TonalPalette palette = make_tonal_palette(seed_hct.hue, seed_hct.chroma);

    float prev_lum = -1.0f;
    for (size_t i = 0; i < kToneStops.size(); ++i) {
        float stop_tone = kToneStops[i];
        core::Color color = palette.at_tone(stop_tone);
        float lum = style::Theme::relative_luminance(color);
        EXPECT_GE(lum, prev_lum);
        prev_lum = lum;
    }
}

TEST(TonalPaletteTest, ExactEndpoints) {
    TonalPalette palette = make_tonal_palette(240.0f, 50.0f);
    core::Color tone_0 = palette.at_tone(0.0f);
    EXPECT_FLOAT_EQ(tone_0.r, 0.0f);
    EXPECT_FLOAT_EQ(tone_0.g, 0.0f);
    EXPECT_FLOAT_EQ(tone_0.b, 0.0f);

    core::Color tone_100 = palette.at_tone(100.0f);
    EXPECT_FLOAT_EQ(tone_100.r, 1.0f);
    EXPECT_FLOAT_EQ(tone_100.g, 1.0f);
    EXPECT_FLOAT_EQ(tone_100.b, 1.0f);
}

TEST(TonalPaletteTest, PiecewiseLinearInterpolationBetweenStops) {
    TonalPalette palette = make_tonal_palette(120.0f, 40.0f);
    core::Color color_40 = palette.at_tone(40.0f);
    core::Color color_50 = palette.at_tone(50.0f);
    core::Color color_45 = palette.at_tone(45.0f);

    EXPECT_NEAR(color_45.r, (color_40.r + color_50.r) * 0.5f, 0.01f);
    EXPECT_NEAR(color_45.g, (color_40.g + color_50.g) * 0.5f, 0.01f);
    EXPECT_NEAR(color_45.b, (color_40.b + color_50.b) * 0.5f, 0.01f);
}

TEST(SchemeTest, Material3PaletteChromaAndHueRules) {
    core::Color seed{0.20f, 0.60f, 0.86f, 1.0f};
    Hct seed_hct = srgb_to_hct(seed);
    M3Palettes palettes = make_m3_palettes(seed);

    EXPECT_GE(palettes.primary.chroma, 48.0f);
    EXPECT_NEAR(palettes.secondary.chroma, 16.0f, 0.01f);
    EXPECT_NEAR(palettes.tertiary.chroma, 24.0f, 0.01f);
    EXPECT_NEAR(palettes.neutral.chroma, 4.0f, 0.01f);
    EXPECT_NEAR(palettes.neutral_variant.chroma, 8.0f, 0.01f);
    EXPECT_NEAR(palettes.error.hue, 25.0f, 0.01f);
    EXPECT_NEAR(palettes.error.chroma, 84.0f, 0.01f);

    float expected_tertiary_hue = std::fmod(seed_hct.hue + 60.0f, 360.0f);
    if (expected_tertiary_hue < 0.0f) {
        expected_tertiary_hue += 360.0f;
    }
    EXPECT_NEAR(palettes.tertiary.hue, expected_tertiary_hue, 0.01f);
}

TEST(SchemeTest, DarkAndLightTokensSatisfyWcag22AaContrast) {
    core::Color seed{0.15f, 0.40f, 0.95f, 1.0f};
    M3Palettes palettes = make_m3_palettes(seed);

    style::ColorTokens dark = make_dark_tokens(palettes);
    EXPECT_GE(style::Theme::contrast_ratio(dark.on_primary, dark.primary), 4.5f);
    EXPECT_GE(style::Theme::contrast_ratio(dark.on_surface, dark.surface), 4.5f);
    EXPECT_GE(style::Theme::contrast_ratio(dark.on_background, dark.background), 4.5f);
    EXPECT_GE(style::Theme::contrast_ratio(dark.on_error, dark.error), 4.5f);

    // Verify secondary and tertiary tonal palettes contrast
    EXPECT_GE(style::Theme::contrast_ratio(palettes.secondary.at_tone(20.0f),
                                           palettes.secondary.at_tone(80.0f)),
              4.5f);
    EXPECT_GE(style::Theme::contrast_ratio(palettes.tertiary.at_tone(20.0f),
                                           palettes.tertiary.at_tone(80.0f)),
              4.5f);

    style::ColorTokens light = make_light_tokens(palettes);
    EXPECT_GE(style::Theme::contrast_ratio(light.on_primary, light.primary), 4.5f);
    EXPECT_GE(style::Theme::contrast_ratio(light.on_surface, light.surface), 4.5f);
    EXPECT_GE(style::Theme::contrast_ratio(light.on_background, light.background), 4.5f);
    EXPECT_GE(style::Theme::contrast_ratio(light.on_error, light.error), 4.5f);

    EXPECT_GE(style::Theme::contrast_ratio(palettes.secondary.at_tone(100.0f),
                                           palettes.secondary.at_tone(40.0f)),
              4.5f);
    EXPECT_GE(style::Theme::contrast_ratio(palettes.tertiary.at_tone(100.0f),
                                           palettes.tertiary.at_tone(40.0f)),
              4.5f);
}

} // namespace north::color::testing
