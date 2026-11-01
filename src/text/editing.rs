//! Shared UTF-8 character-indexed text editing buffer for single-line and multiline widgets.

/// Editable text buffer tracking character-indexed cursor, selection anchor, and word boundaries.
#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct EditBuffer {
    text: String,
    cursor: usize,
    anchor: Option<usize>,
}

impl EditBuffer {
    /// Creates an empty edit buffer.
    pub fn new() -> Self {
        Self::default()
    }

    /// Creates an edit buffer initialized with `text` and cursor placed at the end.
    pub fn from_text<S: Into<String>>(text: S) -> Self {
        let text = text.into();
        let cursor = text.chars().count();
        Self {
            text,
            cursor,
            anchor: None,
        }
    }

    /// Creates an edit buffer from explicit text, cursor character index, and optional selection anchor.
    pub fn with_state(text: String, cursor: usize, anchor: Option<usize>) -> Self {
        let len = text.chars().count();
        Self {
            text,
            cursor: cursor.min(len),
            anchor: anchor.map(|a| a.min(len)),
        }
    }

    /// Returns a string slice of the current buffer contents.
    pub fn text(&self) -> &str {
        &self.text
    }

    /// Consumes the buffer and returns the owned `String`.
    pub fn into_text(self) -> String {
        self.text
    }

    /// Returns `true` if the buffer contains no characters.
    pub fn is_empty(&self) -> bool {
        self.text.is_empty()
    }

    /// Returns the total number of Unicode scalar values (`char`s) in the buffer.
    pub fn char_len(&self) -> usize {
        self.text.chars().count()
    }

    /// Returns the current cursor character index (`0..=char_len()`).
    pub fn cursor(&self) -> usize {
        self.cursor
    }

    /// Returns the selection anchor character index, if any.
    pub fn anchor(&self) -> Option<usize> {
        self.anchor
    }

    /// Returns `(anchor, cursor)` when a selection anchor is present (including zero-width drag anchors).
    pub fn selection_pair(&self) -> Option<(usize, usize)> {
        self.anchor.map(|a| (a, self.cursor))
    }

    /// Replaces the text contents, clamping the cursor and clearing any selection.
    pub fn set_text<S: Into<String>>(&mut self, text: S) {
        self.text = text.into();
        let len = self.char_len();
        self.cursor = self.cursor.min(len);
        self.anchor = None;
    }

    /// Moves the cursor to `pos` (clamped) and clears any active selection.
    pub fn set_cursor(&mut self, pos: usize) {
        self.cursor = pos.min(self.char_len());
        self.anchor = None;
    }

    /// Sets an explicit `(anchor, focus)` character selection and moves cursor to `focus`.
    pub fn set_selection(&mut self, anchor: usize, focus: usize) {
        let len = self.char_len();
        self.anchor = Some(anchor.min(len));
        self.cursor = focus.min(len);
    }

    /// Clears any active selection anchor while leaving the cursor unchanged.
    pub fn clear_selection(&mut self) {
        self.anchor = None;
    }

    /// Selects the entire buffer and places the cursor at the end.
    pub fn select_all(&mut self) {
        let len = self.char_len();
        self.anchor = Some(0);
        self.cursor = len;
    }

    /// Returns the normalized `(start, end)` character bounds if a non-empty selection exists.
    pub fn selection_range(&self) -> Option<(usize, usize)> {
        let anchor = self.anchor?;
        if anchor != self.cursor {
            let len = self.char_len();
            let start = anchor.min(self.cursor).min(len);
            let end = anchor.max(self.cursor).min(len);
            if start < end {
                return Some((start, end));
            }
        }
        None
    }

    /// Returns the selected substring if a non-empty selection is active.
    pub fn selected_text(&self) -> Option<String> {
        let (start, end) = self.selection_range()?;
        let b_start = self.char_to_byte(start);
        let b_end = self.char_to_byte(end);
        Some(self.text[b_start..b_end].to_string())
    }

    /// Converts a character index into a UTF-8 byte offset within `text()`.
    pub fn char_to_byte(&self, char_idx: usize) -> usize {
        self.text
            .char_indices()
            .nth(char_idx)
            .map(|(b, _)| b)
            .unwrap_or(self.text.len())
    }

    /// Converts a UTF-8 byte offset into a character index, snapping down if mid-codepoint.
    pub fn byte_to_char(&self, byte_idx: usize) -> usize {
        let clamped = self.floor_char_boundary(byte_idx);
        self.text[..clamped].chars().count()
    }

    fn floor_char_boundary(&self, byte_idx: usize) -> usize {
        let mut idx = byte_idx.min(self.text.len());
        while idx > 0 && !self.text.is_char_boundary(idx) {
            idx -= 1;
        }
        idx
    }

