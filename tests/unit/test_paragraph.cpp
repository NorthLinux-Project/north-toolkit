#include "north/core/arena.hpp"
#include "north/gpu/d3d11/d3d11_device.hpp"
#include "north/render/display_list.hpp"
#include "north/text/font_manager.hpp"
#include "north/text/glyph_atlas.hpp"
#include "north/text/paragraph.hpp"

#include <gtest/gtest.h>

namespace north::text::testing {

class ParagraphTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        font = FontManager::instance().load_system_font();
        device = std::make_unique<gpu::D3D11Device>();
        initialized = device->init_offscreen(800, 600);
        arena = std::make_unique<core::Arena>(1024 * 64);
    }

    void TearDown() override {
        arena.reset();
        device.reset();
    }

    std::shared_ptr<Font> font;
    std::unique_ptr<gpu::D3D11Device> device;
    std::unique_ptr<core::Arena> arena;
    bool initialized{false};
};

TEST_F(ParagraphTestFixture, MeasuresSingleLineTextWidth) {
    ASSERT_NE(font, nullptr);

    ParagraphBuilder builder;
    TextStyle style{.font = font, .font_size = 16.0f};
    builder.add_text("Single Line Text", style);
    auto p = builder.build();

    p->layout(layout::BoxConstraints::loose({800.0f, 600.0f}));
    EXPECT_GT(p->size().width, 50.0f);
    EXPECT_GT(p->size().height, 10.0f);
    EXPECT_EQ(p->line_count(), 1u);
    EXPECT_GT(p->glyphs().size(), 0u);
}

TEST_F(ParagraphTestFixture, WrapsTextWhenExceedingMaxWidth) {
    ASSERT_NE(font, nullptr);

    ParagraphBuilder builder;
    TextStyle style{.font = font, .font_size = 16.0f};
    builder.add_text("Word1 Word2 Word3 Word4 Word5 Word6 Word7 Word8 Word9 Word10", style);
    auto p = builder.build();

    // Restrict max width to narrow 120px to force multiple lines
    p->layout(layout::BoxConstraints{0.0f, 120.0f, 0.0f, 600.0f});
    EXPECT_GT(p->line_count(), 1u);
    EXPECT_LE(p->size().width, 120.0f);
}

TEST_F(ParagraphTestFixture, ComputesTotalBoundingBoxHeight) {
    ASSERT_NE(font, nullptr);

    ParagraphBuilder builder;
    TextStyle style{.font = font, .font_size = 16.0f, .line_height = 1.5f};
    builder.add_text("Line One\nLine Two\nLine Three", style);
    auto p = builder.build();

    p->layout(layout::BoxConstraints::loose({800.0f, 600.0f}));
    EXPECT_EQ(p->line_count(), 3u);

    float single_line_h = p->lines()[0].height;
    EXPECT_GT(single_line_h, 0.0f);
    EXPECT_FLOAT_EQ(p->size().height, single_line_h * 3.0f);
}

TEST_F(ParagraphTestFixture, RichTextSpansMaintainIndependentStyles) {
    ASSERT_NE(font, nullptr);

    ParagraphBuilder builder;
    TextStyle style1{
        .font = font, .font_size = 14.0f, .color = core::Color{1.0f, 0.0f, 0.0f, 1.0f}};
    TextStyle style2{
        .font = font, .font_size = 28.0f, .color = core::Color{0.0f, 1.0f, 0.0f, 1.0f}};

    builder.add_text("Normal ", style1);
    builder.add_text("Large", style2);
    auto p = builder.build();

    p->layout(layout::BoxConstraints::loose({800.0f, 600.0f}));
    EXPECT_EQ(p->line_count(), 1u);

    const auto& glyphs = p->glyphs();
    ASSERT_GT(glyphs.size(), 7u);

    // Verify first span has size 14 and red color
    EXPECT_FLOAT_EQ(glyphs[0].font_size, 14.0f);
    EXPECT_FLOAT_EQ(glyphs[0].color.r, 1.0f);

    // Verify second span has size 28 and green color
    EXPECT_FLOAT_EQ(glyphs.back().font_size, 28.0f);
    EXPECT_FLOAT_EQ(glyphs.back().color.g, 1.0f);
}

