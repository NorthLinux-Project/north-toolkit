#include "north/core/tree.hpp"
#include "north/event/input_dispatcher.hpp"
#include "north/widget/box.hpp"
#include "north/widget/button.hpp"
#include "north/widget/scroll_area.hpp"

#include <gtest/gtest.h>

using namespace north;

TEST(ScrollAreaTest, BuildAndMeasureScrollArea) {
    core::NodeTree tree;

    auto content = widget::BoxBuilder(tree).width(200.0f).height(500.0f).build();

    auto scroll_area =
        widget::ScrollAreaBuilder(tree).width(200.0f).height(200.0f).child(content).build();

    EXPECT_TRUE(scroll_area.is_valid());
    tree.set_root(scroll_area);
    tree.layout(layout::BoxConstraints::tight({200.0f, 200.0f}));

    auto* node = tree.get(scroll_area);
    ASSERT_NE(node, nullptr);
    ASSERT_NE(node->user_data, nullptr);

    auto* data = reinterpret_cast<widget::ScrollAreaData*>(node->user_data);
    EXPECT_EQ(data->kind, widget::WidgetKind::ScrollArea);
    EXPECT_FLOAT_EQ(node->layout.size.width, 200.0f);
    EXPECT_FLOAT_EQ(node->layout.size.height, 200.0f);
    EXPECT_FLOAT_EQ(data->content_size.height, 500.0f);
    EXPECT_FLOAT_EQ(data->max_scroll.y, 300.0f);
    EXPECT_FLOAT_EQ(data->scroll_offset.y, 0.0f);

    auto* child_node = tree.get(content);
    ASSERT_NE(child_node, nullptr);
    EXPECT_FLOAT_EQ(child_node->layout.offset.y, 0.0f);
}

TEST(ScrollAreaTest, ViewportClippingAndChildOffset) {
    core::NodeTree tree;

    auto content = widget::BoxBuilder(tree)
                       .width(200.0f)
                       .height(600.0f)
                       .background(core::Color{1.0f, 0.0f, 0.0f, 1.0f})
                       .build();

    auto scroll_area = widget::ScrollAreaBuilder(tree)
                           .width(200.0f)
                           .height(200.0f)
                           .scroll_offset({0.0f, 150.0f})
                           .child(content)
                           .build();

    tree.set_root(scroll_area);
    tree.layout(layout::BoxConstraints::tight({200.0f, 200.0f}));

    auto* data = reinterpret_cast<widget::ScrollAreaData*>(tree.get(scroll_area)->user_data);
    EXPECT_FLOAT_EQ(data->scroll_offset.y, 150.0f);

    auto* child_node = tree.get(content);
    ASSERT_NE(child_node, nullptr);
    EXPECT_FLOAT_EQ(child_node->layout.offset.y, -150.0f);

    core::Arena arena(1024 * 64);
    render::DisplayList dl(arena, core::Rect{0.0f, 0.0f, 800.0f, 600.0f});

    tree.paint(&dl);

    // Verify commands in the child were clipped to the scroll area viewport
    EXPECT_GT(dl.size(), 0u);
    bool found_clipped_child = false;
    for (const auto& cmd : dl.commands()) {
        if (cmd.type == render::CommandType::Quad && cmd.quad.bounds.width == 200.0f &&
            cmd.quad.bounds.height == 600.0f) {
            found_clipped_child = true;
            EXPECT_LE(cmd.clip_rect.width, 200.1f);
            EXPECT_LE(cmd.clip_rect.height, 200.1f);
        }
    }
    EXPECT_TRUE(found_clipped_child);
}

