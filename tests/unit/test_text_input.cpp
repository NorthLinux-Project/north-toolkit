#include "north/core/tree.hpp"
#include "north/event/input_dispatcher.hpp"
#include "north/gpu/d3d11/d3d11_device.hpp"
#include "north/render/renderer.hpp"
#include "north/widget/box.hpp"
#include "north/widget/text_input.hpp"

#include <gtest/gtest.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

using namespace north;

TEST(TextInputTest, BuildWidgetAndProperties) {
    core::NodeTree tree;
    auto input = widget::TextInputBuilder(tree)
                     .placeholder("Enter text...")
                     .width(200.0f)
                     .height(36.0f)
                     .font_size(14.0f)
                     .build();

    EXPECT_TRUE(input.is_valid());
    auto* node = tree.get(input);
    ASSERT_NE(node, nullptr);
    ASSERT_NE(node->user_data, nullptr);

    auto* data = reinterpret_cast<widget::TextInputData*>(node->user_data);
    EXPECT_EQ(data->kind, widget::WidgetKind::TextInput);
    EXPECT_EQ(data->placeholder, "Enter text...");
    EXPECT_EQ(data->width, 200.0f);
    EXPECT_EQ(data->height, 36.0f);
    EXPECT_FALSE(data->is_focused);

    tree.set_root(input);
    tree.layout(layout::BoxConstraints::loose({800.0f, 600.0f}));
    EXPECT_EQ(node->layout.size.width, 200.0f);
    EXPECT_EQ(node->layout.size.height, 36.0f);
}

TEST(TextInputTest, FocusManagementAndTyping) {
    core::NodeTree tree;

    struct ChangeEvent {
        std::string text;
        int count{0};
    } change_evt;

    struct SubmitEvent {
        std::string text;
        int count{0};
    } submit_evt;

    auto input = widget::TextInputBuilder(tree)
                     .text("Init")
                     .width(250.0f)
                     .height(40.0f)
                     .on_change(
                         [](std::string_view text, void* ud) {
                             auto* ev = static_cast<ChangeEvent*>(ud);
                             ev->text = std::string(text);
                             ev->count++;
                         },
                         &change_evt)
                     .on_submit(
                         [](std::string_view text, void* ud) {
                             auto* ev = static_cast<SubmitEvent*>(ud);
                             ev->text = std::string(text);
                             ev->count++;
                         },
                         &submit_evt)
                     .build();

    tree.set_root(input);
    tree.layout(layout::BoxConstraints::tight({800.0f, 600.0f}));

    event::InputDispatcher dispatcher(tree);
    EXPECT_FALSE(dispatcher.focused_node().is_valid());

    auto* data = reinterpret_cast<widget::TextInputData*>(tree.get(input)->user_data);
    EXPECT_FALSE(data->is_focused);

    // Focus widget
    dispatcher.set_focus(input);
    EXPECT_EQ(dispatcher.focused_node(), input);
    EXPECT_TRUE(data->is_focused);

    // Type 'A'
    platform::KeyEvent key_a{.key_code = 0, .character = U'A', .is_down = true, .modifiers = 0};
    dispatcher.on_key(key_a);

    EXPECT_EQ(data->buffer.text(), "InitA");
    EXPECT_EQ(change_evt.text, "InitA");
    EXPECT_EQ(change_evt.count, 1);

    // Backspace
    platform::KeyEvent key_back{
        .key_code = VK_BACK, .character = 0, .is_down = true, .modifiers = 0};
    dispatcher.on_key(key_back);

    EXPECT_EQ(data->buffer.text(), "Init");
    EXPECT_EQ(change_evt.text, "Init");
    EXPECT_EQ(change_evt.count, 2);

    // Return (Submit)
    platform::KeyEvent key_enter{
        .key_code = VK_RETURN, .character = 0, .is_down = true, .modifiers = 0};
    dispatcher.on_key(key_enter);

    EXPECT_EQ(submit_evt.text, "Init");
    EXPECT_EQ(submit_evt.count, 1);

    // Clear focus
    dispatcher.clear_focus();
    EXPECT_FALSE(dispatcher.focused_node().is_valid());
    EXPECT_FALSE(data->is_focused);
}