    fn ceil_char_boundary(&self, byte_idx: usize) -> usize {
        let mut idx = byte_idx.min(self.text.len());
        while idx < self.text.len() && !self.text.is_char_boundary(idx) {
            idx += 1;
        }
        idx
    }

    /// Deletes the active non-empty selection. Returns `true` if characters were removed.
    pub fn delete_selection(&mut self) -> bool {
        if let Some((start, end)) = self.selection_range() {
            let b_start = self.char_to_byte(start);
            let b_end = self.char_to_byte(end);
            self.text.replace_range(b_start..b_end, "");
            self.cursor = start;
            self.anchor = None;
            return true;
        }
        self.anchor = None;
        false
    }

    /// Inserts `inserted` at the cursor, replacing any active selection.
    pub fn insert(&mut self, inserted: &str) {
        self.delete_selection();
        if inserted.is_empty() {
            return;
        }
        let pos = self.cursor.min(self.char_len());
        let byte_pos = self.char_to_byte(pos);
        self.text.insert_str(byte_pos, inserted);
        self.cursor = pos + inserted.chars().count();
        self.anchor = None;
    }

    /// Deletes the active selection or the character immediately preceding the cursor.
    pub fn backspace(&mut self) -> bool {
        if self.delete_selection() {
            return true;
        }
        let len = self.char_len();
        let cur = self.cursor.min(len);
        if cur > 0 {
            let start_b = self.char_to_byte(cur - 1);
            let end_b = self.char_to_byte(cur);
            self.text.replace_range(start_b..end_b, "");
            self.cursor = cur - 1;
            return true;
        }
        false
    }

    /// Deletes the active selection or the character immediately following the cursor.
    pub fn delete_forward(&mut self) -> bool {
        if self.delete_selection() {
            return true;
        }
        let len = self.char_len();
        if self.cursor < len {
            let start_b = self.char_to_byte(self.cursor);
            let end_b = self.char_to_byte(self.cursor + 1);
            self.text.replace_range(start_b..end_b, "");
            return true;
        }
        false
    }

    /// Deletes the active selection or the word preceding the cursor.
    pub fn delete_word_backward(&mut self) -> bool {
        if self.delete_selection() {
            return true;
        }
        let cur = self.cursor.min(self.char_len());
        if cur == 0 {
            return false;
        }
        let target = self.word_left_index(cur);
        let start_b = self.char_to_byte(target);
        let end_b = self.char_to_byte(cur);
        self.text.replace_range(start_b..end_b, "");
        self.cursor = target;
        true
    }

    /// Deletes the active selection or the word following the cursor.
    pub fn delete_word_forward(&mut self) -> bool {
        if self.delete_selection() {
            return true;
        }
        let len = self.char_len();
        let cur = self.cursor.min(len);
        if cur >= len {
            return false;
        }
        let target = self.word_right_index(cur);
        let start_b = self.char_to_byte(cur);
        let end_b = self.char_to_byte(target);
        self.text.replace_range(start_b..end_b, "");
        true
    }

    /// Deletes UTF-8 bytes before and after the current cursor position (for Wayland IME `delete_surrounding_text`).
    pub fn delete_surrounding_bytes(&mut self, before_bytes: usize, after_bytes: usize) -> bool {
        if before_bytes == 0 && after_bytes == 0 {
            return false;
        }
        let cursor_byte = self.char_to_byte(self.cursor.min(self.char_len()));
        let start_b = self.floor_char_boundary(cursor_byte.saturating_sub(before_bytes));
        let end_b = self.ceil_char_boundary(cursor_byte.saturating_add(after_bytes));
        if start_b < end_b {
            let new_cursor = self.text[..start_b].chars().count();
            self.text.replace_range(start_b..end_b, "");
            self.cursor = new_cursor;
            self.anchor = None;
            true
        } else {
            false
        }
    }

    /// Moves the cursor to `target`, optionally extending or preserving the selection anchor.
    pub fn move_to(&mut self, target: usize, extend: bool) {
        let len = self.char_len();
        let clamped = target.min(len);
        if extend {
            let anchor = self.anchor.unwrap_or(self.cursor).min(len);
            self.cursor = clamped;
            self.anchor = Some(anchor);
        } else {
            self.cursor = clamped;
            self.anchor = None;
        }
    }

    /// Moves the cursor one character left, collapsing selection if `extend` is `false`.
    pub fn move_left(&mut self, extend: bool) {
        if !extend {
            if let Some((start, _)) = self.selection_range() {
                self.cursor = start;
                self.anchor = None;
                return;
            }
        }
        let next = self.cursor.min(self.char_len()).saturating_sub(1);
        self.move_to(next, extend);
    }

