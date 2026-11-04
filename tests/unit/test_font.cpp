#include "north/text/font_manager.hpp"

#include <gtest/gtest.h>

namespace north::text::testing {

TEST(FontTest, InitializesFontManagerAndLoadsSystemFont) {
    auto& mgr = FontManager::instance();
    EXPECT_NE(mgr.ft_library(), nullptr);

    auto font = mgr.load_system_font("Segoe UI");
    if (!font) {
        font = mgr.load_system_font("Arial");
    }
    ASSERT_NE(font, nullptr);

    EXPECT_GT(font->font_id(), 0u);
    EXPECT_NE(font->ft_face(), nullptr);
    EXPECT_NE(font->hb_font(), nullptr);
    EXPECT_GT(font->units_per_em(), 0);

    float asc = font->ascender(16.0f);
    float dsc = font->descender(16.0f);
    float lh = font->line_height(16.0f);

    EXPECT_GT(asc, 0.0f);
    EXPECT_LE(dsc, 0.0f);
    EXPECT_GE(lh, asc - dsc);

    EXPECT_FALSE(font->family_name().empty());
}

TEST(FontTest, FontManagerCachesLoadedFonts) {
    auto& mgr = FontManager::instance();
    auto font1 = mgr.load_system_font("Segoe UI");
    auto font2 = mgr.load_system_font("Segoe UI");

    if (font1 && font2) {
        EXPECT_EQ(font1.get(), font2.get());
        EXPECT_EQ(font1->font_id(), font2->font_id());
    }
}

TEST(FontTest, ChangesPixelSizeCleanly) {
    auto& mgr = FontManager::instance();
    auto font = mgr.load_system_font();
    ASSERT_NE(font, nullptr);

    font->set_pixel_size(24.0f);
    float asc24 = font->ascender(24.0f);
    font->set_pixel_size(12.0f);
    float asc12 = font->ascender(12.0f);

    EXPECT_FLOAT_EQ(asc24, asc12 * 2.0f);
}

} // namespace north::text::testing
