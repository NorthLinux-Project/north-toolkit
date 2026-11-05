#include "north/core/arena.hpp"
#include "north/core/tree.hpp"
#include "north/event/input_dispatcher.hpp"
#include "north/render/display_list.hpp"
#include "north/style/theme.hpp"
#include "north/widget/accordion.hpp"
#include "north/widget/box.hpp"
#include "north/widget/segmented_button.hpp"
#include "north/widget/tabs.hpp"
#include "north/widget/text_area.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <gtest/gtest.h>

using namespace north;

TEST(TabBarTest, LayoutAndIndicatorMetrics) {
    core::NodeTree tree;
    auto bar = widget::TabBarBuilder(tree)
                   .tab("main.cpp", true, true, "M 0 0 L 10 0 L 10 10 Z")
                   .tab("renderer.hpp", true, false)
                   .tab("settings.json", false, false)
                   .active_tab(1)
                   .add_button(true)
                   .bar_height(42.0f)
                   .theme(style::Theme::dark())
                   .build();

    tree.set_root(bar);
    tree.layout(layout::BoxConstraints::tight({800.0f, 42.0f}));

    auto* node = tree.get(bar);
    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->layout.size.width, 800.0f);
    EXPECT_FLOAT_EQ(node->layout.size.height, 42.0f);

    auto* data = reinterpret_cast<widget::TabBarData*>(node->user_data);
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->tabs.size(), 3u);
    EXPECT_EQ(data->active_index, 1u);
    EXPECT_GT(data->tabs[0].computed_width, 0.0f);
    EXPECT_FLOAT_EQ(data->tabs[1].computed_x, data->tabs[0].computed_width);
    EXPECT_FLOAT_EQ(data->indicator_x.get(), data->tabs[1].computed_x);
    EXPECT_FLOAT_EQ(data->indicator_width.get(), data->tabs[1].computed_width);

    core::Arena arena(16384);
    render::DisplayList dl(arena, {0.0f, 0.0f, 800.0f, 42.0f});
    tree.paint(&dl);
    EXPECT_GT(dl.size(), 0u);
}

TEST(TabBarTest, SelectionCloseAndAddCallbacks) {
    struct CallbackState {
        size_t last_selected{SIZE_MAX};
        size_t last_closed{SIZE_MAX};
        int add_clicks{0};
    } state;

    core::NodeTree tree;
    auto bar = widget::TabBarBuilder(tree)
                   .tab("Tab 0", true)
                   .tab("Tab 1", true)
                   .tab("Tab 2", false)
                   .active_tab(0)
                   .add_button(true)
                   .on_select(
                       [](size_t idx, void* ud) {
                           static_cast<CallbackState*>(ud)->last_selected = idx;
                       },
                       &state)
                   .on_close(
                       [](size_t idx, void* ud) {
                           static_cast<CallbackState*>(ud)->last_closed = idx;
                       },
                       &state)
                   .on_add([](void* ud) { static_cast<CallbackState*>(ud)->add_clicks++; }, &state)
                   .build();

    tree.set_root(bar);
    tree.layout(layout::BoxConstraints::tight({600.0f, 40.0f}));

    auto* data = reinterpret_cast<widget::TabBarData*>(tree.get(bar)->user_data);
    ASSERT_NE(data, nullptr);

    data->select_tab(2);
    EXPECT_EQ(data->active_index, 2u);
    EXPECT_EQ(state.last_selected, 2u);

    EXPECT_FALSE(data->close_tab(2));
    EXPECT_EQ(data->tabs.size(), 3u);

    EXPECT_TRUE(data->close_tab(1));
    EXPECT_EQ(state.last_closed, 1u);
    EXPECT_EQ(data->tabs.size(), 2u);
    EXPECT_EQ(data->active_index, 1u);
}

