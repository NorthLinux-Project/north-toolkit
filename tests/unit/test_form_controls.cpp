#include "north/core/tree.hpp"
#include "north/event/input_dispatcher.hpp"
#include "north/widget/box.hpp"
#include "north/widget/button.hpp"
#include "north/widget/checkbox.hpp"
#include "north/widget/radio_button.hpp"
#include "north/widget/slider.hpp"
#include "north/widget/switch.hpp"
#include "north/widget/text.hpp"
#include "north/widget/text_input.hpp"

#include <gtest/gtest.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

using namespace north;

TEST(FormControlsTest, CheckboxToggleAndSignalSync) {
    core::NodeTree tree;
    core::Signal<bool> is_checked{false};
    bool callback_invoked = false;

    auto cb_node =
        widget::CheckboxBuilder(tree)
            .bind(is_checked)
            .label("Accept Terms")
            .on_change([](bool, void* ud) { *static_cast<bool*>(ud) = true; }, &callback_invoked)
            .build();

    tree.set_root(cb_node);
    tree.layout(layout::BoxConstraints::tight({200.0f, 30.0f}));

    auto* data = reinterpret_cast<widget::CheckboxData*>(tree.get(cb_node)->user_data);
    ASSERT_NE(data, nullptr);
    EXPECT_FALSE(data->checked);
    EXPECT_FLOAT_EQ(data->check_progress.get(), 0.0f);

    event::InputDispatcher dispatcher(tree);

    // Pointer click to toggle ON
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Down,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {10.0f, 15.0f}});
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Up,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {10.0f, 15.0f}});

    EXPECT_TRUE(data->checked);
    EXPECT_TRUE(is_checked.get());
    EXPECT_TRUE(callback_invoked);
    EXPECT_TRUE(data->check_progress.is_animating());

    // External signal change to toggle OFF
    is_checked.set(false);
    EXPECT_FALSE(data->checked);
    EXPECT_TRUE(data->check_progress.is_animating());
}

TEST(FormControlsTest, SwitchSpringAnimationAndChangeCallback) {
    core::NodeTree tree;
    core::Signal<bool> switch_state{false};
    int change_count = 0;

    auto sw_node = widget::SwitchBuilder(tree)
                       .bind(switch_state)
                       .label("Dark Mode")
                       .on_change([](bool, void* ud) { (*static_cast<int*>(ud))++; }, &change_count)
                       .build();

    tree.set_root(sw_node);
    tree.layout(layout::BoxConstraints::tight({160.0f, 30.0f}));

    auto* data = reinterpret_cast<widget::SwitchData*>(tree.get(sw_node)->user_data);
    ASSERT_NE(data, nullptr);
    EXPECT_FALSE(data->checked);
    EXPECT_FLOAT_EQ(data->thumb_position.get(), 0.0f);

    event::InputDispatcher dispatcher(tree);

    // Click on switch track
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Down,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {20.0f, 15.0f}});
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Up,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {20.0f, 15.0f}});

    EXPECT_TRUE(data->checked);
    EXPECT_TRUE(switch_state.get());
    EXPECT_EQ(change_count, 1);
    EXPECT_TRUE(data->thumb_position.is_animating());

    // Advance spring physics
    animation::AnimationManager::instance().tick(0.05f);
    EXPECT_GT(data->thumb_position.get(), 0.0f);

    // Settle animation
    for (int i = 0; i < 60 && data->thumb_position.is_animating(); ++i) {
        animation::AnimationManager::instance().tick(0.016f);
    }
    EXPECT_FALSE(data->thumb_position.is_animating());
    EXPECT_NEAR(data->thumb_position.get(), 1.0f, 0.01f);
}

