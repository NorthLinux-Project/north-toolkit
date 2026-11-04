#include "north/style/theme.hpp"

#include <gtest/gtest.h>

namespace north::style::testing {

TEST(ThemeTest, RelativeLuminanceRangeAndBlackWhiteExtremes) {
    core::Color black{0.0f, 0.0f, 0.0f, 1.0f};
    core::Color white{1.0f, 1.0f, 1.0f, 1.0f};

    EXPECT_FLOAT_EQ(Theme::relative_luminance(black), 0.0f);
    EXPECT_FLOAT_EQ(Theme::relative_luminance(white), 1.0f);

    float max_contrast = Theme::contrast_ratio(black, white);
    EXPECT_NEAR(max_contrast, 21.0f, 0.1f);
}

TEST(ThemeTest, DarkThemeSatisfiesWcag22AaContrastRatios) {
    const auto& dark = Theme::dark().colors();

    // Body text on surface: >= 4.5:1
    float on_surface_contrast = Theme::contrast_ratio(dark.on_surface, dark.surface);
    EXPECT_GE(on_surface_contrast, 4.5f);

    // Body text on background: >= 4.5:1
    float on_bg_contrast = Theme::contrast_ratio(dark.on_background, dark.background);
    EXPECT_GE(on_bg_contrast, 4.5f);

    // Secondary text on surface: >= 4.5:1
    float on_surface_variant_contrast =
        Theme::contrast_ratio(dark.on_surface_variant, dark.surface);
    EXPECT_GE(on_surface_variant_contrast, 4.5f);

    // Text on primary accent: >= 4.5:1
    float on_primary_contrast = Theme::contrast_ratio(dark.on_primary, dark.primary);
    EXPECT_GE(on_primary_contrast, 4.5f);

    // Text on error: >= 4.5:1
    float on_error_contrast = Theme::contrast_ratio(dark.on_error, dark.error);
    EXPECT_GE(on_error_contrast, 4.5f);
}

TEST(ThemeTest, LightThemeSatisfiesWcag22AaContrastRatios) {
    const auto& light = Theme::light().colors();

    // Body text on surface: >= 4.5:1
    float on_surface_contrast = Theme::contrast_ratio(light.on_surface, light.surface);
    EXPECT_GE(on_surface_contrast, 4.5f);

    // Body text on background: >= 4.5:1
    float on_bg_contrast = Theme::contrast_ratio(light.on_background, light.background);
    EXPECT_GE(on_bg_contrast, 4.5f);

    // Secondary text on surface: >= 4.5:1
    float on_surface_variant_contrast =
        Theme::contrast_ratio(light.on_surface_variant, light.surface);
    EXPECT_GE(on_surface_variant_contrast, 4.5f);

    // Text on primary accent: >= 4.5:1
    float on_primary_contrast = Theme::contrast_ratio(light.on_primary, light.primary);
    EXPECT_GE(on_primary_contrast, 4.5f);

    // Text on error: >= 4.5:1
    float on_error_contrast = Theme::contrast_ratio(light.on_error, light.error);
    EXPECT_GE(on_error_contrast, 4.5f);
}

} // namespace north::style::testing
