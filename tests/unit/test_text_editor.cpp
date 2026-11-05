#include "north/core/tree.hpp"
#include "north/platform/clipboard.hpp"
#include "north/render/display_list.hpp"
#include "north/text/font_manager.hpp"
#include "north/text/text_editor.hpp"
#include "north/widget/text_area.hpp"

#include <gtest/gtest.h>

using namespace north;

TEST(TextEditorTest, LineMetricsAndPositioning) {
    text::TextEditor editor("First Line\nSecond\nThird Line Content");

    EXPECT_EQ(editor.line_count(), 3u);
    EXPECT_EQ(editor.line_text(0), "First Line");
    EXPECT_EQ(editor.line_text(1), "Second");
    EXPECT_EQ(editor.line_text(2), "Third Line Content");

    // Line offsets
    EXPECT_EQ(editor.line_start(0), 0u);
    EXPECT_EQ(editor.line_end(0), 10u);
    EXPECT_EQ(editor.line_start(1), 11u);
    EXPECT_EQ(editor.line_end(1), 17u);
    EXPECT_EQ(editor.line_start(2), 18u);
    EXPECT_EQ(editor.line_end(2), 36u);

    // Byte to (line, col) mapping
    EXPECT_EQ(editor.line_of(0), 0u);
    EXPECT_EQ(editor.column_of(0), 0u);
    EXPECT_EQ(editor.line_of(5), 0u);
    EXPECT_EQ(editor.column_of(5), 5u);

    EXPECT_EQ(editor.line_of(11), 1u);
    EXPECT_EQ(editor.column_of(11), 0u);
    EXPECT_EQ(editor.line_of(14), 1u);
    EXPECT_EQ(editor.column_of(14), 3u);

    // Inverse mapping
    EXPECT_EQ(editor.byte_pos_from_line_col(0, 5), 5u);
    EXPECT_EQ(editor.byte_pos_from_line_col(1, 3), 14u);
}

TEST(TextEditorTest, VerticalCursorNavigationWithPreferredColumn) {
    // Line 0: "12345678" (len 8)
    // Line 1: "ab"       (len 2)
    // Line 2: "abcdefghij" (len 10)
    text::TextEditor editor("12345678\nab\nabcdefghij");

    // Position cursor at line 0, column 6
    editor.set_cursor_line_col(0, 6);
    EXPECT_EQ(editor.cursor_pos(), 6u);
    EXPECT_EQ(editor.preferred_column(), 6u);

    // Move Down to short line 1: should clamp to column 2 (byte pos 9 + 2 = 11)
    editor.move_cursor_down();
    EXPECT_EQ(editor.line_of(editor.cursor_pos()), 1u);
    EXPECT_EQ(editor.column_of(editor.cursor_pos()), 2u);
    EXPECT_EQ(editor.preferred_column(), 6u); // Preferred column preserved!

    // Move Down to longer line 2: should restore to column 6! (line 2 start is 12 -> 12 + 6 = 18)
    editor.move_cursor_down();
    EXPECT_EQ(editor.line_of(editor.cursor_pos()), 2u);
    EXPECT_EQ(editor.column_of(editor.cursor_pos()), 6u);
    EXPECT_EQ(editor.cursor_pos(), 18u);

    // Move Up back to short line: clamped to 2
    editor.move_cursor_up();
    EXPECT_EQ(editor.line_of(editor.cursor_pos()), 1u);
    EXPECT_EQ(editor.column_of(editor.cursor_pos()), 2u);

    // Move Up back to line 0: restored to 6
    editor.move_cursor_up();
    EXPECT_EQ(editor.line_of(editor.cursor_pos()), 0u);
    EXPECT_EQ(editor.column_of(editor.cursor_pos()), 6u);
}

TEST(TextEditorTest, HorizontalAndWordNavigation) {
    text::TextEditor editor("hello world_test\nsecond");

    editor.set_cursor_pos(0);
    editor.move_word_right();
    EXPECT_EQ(editor.cursor_pos(), 5u); // End of "hello"

    editor.move_word_right();
    EXPECT_EQ(editor.cursor_pos(), 16u); // End of "world_test"

    editor.move_word_left();
    EXPECT_EQ(editor.cursor_pos(), 6u); // Start of "world_test"

    editor.move_to_line_end();
    EXPECT_EQ(editor.cursor_pos(), 16u);

    editor.move_to_line_start();
    EXPECT_EQ(editor.cursor_pos(), 0u);

    editor.move_to_document_end();
    EXPECT_EQ(editor.cursor_pos(), editor.size());

    editor.move_to_document_start();
    EXPECT_EQ(editor.cursor_pos(), 0u);
}

TEST(TextEditorTest, MultiLineSelection) {
    text::TextEditor editor("Line 1\nLine 2\nLine 3");

    // Select across line boundary
    editor.set_cursor_pos(2); // 'n' in "Line 1"
    editor.set_cursor_pos(10, true); // Selection to 'e' in "Line 2"

    EXPECT_TRUE(editor.has_selection());
    EXPECT_EQ(editor.selection_range().first, 2u);
    EXPECT_EQ(editor.selection_range().second, 10u);
    EXPECT_EQ(editor.selected_text(), "ne 1\nLin");

    // Delete selection
    EXPECT_TRUE(editor.delete_selection());
    EXPECT_FALSE(editor.has_selection());
    EXPECT_EQ(editor.text(), "Lie 2\nLine 3");
    EXPECT_EQ(editor.cursor_pos(), 2u);

    // Select all
    editor.select_all();
    EXPECT_TRUE(editor.has_selection());
    EXPECT_EQ(editor.selected_text(), "Lie 2\nLine 3");
    editor.clear_selection();
    EXPECT_FALSE(editor.has_selection());
}

