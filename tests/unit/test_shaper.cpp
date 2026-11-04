#include "north/text/font_manager.hpp"
#include "north/text/shaper.hpp"

#include <gtest/gtest.h>

namespace north::text::testing {

class ShaperTestFixture : public ::testing::Test {
protected:
    void SetUp() override { font = FontManager::instance().load_system_font(); }

    std::shared_ptr<Font> font;
};

TEST_F(ShaperTestFixture, ShapesAsciiTextCorrectly) {
    ASSERT_NE(font, nullptr);

    auto run = Shaper::shape_text("Hello World", font, 16.0f, TextDirection::LTR);
    EXPECT_EQ(run.direction, TextDirection::LTR);
    EXPECT_FLOAT_EQ(run.font_size, 16.0f);
    EXPECT_GT(run.total_advance, 0.0f);
    EXPECT_EQ(run.glyphs.size(), 11u);

    for (const auto& g : run.glyphs) {
        EXPECT_GT(g.glyph_id, 0u);
        EXPECT_GT(g.advance_x, 0.0f);
    }
}

TEST_F(ShaperTestFixture, ShapesUtf8AccentedCharacters) {
    ASSERT_NE(font, nullptr);

    // "Café" contains 4 codepoints, 5 UTF-8 bytes
    auto run = Shaper::shape_text("Caf\xC3\xA9", font, 16.0f, TextDirection::LTR);
    EXPECT_GT(run.total_advance, 0.0f);
    EXPECT_EQ(run.glyphs.size(), 4u);
}

TEST_F(ShaperTestFixture, EmptyStringProducesEmptyRun) {
    ASSERT_NE(font, nullptr);

    auto run = Shaper::shape_text("", font, 16.0f);
    EXPECT_TRUE(run.glyphs.empty());
    EXPECT_FLOAT_EQ(run.total_advance, 0.0f);
}

TEST_F(ShaperTestFixture, NullFontProducesEmptyRun) {
    auto run = Shaper::shape_text("Hello", nullptr, 16.0f);
    EXPECT_TRUE(run.glyphs.empty());
    EXPECT_FLOAT_EQ(run.total_advance, 0.0f);
}

} // namespace north::text::testing