    /// Moves the cursor one character right, collapsing selection if `extend` is `false`.
    pub fn move_right(&mut self, extend: bool) {
        let len = self.char_len();
        if !extend {
            if let Some((_, end)) = self.selection_range() {
                self.cursor = end;
                self.anchor = None;
                return;
            }
        }
        let next = (self.cursor.min(len) + 1).min(len);
        self.move_to(next, extend);
    }

    /// Moves the cursor to the previous word boundary, collapsing selection if `extend` is `false`.
    pub fn move_word_left(&mut self, extend: bool) {
        if !extend {
            if let Some((start, _)) = self.selection_range() {
                self.cursor = start;
                self.anchor = None;
                return;
            }
        }
        let next = self.word_left_index(self.cursor);
        self.move_to(next, extend);
    }

    /// Moves the cursor to the next word boundary, collapsing selection if `extend` is `false`.
    pub fn move_word_right(&mut self, extend: bool) {
        if !extend {
            if let Some((_, end)) = self.selection_range() {
                self.cursor = end;
                self.anchor = None;
                return;
            }
        }
        let next = self.word_right_index(self.cursor);
        self.move_to(next, extend);
    }

    fn word_left_index(&self, from: usize) -> usize {
        let chars: Vec<char> = self.text.chars().collect();
        let mut idx = from.min(chars.len());
        while idx > 0 && chars[idx - 1].is_whitespace() {
            idx -= 1;
        }
        if idx == 0 {
            return 0;
        }
        let is_word = is_word_char(chars[idx - 1]);
        while idx > 0 && !chars[idx - 1].is_whitespace() && is_word_char(chars[idx - 1]) == is_word {
            idx -= 1;
        }
        idx
    }

    fn word_right_index(&self, from: usize) -> usize {
        let chars: Vec<char> = self.text.chars().collect();
        let len = chars.len();
        let mut idx = from.min(len);
        while idx < len && chars[idx].is_whitespace() {
            idx += 1;
        }
        if idx >= len {
            return len;
        }
        let is_word = is_word_char(chars[idx]);
        while idx < len && !chars[idx].is_whitespace() && is_word_char(chars[idx]) == is_word {
            idx += 1;
        }
        idx
    }

    /// Finds word boundaries `(start, end)` around `char_idx` for double-click word selection.
    pub fn find_word_boundaries(&self, char_idx: usize) -> (usize, usize) {
        let chars: Vec<char> = self.text.chars().collect();
        if chars.is_empty() {
            return (0, 0);
        }
        let idx = char_idx.min(chars.len().saturating_sub(1));
        let c = chars[idx];

        if c == '\n' {
            (idx, idx + 1)
        } else if is_word_char(c) {
            let mut start = idx;
            while start > 0 && is_word_char(chars[start - 1]) {
                start -= 1;
            }
            let mut end = idx;
            while end < chars.len() && is_word_char(chars[end]) {
                end += 1;
            }
            (start, end)
        } else if c.is_whitespace() {
            let mut start = idx;
            while start > 0 && chars[start - 1].is_whitespace() && chars[start - 1] != '\n' {
                start -= 1;
            }
            let mut end = idx;
            while end < chars.len() && chars[end].is_whitespace() && chars[end] != '\n' {
                end += 1;
            }
            (start, end)
        } else {
            let mut start = idx;
            while start > 0 && !is_word_char(chars[start - 1]) && !chars[start - 1].is_whitespace()
            {
                start -= 1;
            }
            let mut end = idx;
            while end < chars.len() && !is_word_char(chars[end]) && !chars[end].is_whitespace() {
                end += 1;
            }
            (start, end)
        }
    }
}

fn is_word_char(ch: char) -> bool {
    ch.is_alphanumeric() || ch == '_'
}

