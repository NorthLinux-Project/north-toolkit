#include "north/style/theme.hpp"

#include "north/animation/animation_manager.hpp"
#include "north/core/signal.hpp"
#include "north/core/tree.hpp"
#include "north/widget/button.hpp"

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

TEST(ThemeTest, FromSeedGeneratesValidDarkAndLightThemes) {
    core::Color seed{0.12f, 0.53f, 0.90f, 1.0f};

    Theme dark_theme = Theme::from_seed(seed, true);
    EXPECT_TRUE(dark_theme.is_dark());
    EXPECT_EQ(dark_theme.colors().primary, dark_theme.palettes().primary.at_tone(80.0f));
    EXPECT_EQ(dark_theme.colors().on_primary, dark_theme.palettes().primary.at_tone(20.0f));

    Theme light_theme = Theme::from_seed(seed, false);
    EXPECT_FALSE(light_theme.is_dark());
    EXPECT_EQ(light_theme.colors().primary, light_theme.palettes().primary.at_tone(40.0f));
    EXPECT_EQ(light_theme.colors().on_primary, light_theme.palettes().primary.at_tone(100.0f));
}

TEST(ThemeTest, SignalThemeDirtyNotificationBubblesPaintOnly) {
    core::Signal<Theme> theme_sig(Theme::dark());

    uint32_t received_flags = 0;
    int notify_count = 0;

    struct Context {
        uint32_t* flags;
        int* count;
    } ctx{&received_flags, &notify_count};

    theme_sig.bind_dirty(
        [](void* user_data, uint32_t flags) {
            auto* c = static_cast<Context*>(user_data);
            *c->flags = flags;
            (*c->count)++;
        },
        &ctx, core::kPaint);

    Theme new_theme = Theme::from_seed(core::Color{0.9f, 0.2f, 0.2f, 1.0f}, true);
    theme_sig.set(new_theme);

    EXPECT_EQ(notify_count, 1);
    EXPECT_EQ(received_flags, core::kPaint);

    // Setting same theme should not notify (Signal deduplication)
    theme_sig.set(new_theme);
    EXPECT_EQ(notify_count, 1);
}

TEST(ThemeTest, WidgetThemeSignalTriggersAnimatedColorTransition) {
    core::NodeTree tree;
    core::Signal<Theme> theme_sig(Theme::dark());

    auto btn_handle = widget::ButtonBuilder(tree, "Test Button")
                          .theme_signal(&theme_sig)
                          .build();
    ASSERT_TRUE(btn_handle.is_valid());

    auto* node = tree.get(btn_handle);
    ASSERT_NE(node, nullptr);
    auto* btn_data = reinterpret_cast<widget::ButtonData*>(node->user_data);
    ASSERT_NE(btn_data, nullptr);

    core::Color initial_primary = theme_sig.get().colors().primary;
    EXPECT_FLOAT_EQ(btn_data->animated_bg.get().r, initial_primary.r);
    EXPECT_FALSE(btn_data->animated_bg.is_animating());

    // Switch theme to a vibrant amber seed
    core::Color amber_seed{0.95f, 0.65f, 0.05f, 1.0f};
    Theme amber_theme = Theme::from_seed(amber_seed, true);
    theme_sig.set(amber_theme);

    // Animated value should now be animating toward the new primary color
    EXPECT_TRUE(btn_data->animated_bg.is_animating());
    EXPECT_NE(btn_data->animated_bg.target().r, initial_primary.r);

    // Advance animation partially
    animation::AnimationManager::instance().tick(0.10f);
    EXPECT_TRUE(btn_data->animated_bg.is_animating());

    // Complete animation
    animation::AnimationManager::instance().tick(0.20f);
    EXPECT_FALSE(btn_data->animated_bg.is_animating());
    EXPECT_NEAR(btn_data->animated_bg.get().r, amber_theme.colors().primary.r, 0.001f);
}

} // namespace north::style::testing