TEST(TextEditorTest, UndoRedoAndTypingCoalescing) {
    text::TextEditor editor;

    // Type "Hello"
    editor.insert("H");
    editor.insert("e");
    editor.insert("l");
    editor.insert("l");
    editor.insert("o");
    EXPECT_EQ(editor.text(), "Hello");
    EXPECT_TRUE(editor.can_undo());

    // Single undo should revert the coalesced word
    EXPECT_TRUE(editor.undo());
    EXPECT_EQ(editor.text(), "");
    EXPECT_TRUE(editor.can_redo());

    // Redo restores "Hello"
    EXPECT_TRUE(editor.redo());
    EXPECT_EQ(editor.text(), "Hello");

    // Add newline and text
    editor.insert_newline();
    editor.insert("World");
    EXPECT_EQ(editor.text(), "Hello\nWorld");

    // Undo reverts "World"
    EXPECT_TRUE(editor.undo());
    EXPECT_EQ(editor.text(), "Hello\n");

    // Another undo reverts newline
    EXPECT_TRUE(editor.undo());
    EXPECT_EQ(editor.text(), "Hello");
}

TEST(TextEditorTest, NewEditClearsRedoStack) {
    text::TextEditor editor("Alpha");
    editor.insert(" Beta");
    EXPECT_EQ(editor.text(), "Alpha Beta");

    editor.undo();
    EXPECT_EQ(editor.text(), "Alpha");
    EXPECT_TRUE(editor.can_redo());

    // Typing a new character clears redo
    editor.insert(" Gamma");
    EXPECT_EQ(editor.text(), "Alpha Gamma");
    EXPECT_FALSE(editor.can_redo());
}

TEST(ClipboardTest, RoundtripTextCopyAndPaste) {
    const std::string original = "North Toolkit Clipboard Verification 12345";
    EXPECT_TRUE(platform::Clipboard::set_text(original));
    EXPECT_TRUE(platform::Clipboard::has_text());

    std::string retrieved = platform::Clipboard::get_text();
    EXPECT_EQ(retrieved, original);
}

TEST(TextAreaWidgetTest, LayoutAndGutterCalculation) {
    core::NodeTree tree;
    auto node = widget::TextAreaBuilder(tree)
                    .text("Line 1\nLine 2\nLine 3\nLine 4\nLine 5")
                    .line_numbers(true)
                    .width(400.0f)
                    .height(200.0f)
                    .build();

    tree.set_root(node);
    tree.layout(layout::BoxConstraints::tight({400.0f, 200.0f}));

    auto* n = tree.get(node);
    ASSERT_NE(n, nullptr);
    EXPECT_FLOAT_EQ(n->layout.size.width, 400.0f);
    EXPECT_FLOAT_EQ(n->layout.size.height, 200.0f);

    auto* data = reinterpret_cast<widget::TextAreaData*>(n->user_data);
    ASSERT_NE(data, nullptr);
    EXPECT_TRUE(data->show_line_numbers);
    EXPECT_GT(data->line_numbers_width, 0.0f);
    EXPECT_EQ(data->editor.line_count(), 5u);
}

TEST(TextAreaWidgetTest, PaintsDisplayListCommands) {
    core::NodeTree tree;
    auto node = widget::TextAreaBuilder(tree)
                    .text("Header\nSecond line")
                    .line_numbers(true)
                    .width(300.0f)
                    .height(150.0f)
                    .build();

    tree.set_root(node);
    tree.layout(layout::BoxConstraints::tight({300.0f, 150.0f}));

    core::Arena arena(64 * 1024);
    render::DisplayList dl(arena);

    auto* n = tree.get(node);
    ASSERT_NE(n, nullptr);
    ASSERT_NE(n->paint_fn, nullptr);

    n->paint_fn(tree, node, core::Point{0.0f, 0.0f}, &dl, n->user_data);

    // DisplayList should have recorded background quads, gutter, text clips, and caret
    EXPECT_GT(dl.size(), 0u);
}

TEST(TextAreaWidgetTest, ThemeSignalUpdatesColors) {
    core::NodeTree tree;
    core::Signal<style::Theme> theme_sig(style::Theme::dark());

    auto node = widget::TextAreaBuilder(tree)
                    .text("Theme test")
                    .theme_signal(&theme_sig)
                    .build();

    auto* n = tree.get(node);
    ASSERT_NE(n, nullptr);
    auto* data = reinterpret_cast<widget::TextAreaData*>(n->user_data);
    ASSERT_NE(data, nullptr);

    core::Color dark_text_color = data->text_color;

    // Switch to light theme
    theme_sig.set(style::Theme::light());
    EXPECT_NE(data->text_color.r, dark_text_color.r);
}