TEST(TabBarTest, DragReorderingAndActiveIndexTracking) {
    struct ReorderLog {
        size_t from{SIZE_MAX};
        size_t to{SIZE_MAX};
    } log;

    core::NodeTree tree;
    auto bar = widget::TabBarBuilder(tree)
                   .tab("Alpha")
                   .tab("Beta")
                   .tab("Gamma")
                   .active_tab(0)
                   .reorderable(true)
                   .on_reorder(
                       [](size_t f, size_t t, void* ud) {
                           auto* r = static_cast<ReorderLog*>(ud);
                           r->from = f;
                           r->to = t;
                       },
                       &log)
                   .build();

    tree.set_root(bar);
    tree.layout(layout::BoxConstraints::tight({600.0f, 40.0f}));

    auto* data = reinterpret_cast<widget::TabBarData*>(tree.get(bar)->user_data);
    ASSERT_NE(data, nullptr);

    EXPECT_TRUE(data->reorder_tab(0, 2));
    EXPECT_EQ(log.from, 0u);
    EXPECT_EQ(log.to, 2u);
    EXPECT_EQ(data->tabs[0].label, "Beta");
    EXPECT_EQ(data->tabs[1].label, "Gamma");
    EXPECT_EQ(data->tabs[2].label, "Alpha");
    EXPECT_EQ(data->active_index, 2u);
}

TEST(TabsViewTest, KeepAlivePreservesInactiveTabNodes) {
    core::NodeTree tree;
    auto editor_a = widget::TextAreaBuilder(tree).text("Buffer A Content").build();
    auto editor_b = widget::TextAreaBuilder(tree).text("Buffer B Content").build();

    auto tabs_view = widget::TabsViewBuilder(tree)
                         .tab("FileA.cpp", editor_a)
                         .tab("FileB.cpp", editor_b)
                         .retention_policy(widget::TabRetentionPolicy::KeepAlive)
                         .active_tab(0)
                         .width(640.0f)
                         .height(420.0f)
                         .build();

    tree.set_root(tabs_view);
    tree.layout(layout::BoxConstraints::tight({640.0f, 420.0f}));

    auto* node_a = tree.get(editor_a);
    auto* node_b = tree.get(editor_b);
    ASSERT_NE(node_a, nullptr);
    ASSERT_NE(node_b, nullptr);

    EXPECT_GT(node_a->layout.size.width, 0.0f);
    EXPECT_GT(node_a->layout.size.height, 0.0f);
    EXPECT_FLOAT_EQ(node_b->layout.size.width, 0.0f);
    EXPECT_FLOAT_EQ(node_b->layout.size.height, 0.0f);

    auto* data_a = reinterpret_cast<widget::TextAreaData*>(node_a->user_data);
    data_a->editor.insert(" - Modified");

    auto* view_data = reinterpret_cast<widget::TabsViewData*>(tree.get(tabs_view)->user_data);
    ASSERT_NE(view_data->bar_data, nullptr);
    view_data->bar_data->select_tab(1);
    tree.layout(layout::BoxConstraints::tight({640.0f, 420.0f}));

    EXPECT_FLOAT_EQ(node_a->layout.size.width, 0.0f);
    EXPECT_GT(node_b->layout.size.width, 0.0f);

    view_data->bar_data->select_tab(0);
    tree.layout(layout::BoxConstraints::tight({640.0f, 420.0f}));
    EXPECT_EQ(data_a->editor.text(), "Buffer A Content - Modified");
}

TEST(TabsViewTest, LazyRetentionPolicyMountsAndUnmountsOnSwitch) {
    struct MountCounter {
        int mounts{0};
    } counter;

    auto factory = [](core::NodeTree& t, size_t, void* ud) -> core::Handle<core::Node> {
        static_cast<MountCounter*>(ud)->mounts++;
        return widget::BoxBuilder(t).size({200.0f, 150.0f}).build();
    };

    core::NodeTree tree;
    auto tabs_view = widget::TabsViewBuilder(tree)
                         .retention_policy(widget::TabRetentionPolicy::Lazy)
                         .lazy_tab("Lazy 0", factory, &counter)
                         .lazy_tab("Lazy 1", factory, &counter)
                         .active_tab(0)
                         .build();

    tree.set_root(tabs_view);
    tree.layout(layout::BoxConstraints::tight({500.0f, 300.0f}));

    auto* view_data = reinterpret_cast<widget::TabsViewData*>(tree.get(tabs_view)->user_data);
    ASSERT_NE(view_data->bar_data, nullptr);

    EXPECT_EQ(counter.mounts, 1);
    EXPECT_TRUE(view_data->bar_data->tabs[0].content_node.is_valid());
    EXPECT_FALSE(view_data->bar_data->tabs[1].content_node.is_valid());

    view_data->bar_data->select_tab(1);
    tree.layout(layout::BoxConstraints::tight({500.0f, 300.0f}));

    EXPECT_EQ(counter.mounts, 2);
    EXPECT_FALSE(view_data->bar_data->tabs[0].content_node.is_valid());
    EXPECT_TRUE(view_data->bar_data->tabs[1].content_node.is_valid());
}