TEST(FormControlsTest, SliderContinuousAndSteppedSeeking) {
    core::NodeTree tree;
    core::Signal<float> slider_val{25.0f};

    // Continuous slider
    auto sl_node =
        widget::SliderBuilder(tree).range(0.0f, 100.0f).bind(slider_val).width(200.0f).build();

    tree.set_root(sl_node);
    tree.layout(layout::BoxConstraints::tight({200.0f, 24.0f}));

    auto* data = reinterpret_cast<widget::SliderData*>(tree.get(sl_node)->user_data);
    ASSERT_NE(data, nullptr);
    EXPECT_FLOAT_EQ(data->value, 25.0f);

    event::InputDispatcher dispatcher(tree);

    // Seek to center: local_x = 100.0f -> (100 - 8) / (200 - 16) = 92 / 184 = 0.5 -> 50.0f
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Down,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {100.0f, 12.0f}});
    EXPECT_NEAR(data->value, 50.0f, 0.5f);
    EXPECT_NEAR(slider_val.get(), 50.0f, 0.5f);

    // Drag to right edge: local_x = 200.0f -> 100.0f
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Move,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {200.0f, 12.0f}});
    EXPECT_FLOAT_EQ(data->value, 100.0f);

    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Up,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {200.0f, 12.0f}});
    EXPECT_FALSE(data->is_dragging);

    // Stepped slider: range 0..10, step 2
    auto step_node =
        widget::SliderBuilder(tree).range(0.0f, 10.0f).step(2.0f).value(0.0f).width(200.0f).build();

    tree.set_root(step_node);
    tree.layout(layout::BoxConstraints::tight({200.0f, 24.0f}));
    auto* step_data = reinterpret_cast<widget::SliderData*>(tree.get(step_node)->user_data);

    // Drag to ~3.5 -> should snap to 4.0
    // local_x for 3.5: t = 0.35 -> local_x = 8 + 0.35 * 184 = 72.4
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Down,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {72.4f, 12.0f}});
    EXPECT_FLOAT_EQ(step_data->value, 4.0f);
}

TEST(FormControlsTest, RadioButtonGroupMutualExclusivity) {
    core::NodeTree tree;
    core::Signal<int> selected_option{1};

    auto rb1 =
        widget::RadioButtonBuilder(tree).value(1).group(selected_option).label("Option 1").build();

    auto rb2 =
        widget::RadioButtonBuilder(tree).value(2).group(selected_option).label("Option 2").build();

    auto rb3 =
        widget::RadioButtonBuilder(tree).value(3).group(selected_option).label("Option 3").build();

    auto group_box = widget::BoxBuilder(tree)
                         .flex(layout::FlexDirection::Column, layout::MainAxisAlignment::Start,
                               layout::CrossAxisAlignment::Start, 8.0f)
                         .child(rb1)
                         .child(rb2)
                         .child(rb3)
                         .build();

    tree.set_root(group_box);
    tree.layout(layout::BoxConstraints::tight({200.0f, 100.0f}));

    auto* d1 = reinterpret_cast<widget::RadioButtonData*>(tree.get(rb1)->user_data);
    auto* d2 = reinterpret_cast<widget::RadioButtonData*>(tree.get(rb2)->user_data);
    auto* d3 = reinterpret_cast<widget::RadioButtonData*>(tree.get(rb3)->user_data);

    EXPECT_TRUE(d1->selected);
    EXPECT_FALSE(d2->selected);
    EXPECT_FALSE(d3->selected);

    event::InputDispatcher dispatcher(tree);

    // Click Option 3
    auto* n3 = tree.get(rb3);
    float rb3_y = n3->layout.offset.y + 9.0f;
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Down,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {9.0f, rb3_y}});
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Up,
                                                 .button = platform::PointerButton::Left,
                                                 .position = {9.0f, rb3_y}});

    EXPECT_EQ(selected_option.get(), 3);
    EXPECT_FALSE(d1->selected);
    EXPECT_FALSE(d2->selected);
    EXPECT_TRUE(d3->selected);
    EXPECT_TRUE(d3->dot_scale.is_animating());
}