pub(crate) fn is_modifier_keysym(key: &str, keysym: u32) -> bool {
    matches!(keysym, 0xffe1..=0xffee | 0xfe01..=0xfe1f)
        || key.starts_with("Shift_")
        || key.starts_with("Control_")
        || key.starts_with("Alt_")
        || key.starts_with("Super_")
        || key.starts_with("Meta_")
        || key.starts_with("Hyper_")
        || key == "ISO_Level3_Shift"
        || key == "Caps_Lock"
        || key == "Num_Lock"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_edit_buffer_insert_delete_multibyte_and_emoji() {
        let mut buf = EditBuffer::new();
        assert!(buf.is_empty());
        assert_eq!(buf.cursor(), 0);
        assert!(!buf.backspace());
        assert!(!buf.delete_forward());

        buf.insert("héllo ");
        buf.insert("世界");
        buf.insert(" 🦀!");
        assert_eq!(buf.text(), "héllo 世界 🦀!");
        assert_eq!(buf.char_len(), 11);
        assert_eq!(buf.cursor(), 11);

        // Move left past '!' and U+1F980, backspace ' '
        buf.move_left(false);
        buf.move_left(false);
        assert_eq!(buf.cursor(), 9);
        assert!(buf.backspace());
        assert_eq!(buf.text(), "héllo 世界🦀!");
        assert_eq!(buf.cursor(), 8);

        // Delete forward removes U+1F980 (4-byte UTF-8 scalar) cleanly
        assert!(buf.delete_forward());
        assert_eq!(buf.text(), "héllo 世界!");
        assert_eq!(buf.cursor(), 8);

        // Select "世界" backwards with Shift+Left twice
        buf.move_left(true);
        buf.move_left(true);
        assert_eq!(buf.selection_range(), Some((6, 8)));
        assert_eq!(buf.selected_text().as_deref(), Some("世界"));

        // Replace selection via insert
        buf.insert("Rust");
        assert_eq!(buf.text(), "héllo Rust!");
        assert_eq!(buf.selection_range(), None);
        assert_eq!(buf.cursor(), 10);
    }

    #[test]
    fn test_edit_buffer_word_movement_and_boundaries() {
        let nl_buf = EditBuffer::from_text("hello  \n\n  world");
        assert_eq!(nl_buf.find_word_boundaries(5), (5, 7));
        assert_eq!(nl_buf.find_word_boundaries(7), (7, 8));
        assert_eq!(nl_buf.find_word_boundaries(9), (9, 11));

        let mut buf = EditBuffer::from_text("quick brown_fox 123!");
        assert_eq!(buf.find_word_boundaries(2), (0, 5));
        assert_eq!(buf.find_word_boundaries(5), (5, 6));
        assert_eq!(buf.find_word_boundaries(8), (6, 15));
        assert_eq!(buf.find_word_boundaries(17), (16, 19));
        assert_eq!(buf.find_word_boundaries(19), (19, 20));

        // Word-left traversal from end (index 20)
        buf.move_word_left(false);
        assert_eq!(buf.cursor(), 19); // before '!'
        buf.move_word_left(false);
        assert_eq!(buf.cursor(), 16); // before "123"
        buf.move_word_left(false);
        assert_eq!(buf.cursor(), 6); // before "brown_fox"
        buf.move_word_left(false);
        assert_eq!(buf.cursor(), 0); // before "quick"
        buf.move_word_left(false);
        assert_eq!(buf.cursor(), 0); // clamped at start

        // Word-right with selection extension
        buf.move_word_right(true);
        assert_eq!(buf.selection_range(), Some((0, 5)));
        assert_eq!(buf.selected_text().as_deref(), Some("quick"));

        buf.move_word_right(true);
        assert_eq!(buf.selection_range(), Some((0, 15)));
        assert_eq!(buf.selected_text().as_deref(), Some("quick brown_fox"));

        // Collapsing right without extend lands at selection end (15)
        buf.move_right(false);
        assert_eq!(buf.cursor(), 15);
        assert_eq!(buf.selection_range(), None);

        // Word deletion backward/forward
        assert!(buf.delete_word_backward());
        assert_eq!(buf.text(), "quick  123!");
        assert!(buf.delete_word_forward());
        assert_eq!(buf.text(), "quick !");
    }

    #[test]
    fn test_edit_buffer_delete_surrounding_bytes_utf8_safety() {
        // '日' '本' '語' are 3 bytes each in UTF-8 (9 bytes total)
        let mut buf = EditBuffer::from_text("日本語");
        buf.set_cursor(2); // between '本' and '語' (byte offset 6)

        // Exact 3 bytes before ('本') and 3 bytes after ('語')
        assert!(buf.delete_surrounding_bytes(3, 3));
        assert_eq!(buf.text(), "日");
        assert_eq!(buf.cursor(), 1);

        // Unaligned byte counts (e.g. 1 byte into 3-byte char) must snap safely without panicking
        let mut buf2 = EditBuffer::from_text("あいう");
        buf2.set_cursor(1); // after 'あ' (byte 3)
        assert!(buf2.delete_surrounding_bytes(1, 1));
        assert_eq!(buf2.text(), "う");
        assert_eq!(buf2.cursor(), 0);

        // Oversized byte counts clamp to string bounds
        assert!(buf2.delete_surrounding_bytes(100, 100));
        assert_eq!(buf2.text(), "");
        assert_eq!(buf2.cursor(), 0);
    }
}
