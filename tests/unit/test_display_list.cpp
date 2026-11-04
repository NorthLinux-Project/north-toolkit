#include "north/core/arena.hpp"
#include "north/render/display_list.hpp"

#include <gtest/gtest.h>

namespace north::render::testing {

class DisplayListTestFixture : public ::testing::Test {
protected:
    void SetUp() override { arena = std::make_unique<core::Arena>(1024 * 64); }

    void TearDown() override { arena.reset(); }

    std::unique_ptr<core::Arena> arena;
};

TEST_F(DisplayListTestFixture, InitialStateIsEmpty) {
    DisplayList dl(*arena, core::Rect{0.0f, 0.0f, 800.0f, 600.0f});
    EXPECT_TRUE(dl.empty());
    EXPECT_EQ(dl.size(), 0);
    EXPECT_TRUE(dl.commands().empty());
    EXPECT_EQ(dl.current_clip(), (core::Rect{0.0f, 0.0f, 800.0f, 600.0f}));
}

TEST_F(DisplayListTestFixture, PushesQuadsAndRecordsClip) {
    core::Rect viewport{0.0f, 0.0f, 800.0f, 600.0f};
    DisplayList dl(*arena, viewport);

    core::Rect rect1{10.0f, 20.0f, 100.0f, 50.0f};
    core::CornerRadii radii1 = core::CornerRadii::uniform(8.0f);
    core::Color fill1 = core::Color::from_rgba8(255, 0, 0, 255);
    core::Color border1 = core::Color::from_rgba8(0, 0, 0, 255);
    core::Edges border_width1 = core::Edges::uniform(2.0f);

    dl.push_quad(rect1, radii1, fill1, border1, border_width1);

    EXPECT_FALSE(dl.empty());
    EXPECT_EQ(dl.size(), 1);

    auto cmds = dl.commands();
    ASSERT_EQ(cmds.size(), 1);
    EXPECT_EQ(cmds[0].type, CommandType::Quad);
    EXPECT_EQ(cmds[0].clip_rect, viewport);
    EXPECT_EQ(cmds[0].quad.bounds, rect1);
    EXPECT_FLOAT_EQ(cmds[0].quad.radii.top_left, 8.0f);
    EXPECT_FLOAT_EQ(cmds[0].quad.fill_color.r, 1.0f);
    EXPECT_FLOAT_EQ(cmds[0].quad.border_width.top, 2.0f);
}

TEST_F(DisplayListTestFixture, PushesShadowsWithParameters) {
    DisplayList dl(*arena, core::Rect{0.0f, 0.0f, 800.0f, 600.0f});

    core::Rect rect{50.0f, 50.0f, 200.0f, 100.0f};
    core::CornerRadii radii = core::CornerRadii::uniform(12.0f);
    core::Color shadow_color = core::Color::from_rgba8(0, 0, 0, 128);
    float blur = 16.0f;
    float spread = 4.0f;
    core::Point offset{0.0f, 8.0f};

    dl.push_shadow(rect, radii, shadow_color, blur, spread, offset);

    ASSERT_EQ(dl.size(), 1);
    auto cmds = dl.commands();
    EXPECT_EQ(cmds[0].type, CommandType::Shadow);
    EXPECT_EQ(cmds[0].shadow.bounds, rect);
    EXPECT_FLOAT_EQ(cmds[0].shadow.blur_radius, 16.0f);
    EXPECT_FLOAT_EQ(cmds[0].shadow.spread, 4.0f);
    EXPECT_FLOAT_EQ(cmds[0].shadow.offset.y, 8.0f);
}

TEST_F(DisplayListTestFixture, ClipStackAccumulationAndRestoration) {
    core::Rect initial{0.0f, 0.0f, 1000.0f, 1000.0f};
    DisplayList dl(*arena, initial);

    dl.push_quad(core::Rect{0.0f, 0.0f, 10.0f, 10.0f}, {}, core::Color{1, 0, 0, 1});
    EXPECT_EQ(dl.commands()[0].clip_rect, initial);

    core::Rect clip1{100.0f, 100.0f, 400.0f, 400.0f};
    dl.push_clip(clip1);
    EXPECT_EQ(dl.current_clip(), clip1);

    dl.push_quad(core::Rect{0.0f, 0.0f, 10.0f, 10.0f}, {}, core::Color{0, 1, 0, 1});
    EXPECT_EQ(dl.commands()[1].clip_rect, clip1);

    core::Rect clip2{200.0f, 200.0f, 500.0f, 500.0f};
    dl.push_clip(clip2);
    core::Rect expected_intersection{200.0f, 200.0f, 300.0f, 300.0f};
    EXPECT_EQ(dl.current_clip(), expected_intersection);

    dl.push_quad(core::Rect{0.0f, 0.0f, 10.0f, 10.0f}, {}, core::Color{0, 0, 1, 1});
    EXPECT_EQ(dl.commands()[2].clip_rect, expected_intersection);

    dl.pop_clip();
    EXPECT_EQ(dl.current_clip(), clip1);

    dl.pop_clip();
    EXPECT_EQ(dl.current_clip(), initial);

    dl.pop_clip();
    EXPECT_EQ(dl.current_clip(), initial);
}

TEST_F(DisplayListTestFixture, DynamicallyGrowsBeyondInitialCapacity) {
    DisplayList dl(*arena, core::Rect{0.0f, 0.0f, 800.0f, 600.0f});

    for (size_t i = 0; i < 200; ++i) {
        float fi = static_cast<float>(i);
        dl.push_quad(core::Rect{fi, fi, 10.0f, 10.0f}, {}, core::Color{1.0f, 1.0f, 1.0f, 1.0f});
    }

    EXPECT_EQ(dl.size(), 200);
    auto cmds = dl.commands();
    ASSERT_EQ(cmds.size(), 200);

    for (size_t i = 0; i < 200; ++i) {
        float fi = static_cast<float>(i);
        EXPECT_FLOAT_EQ(cmds[i].quad.bounds.x, fi);
        EXPECT_FLOAT_EQ(cmds[i].quad.bounds.y, fi);
    }
}

TEST_F(DisplayListTestFixture, ReusedDisplayListAfterArenaReset) {
    {
        DisplayList dl(*arena, core::Rect{0.0f, 0.0f, 800.0f, 600.0f});
        for (int i = 0; i < 50; ++i) {
            dl.push_quad(core::Rect{0, 0, 10, 10}, {}, core::Color{1, 1, 1, 1});
        }
        EXPECT_EQ(dl.size(), 50);
    }

    arena->reset();

    {
        DisplayList dl(*arena, core::Rect{0.0f, 0.0f, 800.0f, 600.0f});
        EXPECT_EQ(dl.size(), 0);
        EXPECT_TRUE(dl.empty());
        dl.push_quad(core::Rect{5, 5, 20, 20}, {}, core::Color{0, 0, 0, 1});
        EXPECT_EQ(dl.size(), 1);
    }
}

} // namespace north::render::testing