TEST(FormControlsTest, FormControlsTabFocusCycle) {
    core::NodeTree tree;

    auto btn = widget::ButtonBuilder(tree, "Submit").build();
    auto ti = widget::TextInputBuilder(tree).text("Input").build();
    auto cb = widget::CheckboxBuilder(tree).label("Check").build();
    auto sw = widget::SwitchBuilder(tree).label("Toggle").build();
    auto sl = widget::SliderBuilder(tree).width(100.0f).build();
    auto rb = widget::RadioButtonBuilder(tree).label("Radio").build();

    auto container = widget::BoxBuilder(tree)
                         .flex(layout::FlexDirection::Column)
                         .child(btn)
                         .child(ti)
                         .child(cb)
                         .child(sw)
                         .child(sl)
                         .child(rb)
                         .build();

    tree.set_root(container);
    tree.layout(layout::BoxConstraints::tight({300.0f, 300.0f}));

    event::InputDispatcher dispatcher(tree);

    // Initial focus on Tab: first focusable should be Button
    platform::KeyEvent tab_down{
        .key_code = VK_TAB, .character = 0, .is_down = true, .modifiers = 0};
    dispatcher.on_key(tab_down);
    EXPECT_EQ(dispatcher.focused_node(), btn);

    // Next Tab -> TextInput
    dispatcher.on_key(tab_down);
    EXPECT_EQ(dispatcher.focused_node(), ti);

    // Next Tab -> Checkbox
    dispatcher.on_key(tab_down);
    EXPECT_EQ(dispatcher.focused_node(), cb);

    // Next Tab -> Switch
    dispatcher.on_key(tab_down);
    EXPECT_EQ(dispatcher.focused_node(), sw);

    // Next Tab -> Slider
    dispatcher.on_key(tab_down);
    EXPECT_EQ(dispatcher.focused_node(), sl);

    // Next Tab -> RadioButton
    dispatcher.on_key(tab_down);
    EXPECT_EQ(dispatcher.focused_node(), rb);

    // Next Tab -> wraps back to Button
    dispatcher.on_key(tab_down);
    EXPECT_EQ(dispatcher.focused_node(), btn);

    // Shift+Tab backward -> RadioButton
    platform::KeyEvent shift_tab{
        .key_code = VK_TAB, .character = 0, .is_down = true, .modifiers = platform::kModShift};
    dispatcher.on_key(shift_tab);
    EXPECT_EQ(dispatcher.focused_node(), rb);
}

TEST(FormControlsTest, KeyboardActivationAndArrowAdjustments) {
    core::NodeTree tree;

    auto cb = widget::CheckboxBuilder(tree).checked(false).build();
    auto sw = widget::SwitchBuilder(tree).checked(false).build();
    auto sl = widget::SliderBuilder(tree).range(0.0f, 100.0f).step(5.0f).value(50.0f).build();
    auto rb = widget::RadioButtonBuilder(tree).selected(false).build();

    auto container = widget::BoxBuilder(tree)
                         .flex(layout::FlexDirection::Column)
                         .child(cb)
                         .child(sw)
                         .child(sl)
                         .child(rb)
                         .build();

    tree.set_root(container);
    tree.layout(layout::BoxConstraints::tight({300.0f, 200.0f}));

    event::InputDispatcher dispatcher(tree);

    // Focus Checkbox and press Space
    dispatcher.set_focus(cb);
    auto* cb_data = reinterpret_cast<widget::CheckboxData*>(tree.get(cb)->user_data);
    EXPECT_FALSE(cb_data->checked);
    dispatcher.on_key(
        platform::KeyEvent{.key_code = VK_SPACE, .character = 0, .is_down = true, .modifiers = 0});
    EXPECT_TRUE(cb_data->checked);

    // Focus Switch and press Space
    dispatcher.set_focus(sw);
    auto* sw_data = reinterpret_cast<widget::SwitchData*>(tree.get(sw)->user_data);
    EXPECT_FALSE(sw_data->checked);
    dispatcher.on_key(
        platform::KeyEvent{.key_code = VK_SPACE, .character = 0, .is_down = true, .modifiers = 0});
    EXPECT_TRUE(sw_data->checked);

    // Focus Radio and press Enter
    dispatcher.set_focus(rb);
    auto* rb_data = reinterpret_cast<widget::RadioButtonData*>(tree.get(rb)->user_data);
    EXPECT_FALSE(rb_data->selected);
    dispatcher.on_key(
        platform::KeyEvent{.key_code = VK_RETURN, .character = 0, .is_down = true, .modifiers = 0});
    EXPECT_TRUE(rb_data->selected);

    // Focus Slider and adjust with Arrows
    dispatcher.set_focus(sl);
    auto* sl_data = reinterpret_cast<widget::SliderData*>(tree.get(sl)->user_data);
    EXPECT_FLOAT_EQ(sl_data->value, 50.0f);

    // Right arrow -> 55.0
    dispatcher.on_key(
        platform::KeyEvent{.key_code = VK_RIGHT, .character = 0, .is_down = true, .modifiers = 0});
    EXPECT_FLOAT_EQ(sl_data->value, 55.0f);

    // Left arrow -> 50.0
    dispatcher.on_key(
        platform::KeyEvent{.key_code = VK_LEFT, .character = 0, .is_down = true, .modifiers = 0});
    EXPECT_FLOAT_EQ(sl_data->value, 50.0f);

    // Home -> 0.0
    dispatcher.on_key(
        platform::KeyEvent{.key_code = VK_HOME, .character = 0, .is_down = true, .modifiers = 0});
    EXPECT_FLOAT_EQ(sl_data->value, 0.0f);

    // End -> 100.0
    dispatcher.on_key(
        platform::KeyEvent{.key_code = VK_END, .character = 0, .is_down = true, .modifiers = 0});
    EXPECT_FLOAT_EQ(sl_data->value, 100.0f);
}