TEST(TextInputTest, TabFocusCycle) {
    core::NodeTree tree;

    auto input1 = widget::TextInputBuilder(tree).placeholder("Input 1").build();
    auto input2 = widget::TextInputBuilder(tree).placeholder("Input 2").build();

    auto root = widget::BoxBuilder(tree)
                    .flex(layout::FlexDirection::Column)
                    .child(input1)
                    .child(input2)
                    .build();

    tree.set_root(root);
    tree.layout(layout::BoxConstraints::tight({800.0f, 600.0f}));

    event::InputDispatcher dispatcher(tree);

    // Press Tab with no focused widget -> focuses first widget
    platform::KeyEvent tab_event{
        .key_code = VK_TAB, .character = 0, .is_down = true, .modifiers = 0};
    dispatcher.on_key(tab_event);
    EXPECT_EQ(dispatcher.focused_node(), input1);

    // Press Tab again -> cycles to input2
    dispatcher.on_key(tab_event);
    EXPECT_EQ(dispatcher.focused_node(), input2);

    // Press Tab again -> wraps back to input1
    dispatcher.on_key(tab_event);
    EXPECT_EQ(dispatcher.focused_node(), input1);

    // Press Shift+Tab -> cycles in reverse to input2
    platform::KeyEvent shift_tab_event{
        .key_code = VK_TAB, .character = 0, .is_down = true, .modifiers = platform::kModShift};
    dispatcher.on_key(shift_tab_event);
    EXPECT_EQ(dispatcher.focused_node(), input2);
}

TEST(TextInputTest, PointerClickFocusAndClear) {
    core::NodeTree tree;

    auto input = widget::TextInputBuilder(tree).width(200.0f).height(40.0f).text("Hello").build();

    auto root = widget::BoxBuilder(tree).width(600.0f).height(400.0f).child(input).build();

    tree.set_root(root);
    tree.layout(layout::BoxConstraints::tight({600.0f, 400.0f}));

    event::InputDispatcher dispatcher(tree);

    // Click inside TextInput (e.g. at 50, 20)
    platform::PointerEvent click_in{.action = platform::PointerAction::Down,
                                    .button = platform::PointerButton::Left,
                                    .position = core::Point{50.0f, 20.0f}};
    dispatcher.on_pointer(click_in);
    EXPECT_EQ(dispatcher.focused_node(), input);

    // Click outside TextInput (e.g. at 500, 300)
    platform::PointerEvent click_out{.action = platform::PointerAction::Down,
                                     .button = platform::PointerButton::Left,
                                     .position = core::Point{500.0f, 300.0f}};
    dispatcher.on_pointer(click_out);
    EXPECT_FALSE(dispatcher.focused_node().is_valid());
}

TEST(TextInputTest, ImeCompositionAndCommit) {
    core::NodeTree tree;

    std::string last_changed;
    auto input =
        widget::TextInputBuilder(tree)
            .width(200.0f)
            .height(40.0f)
            .on_change([](std::string_view text,
                          void* ud) { *static_cast<std::string*>(ud) = std::string(text); },
                       &last_changed)
            .build();

    tree.set_root(input);
    tree.layout(layout::BoxConstraints::tight({800.0f, 600.0f}));

    event::InputDispatcher dispatcher(tree);
    dispatcher.set_focus(input);

    auto* data = reinterpret_cast<widget::TextInputData*>(tree.get(input)->user_data);

    // IME composition pre-edit text
    dispatcher.on_ime_composition("nihao", 5);
    EXPECT_EQ(data->composition_text, "nihao");
    EXPECT_EQ(data->composition_cursor, 5);
    EXPECT_TRUE(data->buffer.empty());

    // IME commit Chinese characters
    dispatcher.on_ime_commit("\xE4\xBD\xA0\xE5\xA5\xBD");
    EXPECT_TRUE(data->composition_text.empty());
    EXPECT_EQ(data->buffer.text(), "\xE4\xBD\xA0\xE5\xA5\xBD");
    EXPECT_EQ(last_changed, "\xE4\xBD\xA0\xE5\xA5\xBD");
}

