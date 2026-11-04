#include "north/text/text_buffer.hpp"

#include <gtest/gtest.h>

using namespace north::text;

TEST(TextBufferTest, DefaultAndInitialState) {
    TextBuffer buf1;
    EXPECT_TRUE(buf1.empty());
    EXPECT_EQ(buf1.size(), 0u);
    EXPECT_EQ(buf1.cursor_pos(), 0u);
    EXPECT_FALSE(buf1.has_selection());

    TextBuffer buf2("Hello North");
    EXPECT_FALSE(buf2.empty());
    EXPECT_EQ(buf2.size(), 11u);
    EXPECT_EQ(buf2.cursor_pos(), 11u);
    EXPECT_EQ(buf2.text(), "Hello North");
}

TEST(TextBufferTest, InsertAsciiAndUtf8) {
    TextBuffer buf;
    buf.insert("Hello");
    EXPECT_EQ(buf.text(), "Hello");
    EXPECT_EQ(buf.cursor_pos(), 5u);

    // Multibyte UTF-8 character (Greek Pi: 2 bytes 0xCF 0x80)
    buf.insert(U'\u03C0');
    EXPECT_EQ(buf.cursor_pos(), 7u);
    EXPECT_EQ(buf.size(), 7u);

    // Multibyte 4-byte UTF-8 emoji (U+1F600: 4 bytes)
    buf.insert(U'\U0001F600');
    EXPECT_EQ(buf.cursor_pos(), 11u);
    EXPECT_EQ(buf.size(), 11u);
}

TEST(TextBufferTest, CodepointNavigation) {
    // String with 1-byte, 2-byte, 3-byte, and 4-byte characters
    // 'A' (1 byte) + U+00E9 (2 bytes) + U+4E16 (3 bytes) + U+1F600 (4 bytes)
    TextBuffer buf;
    buf.insert('A');
    buf.insert(U'\u00E9');
    buf.insert(U'\u4E16');
    buf.insert(U'\U0001F600');

    EXPECT_EQ(buf.cursor_pos(), 10u);

    // Left movement jumps over entire 4-byte emoji to byte 6
    buf.move_cursor_left(false);
    EXPECT_EQ(buf.cursor_pos(), 6u);

    // Left movement jumps over 3-byte kanji to byte 3
    buf.move_cursor_left(false);
    EXPECT_EQ(buf.cursor_pos(), 3u);

    // Left movement jumps over 2-byte e-acute to byte 1
    buf.move_cursor_left(false);
    EXPECT_EQ(buf.cursor_pos(), 1u);

    // Left movement jumps over 1-byte 'A' to byte 0
    buf.move_cursor_left(false);
    EXPECT_EQ(buf.cursor_pos(), 0u);

    // Clamped at start
    buf.move_cursor_left(false);
    EXPECT_EQ(buf.cursor_pos(), 0u);

    // Right movement jumps over 'A' to byte 1
    buf.move_cursor_right(false);
    EXPECT_EQ(buf.cursor_pos(), 1u);

    // Right movement jumps to byte 3
    buf.move_cursor_right(false);
    EXPECT_EQ(buf.cursor_pos(), 3u);

    // Move to end
    buf.move_cursor_to_end(false);
    EXPECT_EQ(buf.cursor_pos(), 10u);

    // Move to start
    buf.move_cursor_to_start(false);
    EXPECT_EQ(buf.cursor_pos(), 0u);
}

TEST(TextBufferTest, DeleteOperations) {
    TextBuffer buf("\x41\xC3\xA9\xE4\xB8\x96\xF0\x9F\x98\x80");
    EXPECT_EQ(buf.size(), 10u);
    EXPECT_EQ(buf.cursor_pos(), 10u);

    // Delete backward removes 4-byte emoji
    EXPECT_TRUE(buf.delete_backward());
    EXPECT_EQ(buf.size(), 6u);
    EXPECT_EQ(buf.cursor_pos(), 6u);

    // Move to start and delete forward removes 'A'
    buf.move_cursor_to_start(false);
    EXPECT_TRUE(buf.delete_forward());
    EXPECT_EQ(buf.size(), 5u);
    EXPECT_EQ(buf.cursor_pos(), 0u);
}

TEST(TextBufferTest, SelectionAndReplacement) {
    TextBuffer buf("The quick brown fox");
    buf.set_cursor_pos(4, false); // start of "quick"
    buf.set_cursor_pos(9, true);  // select "quick"

    EXPECT_TRUE(buf.has_selection());
    EXPECT_EQ(buf.selected_text(), "quick");

    // Replace selection by typing "slow"
    buf.insert("slow");
    EXPECT_EQ(buf.text(), "The slow brown fox");
    EXPECT_FALSE(buf.has_selection());
    EXPECT_EQ(buf.cursor_pos(), 8u);

    // Select all
    buf.select_all();
    EXPECT_TRUE(buf.has_selection());
    EXPECT_EQ(buf.selected_text(), "The slow brown fox");

    // Clear selection
    buf.clear_selection();
    EXPECT_FALSE(buf.has_selection());
}

TEST(TextBufferTest, WordNavigation) {
    TextBuffer buf("alpha   beta_gamma delta");
    buf.move_cursor_to_start(false);

    buf.move_word_right(false);
    EXPECT_EQ(buf.cursor_pos(), 8u); // after "alpha   "

    buf.move_word_right(false);
    EXPECT_EQ(buf.cursor_pos(), 19u); // after "beta_gamma "

    buf.move_word_left(false);
    EXPECT_EQ(buf.cursor_pos(), 8u); // start of "beta_gamma"
}

TEST(TextBufferTest, ClipboardCopyAndPaste) {
    TextBuffer buf("Hello Clipboard World");
    buf.set_cursor_pos(6, false);
    buf.set_cursor_pos(15, true);
    EXPECT_EQ(buf.selected_text(), "Clipboard");

    EXPECT_TRUE(buf.copy_to_clipboard());

    TextBuffer paste_buf("Result: ");
    EXPECT_TRUE(paste_buf.paste_from_clipboard());
    EXPECT_EQ(paste_buf.text(), "Result: Clipboard");

    // Cut
    EXPECT_TRUE(buf.cut_to_clipboard());
    EXPECT_EQ(buf.text(), "Hello  World");
}