TEST(ScrollAreaTest, WheelScrollClampingAndDeltas) {
    core::NodeTree tree;

    struct ScrollEvent {
        core::Point offset{};
        int count{0};
    } scroll_evt;

    auto content = widget::BoxBuilder(tree).width(200.0f).height(400.0f).build();

    auto scroll_area = widget::ScrollAreaBuilder(tree)
                           .width(200.0f)
                           .height(200.0f)
                           .child(content)
                           .on_scroll(
                               [](core::Point offset, void* ud) {
                                   auto* ev = static_cast<ScrollEvent*>(ud);
                                   ev->offset = offset;
                                   ev->count++;
                               },
                               &scroll_evt)
                           .build();

    tree.set_root(scroll_area);
    tree.layout(layout::BoxConstraints::tight({200.0f, 200.0f}));

    event::InputDispatcher dispatcher(tree);

    // Scroll down by 48px
    dispatcher.on_scroll(0.0f, -48.0f, {50.0f, 50.0f});
    auto* data = reinterpret_cast<widget::ScrollAreaData*>(tree.get(scroll_area)->user_data);
    EXPECT_FLOAT_EQ(data->scroll_offset.y, 48.0f);
    EXPECT_EQ(scroll_evt.count, 1);
    EXPECT_FLOAT_EQ(scroll_evt.offset.y, 48.0f);

    // Scroll further past max_scroll (max is 200)
    dispatcher.on_scroll(0.0f, -300.0f, {50.0f, 50.0f});
    EXPECT_FLOAT_EQ(data->scroll_offset.y, 200.0f);

    // Scroll up past 0
    dispatcher.on_scroll(0.0f, 500.0f, {50.0f, 50.0f});
    EXPECT_FLOAT_EQ(data->scroll_offset.y, 0.0f);
}

TEST(ScrollAreaTest, ScrolledChildHitTesting) {
    core::NodeTree tree;

    static int btn_clicks = 0;
    btn_clicks = 0;

    auto btn = widget::ButtonBuilder(tree, "Target Button")
                   .padding(core::Edges::uniform(10.0f))
                   .on_click([](void*) { btn_clicks++; })
                   .build();

    auto spacer = widget::BoxBuilder(tree).width(200.0f).height(300.0f).build();

    auto bottom_spacer = widget::BoxBuilder(tree).width(200.0f).height(300.0f).build();

    auto content = widget::BoxBuilder(tree)
                       .flex(layout::FlexDirection::Column)
                       .child(spacer)
                       .child(btn)
                       .child(bottom_spacer)
                       .build();

    auto scroll_area = widget::ScrollAreaBuilder(tree)
                           .width(200.0f)
                           .height(200.0f)
                           .scroll_offset({0.0f, 250.0f})
                           .child(content)
                           .build();

    tree.set_root(scroll_area);
    tree.layout(layout::BoxConstraints::tight({200.0f, 200.0f}));

    // With scroll_offset 250, button at content-relative 300 is at screen y = 300 - 250 = 50
    event::InputDispatcher dispatcher(tree);

    platform::PointerEvent down_evt{.action = platform::PointerAction::Down,
                                    .button = platform::PointerButton::Left,
                                    .position = {50.0f, 65.0f}};
    dispatcher.on_pointer(down_evt);

    platform::PointerEvent up_evt{.action = platform::PointerAction::Up,
                                  .button = platform::PointerButton::Left,
                                  .position = {50.0f, 65.0f}};
    dispatcher.on_pointer(up_evt);

    EXPECT_EQ(btn_clicks, 1);
}

TEST(ScrollAreaTest, ScrollbarLightModeThemeContrast) {
    core::NodeTree tree;

    auto sa_light = widget::ScrollAreaBuilder(tree).theme(style::Theme::light()).build();

    auto* node_light = tree.get(sa_light);
    ASSERT_NE(node_light, nullptr);
    auto* data_light = reinterpret_cast<widget::ScrollAreaData*>(node_light->user_data);
    ASSERT_NE(data_light, nullptr);

    const auto& light_colors = style::Theme::light().colors();
    EXPECT_FLOAT_EQ(data_light->scrollbar_style.thumb_color.r, light_colors.on_surface.r);
    EXPECT_FLOAT_EQ(data_light->scrollbar_style.thumb_color.g, light_colors.on_surface.g);
    EXPECT_FLOAT_EQ(data_light->scrollbar_style.thumb_color.b, light_colors.on_surface.b);
    EXPECT_FLOAT_EQ(data_light->scrollbar_style.thumb_color.a, 0.45f);

    // Verify blended thumb has strong contrast on light surface
    core::Color blended_light{
        data_light->scrollbar_style.thumb_color.r * 0.45f + light_colors.surface.r * 0.55f,
        data_light->scrollbar_style.thumb_color.g * 0.45f + light_colors.surface.g * 0.55f,
        data_light->scrollbar_style.thumb_color.b * 0.45f + light_colors.surface.b * 0.55f, 1.0f};
    float contrast = style::Theme::contrast_ratio(blended_light, light_colors.surface);
    EXPECT_GT(contrast, 2.0f);
}