TEST(FormControlsTest, FullDemoTreeInteractions) {
    core::NodeTree tree;
    core::Signal<int> counter{0};
    core::Signal<bool> is_dark{true};
    core::Signal<bool> notifications{true};
    core::Signal<float> slider_val{45.0f};
    core::Signal<int> radio_choice{0};
    std::string input_text{"North UI Toolkit"};

    auto rebuild = [&]() {
        if (tree.root().is_valid()) {
            tree.destroy_node(tree.root());
        }

        const auto& theme = is_dark.get() ? style::Theme::dark() : style::Theme::light();
        const auto& c = theme.colors();

        std::string count_str = "Count: " + std::to_string(counter.get());
        auto count_text =
            widget::TextBuilder(tree, count_str).font_size(28.0f).color(c.on_surface).build();

        auto dec_btn = widget::ButtonBuilder(tree, "-")
                           .theme(theme)
                           .padding(core::Edges{8.0f, 18.0f, 8.0f, 18.0f})
                           .on_click(
                               [](void* ud) {
                                   auto* sig = static_cast<core::Signal<int>*>(ud);
                                   sig->set(sig->get() - 1);
                               },
                               &counter)
                           .build();

        auto inc_btn = widget::ButtonBuilder(tree, "+")
                           .theme(theme)
                           .padding(core::Edges{8.0f, 18.0f, 8.0f, 18.0f})
                           .on_click(
                               [](void* ud) {
                                   auto* sig = static_cast<core::Signal<int>*>(ud);
                                   sig->set(sig->get() + 1);
                               },
                               &counter)
                           .build();

        auto theme_switch =
            widget::SwitchBuilder(tree).label("Dark Theme").theme(theme).bind(is_dark).build();

        auto button_row = widget::BoxBuilder(tree)
                              .flex(layout::FlexDirection::Row, layout::MainAxisAlignment::Center,
                                    layout::CrossAxisAlignment::Center, 10.0f)
                              .child(dec_btn)
                              .child(inc_btn)
                              .child(theme_switch)
                              .build();

        auto checkbox = widget::CheckboxBuilder(tree)
                            .label("Notifications")
                            .theme(theme)
                            .bind(notifications)
                            .build();

        auto radio_a = widget::RadioButtonBuilder(tree)
                           .label("Mode A")
                           .value(0)
                           .theme(theme)
                           .group(radio_choice)
                           .build();

        auto radio_b = widget::RadioButtonBuilder(tree)
                           .label("Mode B")
                           .value(1)
                           .theme(theme)
                           .group(radio_choice)
                           .build();

        auto options_row = widget::BoxBuilder(tree)
                               .flex(layout::FlexDirection::Row, layout::MainAxisAlignment::Center,
                                     layout::CrossAxisAlignment::Center, 14.0f)
                               .child(checkbox)
                               .child(radio_a)
                               .child(radio_b)
                               .build();

        auto slider = widget::SliderBuilder(tree)
                          .theme(theme)
                          .range(0.0f, 100.0f)
                          .bind(slider_val)
                          .width(240.0f)
                          .build();

        auto card = widget::BoxBuilder(tree)
                        .flex(layout::FlexDirection::Column, layout::MainAxisAlignment::Center,
                              layout::CrossAxisAlignment::Center, 12.0f)
                        .padding(core::Edges::uniform(24.0f))
                        .background(c.surface)
                        .child(count_text)
                        .child(button_row)
                        .child(options_row)
                        .child(slider)
                        .build();

        auto root = widget::BoxBuilder(tree)
                        .flex(layout::FlexDirection::Column, layout::MainAxisAlignment::Center,
                              layout::CrossAxisAlignment::Center)
                        .background(c.background)
                        .child(card)
                        .build();

        tree.set_root(root);
        tree.layout(layout::BoxConstraints::tight({680.0f, 680.0f}));
    };

    struct RebuildCtx {
        decltype(rebuild)* fn;
    } rctx{&rebuild};

    counter.bind_dirty([](void* ud, uint32_t) { (*static_cast<RebuildCtx*>(ud)->fn)(); }, &rctx);

    is_dark.bind_dirty([](void* ud, uint32_t) { (*static_cast<RebuildCtx*>(ud)->fn)(); }, &rctx);

    rebuild();

    event::InputDispatcher dispatcher(tree);

    // Sequence 1: Click "Dark Theme"
    // Find Switch node
    core::Handle<core::Node> sw_node;
    for (size_t i = 0; i < tree.node_count(); ++i) {
        core::Handle<core::Node> h{static_cast<uint32_t>(i), 1};
        auto* n = tree.get(h);
        if (n && n->user_data) {
            auto* k = reinterpret_cast<widget::WidgetKind*>(n->user_data);
            if (*k == widget::WidgetKind::Switch) {
                sw_node = h;
                break;
            }
        }
    }
    ASSERT_TRUE(sw_node.is_valid());

    core::Point sw_pt{0.0f, 0.0f};
    for (auto cur = sw_node; cur.is_valid();) {
        auto* p = tree.get(cur);
        if (!p)
            break;
        sw_pt = sw_pt + p->layout.offset;
        cur = p->parent;
    }
    sw_pt = sw_pt + core::Point{5.0f, 5.0f};

    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Down,
                                                 .button = platform::PointerButton::Left,
                                                 .position = sw_pt});
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Up,
                                                 .button = platform::PointerButton::Left,
                                                 .position = sw_pt});

    // Sequence 2: Now click Mode B!
    core::Handle<core::Node> rb_node;
    auto find_rb = [&](auto& self, core::Handle<core::Node> h) -> void {
        auto* n = tree.get(h);
        if (!n)
            return;
        if (n->user_data) {
            auto* k = reinterpret_cast<widget::WidgetKind*>(n->user_data);
            if (*k == widget::WidgetKind::RadioButton) {
                auto* rb = reinterpret_cast<widget::RadioButtonData*>(n->user_data);
                if (rb->value == 1) {
                    rb_node = h;
                    return;
                }
            }
        }
        for (auto c : n->children) {
            self(self, c);
        }
    };
    find_rb(find_rb, tree.root());
    ASSERT_TRUE(rb_node.is_valid());

    core::Point rb_pt{0.0f, 0.0f};
    for (auto cur = rb_node; cur.is_valid();) {
        auto* p = tree.get(cur);
        if (!p)
            break;
        rb_pt = rb_pt + p->layout.offset;
        cur = p->parent;
    }
    rb_pt = rb_pt + core::Point{5.0f, 5.0f};

    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Down,
                                                 .button = platform::PointerButton::Left,
                                                 .position = rb_pt});
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Up,
                                                 .button = platform::PointerButton::Left,
                                                 .position = rb_pt});

    EXPECT_EQ(radio_choice.get(), 1);

    for (int i = 0; i < 30; ++i) {
        animation::AnimationManager::instance().tick(0.016f);
    }

    // Sequence 3: Click inc_btn ("+") -> triggers counter rebuild
    counter.set(counter.get() + 1);

    // Sequence 4: Click Mode A
    core::Handle<core::Node> rb_a_node;
    auto find_rba = [&](auto& self, core::Handle<core::Node> h) -> void {
        auto* n = tree.get(h);
        if (!n)
            return;
        if (n->user_data) {
            auto* k = reinterpret_cast<widget::WidgetKind*>(n->user_data);
            if (*k == widget::WidgetKind::RadioButton) {
                auto* rb = reinterpret_cast<widget::RadioButtonData*>(n->user_data);
                if (rb->value == 0) {
                    rb_a_node = h;
                    return;
                }
            }
        }
        for (auto c : n->children) {
            self(self, c);
        }
    };
    find_rba(find_rba, tree.root());
    ASSERT_TRUE(rb_a_node.is_valid());

    core::Point rba_pt{0.0f, 0.0f};
    for (auto cur = rb_a_node; cur.is_valid();) {
        auto* p = tree.get(cur);
        if (!p)
            break;
        rba_pt = rba_pt + p->layout.offset;
        cur = p->parent;
    }
    rba_pt = rba_pt + core::Point{5.0f, 5.0f};

    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Down,
                                                 .button = platform::PointerButton::Left,
                                                 .position = rba_pt});
    dispatcher.on_pointer(platform::PointerEvent{.action = platform::PointerAction::Up,
                                                 .button = platform::PointerButton::Left,
                                                 .position = rba_pt});

    EXPECT_EQ(radio_choice.get(), 0);

    for (int i = 0; i < 30; ++i) {
        animation::AnimationManager::instance().tick(0.016f);
    }
}