TEST(TextInputTest, LongTextScrollAndClipping) {
    core::NodeTree tree;
    auto input = widget::TextInputBuilder(tree)
                     .width(100.0f)
                     .height(32.0f)
                     .text("dddddddddddddddddddddddddddddddddddddddddd")
                     .build();

    tree.set_root(input);
    tree.layout(layout::BoxConstraints::loose({600.0f, 400.0f}));

    event::InputDispatcher dispatcher(tree);
    dispatcher.set_focus(input);

    auto* data = reinterpret_cast<widget::TextInputData*>(tree.get(input)->user_data);
    ASSERT_NE(data, nullptr);

    core::Arena arena(1024 * 64);
    render::DisplayList dl(arena);

    auto* node = tree.get(input);
    ASSERT_NE(node->paint_fn, nullptr);
    node->paint_fn(tree, input, {10.0f, 10.0f}, &dl, node->user_data);

    // Verify horizontal scrolling occurred to bring caret into view
    EXPECT_GT(data->scroll_offset, 0.0f);

    // Verify commands inside the input have their clip_rect constrained to the input bounds
    float content_w = node->layout.size.width - data->padding.horizontal();
    float content_h = node->layout.size.height - data->padding.vertical();

    for (const auto& cmd : dl.commands()) {
        if (cmd.type == render::CommandType::GlyphRun) {
            EXPECT_LE(cmd.clip_rect.x, 10.0f + data->padding.left + 0.1f);
            EXPECT_LE(cmd.clip_rect.width, content_w + 0.1f);
            EXPECT_LE(cmd.clip_rect.height, content_h + 0.1f);
        }
    }
}

TEST(TextInputTest, SelectAllAndSelectionHighlightColor) {
    core::NodeTree tree;
    const auto& dark_theme = style::Theme::dark();

    auto input = widget::TextInputBuilder(tree)
                     .theme(dark_theme)
                     .width(200.0f)
                     .height(36.0f)
                     .text("Hello World")
                     .build();

    tree.set_root(input);
    tree.layout(layout::BoxConstraints::loose({600.0f, 400.0f}));

    event::InputDispatcher dispatcher(tree);
    dispatcher.set_focus(input);

    auto* data = reinterpret_cast<widget::TextInputData*>(tree.get(input)->user_data);
    ASSERT_NE(data, nullptr);

    // Verify selection color has sufficient opacity and clear primary blue hue
    EXPECT_GE(data->selection_color.a, 0.45f);
    EXPECT_GT(data->selection_color.b, data->selection_color.r);

    // Send Ctrl+A
    platform::KeyEvent select_all_key{
        .key_code = 'A', .character = 0, .is_down = true, .modifiers = platform::kModControl};
    dispatcher.on_key(select_all_key);

    EXPECT_TRUE(data->buffer.has_selection());
    auto [start, end] = data->buffer.selection_range();
    EXPECT_EQ(start, 0u);
    EXPECT_EQ(end, 11u);
    EXPECT_EQ(data->buffer.selected_text(), "Hello World");

    // Paint into display list and verify selection quad was emitted
    core::Arena arena(1024 * 64);
    render::DisplayList dl(arena);
    auto* node = tree.get(input);
    ASSERT_NE(node->paint_fn, nullptr);
    node->paint_fn(tree, input, {0.0f, 0.0f}, &dl, node->user_data);

    bool found_selection_quad = false;
    for (const auto& cmd : dl.commands()) {
        if (cmd.type == render::CommandType::Quad) {
            if (cmd.quad.fill_color.r == data->selection_color.r &&
                cmd.quad.fill_color.g == data->selection_color.g &&
                cmd.quad.fill_color.b == data->selection_color.b) {
                found_selection_quad = true;
                EXPECT_GT(cmd.quad.bounds.width, 10.0f);
            }
        }
    }
    EXPECT_TRUE(found_selection_quad);
}