TEST_F(ParagraphTestFixture, HorizontalAlignmentShiftsLineOffsets) {
    ASSERT_NE(font, nullptr);

    TextStyle style{.font = font, .font_size = 16.0f};

    ParagraphBuilder left_builder(TextAlign::Left);
    left_builder.add_text("Aligned", style);
    auto p_left = left_builder.build();
    p_left->layout(layout::BoxConstraints{300.0f, 300.0f, 0.0f, 600.0f});

    ParagraphBuilder center_builder(TextAlign::Center);
    center_builder.add_text("Aligned", style);
    auto p_center = center_builder.build();
    p_center->layout(layout::BoxConstraints{300.0f, 300.0f, 0.0f, 600.0f});

    ParagraphBuilder right_builder(TextAlign::Right);
    right_builder.add_text("Aligned", style);
    auto p_right = right_builder.build();
    p_right->layout(layout::BoxConstraints{300.0f, 300.0f, 0.0f, 600.0f});

    ASSERT_FALSE(p_left->glyphs().empty());
    ASSERT_FALSE(p_center->glyphs().empty());
    ASSERT_FALSE(p_right->glyphs().empty());

    float left_x = p_left->glyphs()[0].pos.x;
    float center_x = p_center->glyphs()[0].pos.x;
    float right_x = p_right->glyphs()[0].pos.x;

    EXPECT_FLOAT_EQ(left_x, 0.0f);
    EXPECT_GT(center_x, left_x);
    EXPECT_GT(right_x, center_x);
}

TEST_F(ParagraphTestFixture, PaintsGlyphRunsIntoDisplayList) {
    ASSERT_TRUE(initialized);
    ASSERT_NE(font, nullptr);

    GlyphAtlas atlas(*device);
    render::DisplayList dl(*arena, core::Rect{0.0f, 0.0f, 800.0f, 600.0f});

    ParagraphBuilder builder;
    TextStyle style{.font = font, .font_size = 18.0f};
    builder.add_text("Render Me", style);
    auto p = builder.build();

    p->layout(layout::BoxConstraints::loose({800.0f, 600.0f}));
    p->paint(dl, atlas, core::Point{50.0f, 50.0f});

    EXPECT_FALSE(dl.empty());
    ASSERT_EQ(dl.size(), 1u);

    auto cmds = dl.commands();
    EXPECT_EQ(cmds[0].type, render::CommandType::GlyphRun);
    EXPECT_NE(cmds[0].glyph_run.atlas_texture, gpu::kInvalidTexture);
    EXPECT_GT(cmds[0].glyph_run.glyphs.size(), 0u);
}

TEST_F(ParagraphTestFixture, SnapsGlyphYCoordinatesToIntegerScanlines) {
    ASSERT_TRUE(initialized);
    ASSERT_NE(font, nullptr);

    GlyphAtlas atlas(*device);
    render::DisplayList dl(*arena, core::Rect{0.0f, 0.0f, 800.0f, 600.0f});

    ParagraphBuilder builder;
    TextStyle style{.font = font, .font_size = 14.0f};
    builder.add_text("dddddddd", style);
    auto p = builder.build();

    p->layout(layout::BoxConstraints::loose({800.0f, 600.0f}));
    // Paint with non-integer origin to verify baseline Y snapping
    p->paint(dl, atlas, core::Point{12.35f, 24.68f});

    ASSERT_EQ(dl.size(), 1u);
    auto cmds = dl.commands();
    ASSERT_EQ(cmds[0].type, render::CommandType::GlyphRun);
    for (const auto& g : cmds[0].glyph_run.glyphs) {
        EXPECT_FLOAT_EQ(g.dst_rect.y, std::floor(g.dst_rect.y));
    }
}

} // namespace north::text::testing