TEST(TabBarInputTest, PointerClickDragReorderAndCtrlTabKeyboard) {
    core::NodeTree tree;
    auto bar = widget::TabBarBuilder(tree)
                   .tab("First")
                   .tab("Second")
                   .tab("Third")
                   .active_tab(0)
                   .reorderable(true)
                   .build();

    tree.set_root(bar);
    tree.layout(layout::BoxConstraints::tight({600.0f, 40.0f}));

    event::InputDispatcher dispatcher(tree);
    auto* data = reinterpret_cast<widget::TabBarData*>(tree.get(bar)->user_data);

    float second_tab_center_x =
        data->tabs[1].computed_x + data->tabs[1].computed_width * 0.5f;
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Down,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {second_tab_center_x, 20.0f}});
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Up,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {second_tab_center_x, 20.0f}});
    EXPECT_EQ(data->active_index, 1u);

    dispatcher.on_key(platform::KeyEvent{.key_code = VK_TAB,
                                         .character = 0,
                                         .is_down = true,
                                         .modifiers = platform::kModControl});
    EXPECT_EQ(data->active_index, 2u);

    dispatcher.on_key(platform::KeyEvent{.key_code = VK_TAB,
                                         .character = 0,
                                         .is_down = true,
                                         .modifiers = platform::kModControl | platform::kModShift});
    EXPECT_EQ(data->active_index, 1u);

    float first_x = data->tabs[0].computed_x + 20.0f;
    float third_end_x = data->tabs[2].computed_x + data->tabs[2].computed_width - 5.0f;
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Down,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {first_x, 20.0f}});
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Move,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {third_end_x, 20.0f}});
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Up,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {third_end_x, 20.0f}});

    EXPECT_EQ(data->tabs[2].label, "First");
}

TEST(SegmentedButtonTest, SingleSelectAndMultiSelectModes) {
    core::NodeTree tree;
    auto single_seg = widget::SegmentedButtonBuilder(tree)
                          .segment("Code")
                          .segment("Split")
                          .segment("Preview")
                          .selected(0)
                          .multi_select(false)
                          .theme(style::Theme::dark())
                          .build();

    tree.set_root(single_seg);
    tree.layout(layout::BoxConstraints::tight({300.0f, 36.0f}));

    auto* s_data = reinterpret_cast<widget::SegmentedButtonData*>(tree.get(single_seg)->user_data);
    ASSERT_NE(s_data, nullptr);
    EXPECT_EQ(s_data->selected_index(), 0u);

    s_data->toggle_segment(2);
    EXPECT_FALSE(s_data->is_selected(0));
    EXPECT_TRUE(s_data->is_selected(2));
    EXPECT_EQ(s_data->selected_index(), 2u);

    auto multi_seg = widget::SegmentedButtonBuilder(tree)
                         .multi_select(true)
                         .segment("Bold")
                         .segment("Italic")
                         .segment("Underline")
                         .selected(0, true)
                         .selected(1, true)
                         .build();

    auto* m_data = reinterpret_cast<widget::SegmentedButtonData*>(tree.get(multi_seg)->user_data);
    ASSERT_NE(m_data, nullptr);
    EXPECT_TRUE(m_data->is_selected(0));
    EXPECT_TRUE(m_data->is_selected(1));
    EXPECT_FALSE(m_data->is_selected(2));

    m_data->toggle_segment(1);
    EXPECT_TRUE(m_data->is_selected(0));
    EXPECT_FALSE(m_data->is_selected(1));
}