TEST(TextInputTest, PointerDragSelection) {
    core::NodeTree tree;
    auto input = widget::TextInputBuilder(tree)
                     .width(200.0f)
                     .height(36.0f)
                     .text("Sample Selection Text")
                     .build();

    tree.set_root(input);
    tree.layout(layout::BoxConstraints::tight({600.0f, 400.0f}));

    event::InputDispatcher dispatcher(tree);

    // Mouse Down at beginning of text (padding.left = 12, so around x = 16)
    platform::PointerEvent down_evt{.action = platform::PointerAction::Down,
                                    .button = platform::PointerButton::Left,
                                    .position = core::Point{16.0f, 18.0f}};
    dispatcher.on_pointer(down_evt);
    EXPECT_EQ(dispatcher.focused_node(), input);

    auto* data = reinterpret_cast<widget::TextInputData*>(tree.get(input)->user_data);
    ASSERT_NE(data, nullptr);

    // Mouse Move across text to x = 80
    platform::PointerEvent move_evt{.action = platform::PointerAction::Move,
                                    .button = platform::PointerButton::Left,
                                    .position = core::Point{80.0f, 18.0f}};
    dispatcher.on_pointer(move_evt);

    EXPECT_TRUE(data->buffer.has_selection());
    auto [start, end] = data->buffer.selection_range();
    EXPECT_LT(start, end);
    EXPECT_FALSE(data->buffer.selected_text().empty());

    // Mouse Up
    platform::PointerEvent up_evt{.action = platform::PointerAction::Up,
                                  .button = platform::PointerButton::Left,
                                  .position = core::Point{80.0f, 18.0f}};
    dispatcher.on_pointer(up_evt);
    EXPECT_TRUE(data->buffer.has_selection());
}

TEST(TextInputTest, TypingLongStringToScroll) {
    core::NodeTree tree;
    auto input = widget::TextInputBuilder(tree).width(200.0f).height(32.0f).text("").build();

    auto card = widget::BoxBuilder(tree)
                    .flex(layout::FlexDirection::Column, layout::MainAxisAlignment::Center,
                          layout::CrossAxisAlignment::Stretch, 18.0f)
                    .child(input)
                    .build();

    auto root = widget::BoxBuilder(tree)
                    .flex(layout::FlexDirection::Column, layout::MainAxisAlignment::Center,
                          layout::CrossAxisAlignment::Center)
                    .child(card)
                    .build();

    tree.set_root(root);
    tree.layout(layout::BoxConstraints::tight({640.0f, 480.0f}));

    event::InputDispatcher dispatcher(tree);
    dispatcher.set_focus(input);

    for (int i = 0; i < 42; ++i) {
        platform::KeyEvent ke{.key_code = 0, .character = U'd', .is_down = true, .modifiers = 0};
        dispatcher.on_key(ke);
    }

    auto* data = reinterpret_cast<widget::TextInputData*>(tree.get(input)->user_data);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->buffer.text().size(), 42);

    core::Arena arena(1024 * 64);
    render::DisplayList dl(arena);
    auto* node = tree.get(input);

    gpu::D3D11Device device;
    if (device.init_offscreen(640, 480)) {
        render::Renderer renderer(device);
        if (renderer.init()) {
            renderer.render_frame(tree, {640.0f, 480.0f}, core::Color{0, 0, 0, 1});
        }
    } else {
        node->paint_fn(tree, input, {220.0f, 200.0f}, &dl, node->user_data);
    }

    EXPECT_GT(data->scroll_offset, 0.0f);
}
