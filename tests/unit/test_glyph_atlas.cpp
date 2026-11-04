#include "north/gpu/d3d11/d3d11_device.hpp"
#include "north/text/font_manager.hpp"
#include "north/text/glyph_atlas.hpp"

#include <gtest/gtest.h>

namespace north::text::testing {

class GlyphAtlasTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        device = std::make_unique<gpu::D3D11Device>();
        initialized = device->init_offscreen(800, 600);
        font = FontManager::instance().load_system_font();
    }

    void TearDown() override { device.reset(); }

    std::unique_ptr<gpu::D3D11Device> device;
    std::shared_ptr<Font> font;
    bool initialized{false};
};

TEST_F(GlyphAtlasTestFixture, InsertsAndCachesGlyphs) {
    ASSERT_TRUE(initialized);
    ASSERT_NE(font, nullptr);

    GlyphAtlas atlas(*device);
    EXPECT_EQ(atlas.cached_glyph_count(), 0);
    EXPECT_EQ(atlas.page_count(), 0);

    // FreeType glyph index for 'A'
    uint32_t glyph_a = FT_Get_Char_Index(font->ft_face(), 'A');
    ASSERT_GT(glyph_a, 0u);

    auto g1 = atlas.get_or_insert_glyph(font, glyph_a, 16.0f, 0.0f);
    EXPECT_GT(g1.width, 0.0f);
    EXPECT_GT(g1.height, 0.0f);
    EXPECT_NE(g1.page_texture, gpu::kInvalidTexture);
    EXPECT_GT(g1.uv_rect.width, 0.0f);
    EXPECT_GT(g1.uv_rect.height, 0.0f);

    EXPECT_EQ(atlas.cached_glyph_count(), 1);
    EXPECT_EQ(atlas.page_count(), 1);

    // Retrieve again from cache
    auto g2 = atlas.get_or_insert_glyph(font, glyph_a, 16.0f, 0.0f);
    EXPECT_EQ(g1.page_texture, g2.page_texture);
    EXPECT_FLOAT_EQ(g1.uv_rect.x, g2.uv_rect.x);
    EXPECT_FLOAT_EQ(g1.uv_rect.y, g2.uv_rect.y);
    EXPECT_EQ(atlas.cached_glyph_count(), 1);
}

TEST_F(GlyphAtlasTestFixture, SmallGlyphRasterizationAndMetrics) {
    ASSERT_TRUE(initialized);
    ASSERT_NE(font, nullptr);

    font->set_pixel_size(14.0f);
    FT_Face face = font->ft_face();
    uint32_t glyph_d = FT_Get_Char_Index(face, 'd');
    ASSERT_GT(glyph_d, 0u);

    GlyphAtlas atlas(*device);
    auto cg = atlas.get_or_insert_glyph(font, glyph_d, 14.0f, 0.0f);
    EXPECT_GT(cg.width, 0.0f);
    EXPECT_GT(cg.height, 0.0f);
    EXPECT_NE(cg.page_texture, gpu::kInvalidTexture);
    EXPECT_GE(cg.offset_y, cg.height);
}

TEST_F(GlyphAtlasTestFixture, SubpixelQuarterPixelBins) {
    ASSERT_TRUE(initialized);
    ASSERT_NE(font, nullptr);

    GlyphAtlas atlas(*device);
    uint32_t glyph_h = FT_Get_Char_Index(font->ft_face(), 'H');
    ASSERT_GT(glyph_h, 0u);

    auto g_sub0 = atlas.get_or_insert_glyph(font, glyph_h, 16.0f, 0.0f);
    auto g_sub1 = atlas.get_or_insert_glyph(font, glyph_h, 16.0f, 0.25f);
    auto g_sub2 = atlas.get_or_insert_glyph(font, glyph_h, 16.0f, 0.5f);
    auto g_sub3 = atlas.get_or_insert_glyph(font, glyph_h, 16.0f, 0.75f);

    EXPECT_EQ(atlas.cached_glyph_count(), 4);
    EXPECT_NE(g_sub0.uv_rect.x, g_sub1.uv_rect.x);
    EXPECT_NE(g_sub1.uv_rect.x, g_sub2.uv_rect.x);
    EXPECT_NE(g_sub2.uv_rect.x, g_sub3.uv_rect.x);
}

TEST_F(GlyphAtlasTestFixture, CleansTexturesOnClear) {
    ASSERT_TRUE(initialized);
    ASSERT_NE(font, nullptr);

    GlyphAtlas atlas(*device);
    uint32_t glyph_a = FT_Get_Char_Index(font->ft_face(), 'A');
    atlas.get_or_insert_glyph(font, glyph_a, 16.0f);

    EXPECT_EQ(atlas.page_count(), 1);
    atlas.clear();
    EXPECT_EQ(atlas.page_count(), 0);
    EXPECT_EQ(atlas.cached_glyph_count(), 0);
}

} // namespace north::text::testing