TEST(SegmentedButtonTest, PointerAndKeyboardInteraction) {
    core::NodeTree tree;
    auto seg = widget::SegmentedButtonBuilder(tree)
                   .segment("Day")
                   .segment("Week")
                   .segment("Month")
                   .selected(0)
                   .width(300.0f)
                   .height(36.0f)
                   .build();

    tree.set_root(seg);
    tree.layout(layout::BoxConstraints::tight({300.0f, 36.0f}));

    event::InputDispatcher dispatcher(tree);
    auto* data = reinterpret_cast<widget::SegmentedButtonData*>(tree.get(seg)->user_data);

    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Down,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {150.0f, 18.0f}});
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Up,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {150.0f, 18.0f}});
    EXPECT_EQ(data->selected_index(), 1u);

    dispatcher.on_key(
        platform::KeyEvent{.key_code = VK_RIGHT, .character = 0, .is_down = true, .modifiers = 0});
    EXPECT_EQ(data->selected_index(), 2u);

    core::Arena arena(8192);
    render::DisplayList dl(arena, {0.0f, 0.0f, 300.0f, 36.0f});
    tree.paint(&dl);
    EXPECT_GT(dl.size(), 0u);
}

TEST(AccordionTest, SingleAndMultiExpandModesAndHeightLayout) {
    core::NodeTree tree;
    auto body0 = widget::BoxBuilder(tree).size({260.0f, 80.0f}).build();
    auto body1 = widget::BoxBuilder(tree).size({260.0f, 100.0f}).build();

    auto acc = widget::AccordionBuilder(tree)
                   .allow_multiple(false)
                   .header_height(40.0f)
                   .width(300.0f)
                   .item("Section 1", body0, true)
                   .item("Section 2", body1, false)
                   .theme(style::Theme::dark())
                   .build();

    tree.set_root(acc);
    tree.layout(layout::BoxConstraints::loose({300.0f, 600.0f}));

    auto* acc_node = tree.get(acc);
    auto* data = reinterpret_cast<widget::AccordionData*>(acc_node->user_data);
    ASSERT_NE(data, nullptr);

    EXPECT_TRUE(data->is_expanded(0));
    EXPECT_FALSE(data->is_expanded(1));
    float expected_h_0 = 40.0f * 2.0f + 80.0f + data->content_padding.vertical();
    EXPECT_FLOAT_EQ(acc_node->layout.size.height, expected_h_0);

    data->set_expanded(1, true, false);
    tree.layout(layout::BoxConstraints::loose({300.0f, 600.0f}));

    EXPECT_FALSE(data->is_expanded(0));
    EXPECT_TRUE(data->is_expanded(1));
    float expected_h_1 = 40.0f * 2.0f + 100.0f + data->content_padding.vertical();
    EXPECT_FLOAT_EQ(acc_node->layout.size.height, expected_h_1);
}

TEST(AccordionTest, PointerHeaderClickAndDisplayListPainting) {
    core::NodeTree tree;
    auto body0 = widget::BoxBuilder(tree).size({240.0f, 60.0f}).build();
    auto body1 = widget::BoxBuilder(tree).size({240.0f, 60.0f}).build();

    auto acc = widget::AccordionBuilder(tree)
                   .allow_multiple(true)
                   .header_height(40.0f)
                   .width(300.0f)
                   .item("Panel A", body0, false)
                   .item("Panel B", body1, false)
                   .build();

    tree.set_root(acc);
    tree.layout(layout::BoxConstraints::loose({300.0f, 500.0f}));

    event::InputDispatcher dispatcher(tree);
    auto* data = reinterpret_cast<widget::AccordionData*>(tree.get(acc)->user_data);

    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Down,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {100.0f, 20.0f}});
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Up,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {100.0f, 20.0f}});
    EXPECT_TRUE(data->is_expanded(0));

    core::Arena arena(16384);
    render::DisplayList dl(arena, {0.0f, 0.0f, 300.0f, 500.0f});
    tree.paint(&dl);
    EXPECT_GT(dl.size(), 0u);
}
