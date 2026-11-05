#include "north/text/text_editor.hpp"
#include "north/platform/clipboard.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cctype>

namespace north::text {

namespace {

void append_utf8(std::string& out, char32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

bool is_word_char(char c) noexcept {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

uint64_t current_time_ms() noexcept {
    return GetTickCount64();
}

} // namespace

bool UndoAction::can_coalesce_with(const UndoAction& next) const noexcept {
    if (kind != next.kind) {
        return false;
    }
    if (next.timestamp_ms > timestamp_ms && (next.timestamp_ms - timestamp_ms) > 500) {
        return false;
    }

    if (kind == Kind::Insert) {
        if (next.start_pos != start_pos + inserted_text.size()) {
            return false;
        }
        if (inserted_text.find('\n') != std::string::npos || next.inserted_text.find('\n') != std::string::npos) {
            return false;
        }
        if (!inserted_text.empty() && next.inserted_text.size() == 1) {
            bool prev_ws = std::isspace(static_cast<unsigned char>(inserted_text.back()));
            bool next_ws = std::isspace(static_cast<unsigned char>(next.inserted_text[0]));
            return prev_ws == next_ws;
        }
    } else if (kind == Kind::Delete) {
        if (next.start_pos + next.deleted_text.size() == start_pos) {
            return true;
        }
    }
    return false;
}

TextEditor::TextEditor() {
    rebuild_line_starts();
}

TextEditor::TextEditor(std::string_view initial_text) : text_(initial_text) {
    cursor_pos_ = text_.size();
    selection_anchor_ = cursor_pos_;
    rebuild_line_starts();
    update_preferred_column();
}

void TextEditor::set_text(std::string_view text) {
    text_ = std::string(text);
    cursor_pos_ = text_.size();
    selection_anchor_ = cursor_pos_;
    rebuild_line_starts();
    update_preferred_column();
    clear_history();
}

void TextEditor::rebuild_line_starts() {
    line_starts_.clear();
    line_starts_.push_back(0);
    for (size_t i = 0; i < text_.size(); ++i) {
        if (text_[i] == '\n') {
            line_starts_.push_back(i + 1);
        }
    }
}

void TextEditor::update_preferred_column() {
    preferred_column_ = column_of(cursor_pos_);
}

std::pair<size_t, size_t> TextEditor::selection_range() const noexcept {
    return {(std::min)(cursor_pos_, selection_anchor_), (std::max)(cursor_pos_, selection_anchor_)};
}

std::string_view TextEditor::selected_text() const noexcept {
    if (!has_selection()) {
        return {};
    }
    auto [start, end] = selection_range();
    return std::string_view(text_.data() + start, end - start);
}

void TextEditor::select_all() noexcept {
    selection_anchor_ = 0;
    cursor_pos_ = text_.size();
    update_preferred_column();
}

void TextEditor::clear_selection() noexcept {
    selection_anchor_ = cursor_pos_;
}

size_t TextEditor::line_of(size_t byte_pos) const noexcept {
    if (byte_pos >= text_.size()) {
        return line_starts_.size() - 1;
    }
    auto it = std::upper_bound(line_starts_.begin(), line_starts_.end(), byte_pos);
    return static_cast<size_t>(std::distance(line_starts_.begin(), it) - 1);
}

size_t TextEditor::column_of(size_t byte_pos) const noexcept {
    size_t line = line_of(byte_pos);
    size_t start = line_starts_[line];
    size_t clamped_pos = (std::min)(byte_pos, text_.size());

    size_t col = 0;
    size_t p = start;
    while (p < clamped_pos) {
        p = next_codepoint(p);
        ++col;
    }
    return col;
}

TextPosition TextEditor::position_of(size_t byte_pos) const noexcept {
    return TextPosition{.line = line_of(byte_pos), .column = column_of(byte_pos)};
}

size_t TextEditor::byte_pos_from_line_col(size_t line, size_t col) const noexcept {
    if (line >= line_starts_.size()) {
        line = line_starts_.size() - 1;
    }
    size_t p = line_starts_[line];
    size_t end = line_end(line);

    for (size_t c = 0; c < col && p < end; ++c) {
        p = next_codepoint(p);
    }
    return p;
}

size_t TextEditor::line_start(size_t line) const noexcept {
    if (line >= line_starts_.size()) {
        return text_.size();
    }
    return line_starts_[line];
}

size_t TextEditor::line_end(size_t line) const noexcept {
    if (line >= line_starts_.size()) {
        return text_.size();
    }
    if (line + 1 < line_starts_.size()) {
        size_t next_start = line_starts_[line + 1];
        if (next_start >= 2 && text_[next_start - 2] == '\r') {
            return next_start - 2;
        }
        return next_start - 1;
    }
    return text_.size();
}

std::string_view TextEditor::line_text(size_t line) const noexcept {
    size_t s = line_start(line);
    size_t e = line_end(line);
    if (s >= text_.size() || s >= e) {
        return {};
    }
    return std::string_view(text_.data() + s, e - s);
}

size_t TextEditor::prev_codepoint(size_t pos) const noexcept {
    if (pos == 0) {
        return 0;
    }
    --pos;
    while (pos > 0 && (static_cast<uint8_t>(text_[pos]) & 0xC0) == 0x80) {
        --pos;
    }
    return pos;
}

size_t TextEditor::next_codepoint(size_t pos) const noexcept {
    if (pos >= text_.size()) {
        return text_.size();
    }
    ++pos;
    while (pos < text_.size() && (static_cast<uint8_t>(text_[pos]) & 0xC0) == 0x80) {
        ++pos;
    }
    return pos;
}

void TextEditor::record_action(UndoAction action) {
    if (!undo_stack_.empty() && undo_stack_.back().can_coalesce_with(action)) {
        if (action.kind == UndoAction::Kind::Insert) {
            undo_stack_.back().inserted_text += action.inserted_text;
            undo_stack_.back().timestamp_ms = action.timestamp_ms;
            return;
        }
        if (action.kind == UndoAction::Kind::Delete) {
            undo_stack_.back().deleted_text = action.deleted_text + undo_stack_.back().deleted_text;
            undo_stack_.back().start_pos = action.start_pos;
            undo_stack_.back().timestamp_ms = action.timestamp_ms;
            return;
        }
    }
    undo_stack_.push_back(std::move(action));
    redo_stack_.clear();
}

bool TextEditor::insert(char32_t codepoint) {
    if (codepoint == 0) {
        return false;
    }
    std::string encoded;
    append_utf8(encoded, codepoint);
    return insert(std::string_view(encoded));
}

bool TextEditor::insert(std::string_view str) {
    if (str.empty()) {
        return false;
    }

    UndoAction action;
    action.timestamp_ms = current_time_ms();

    if (has_selection()) {
        auto [start, end] = selection_range();
        action.kind = UndoAction::Kind::Replace;
        action.start_pos = start;
        action.deleted_text = text_.substr(start, end - start);
        action.inserted_text = std::string(str);

        text_.erase(start, end - start);
        text_.insert(start, str);
        cursor_pos_ = start + str.size();
        selection_anchor_ = cursor_pos_;
    } else {
        action.kind = UndoAction::Kind::Insert;
        action.start_pos = cursor_pos_;
        action.inserted_text = std::string(str);

        text_.insert(cursor_pos_, str);
        cursor_pos_ += str.size();
        selection_anchor_ = cursor_pos_;
    }

    record_action(std::move(action));
    rebuild_line_starts();
    update_preferred_column();
    return true;
}

bool TextEditor::insert_newline() {
    return insert("\n");
}

bool TextEditor::delete_selection() {
    if (!has_selection()) {
        return false;
    }
    auto [start, end] = selection_range();
    UndoAction action{
        .kind = UndoAction::Kind::Delete,
        .start_pos = start,
        .deleted_text = text_.substr(start, end - start),
        .timestamp_ms = current_time_ms()
    };

    text_.erase(start, end - start);
    cursor_pos_ = start;
    selection_anchor_ = cursor_pos_;

    record_action(std::move(action));
    rebuild_line_starts();
    update_preferred_column();
    return true;
}

bool TextEditor::delete_backward() {
    if (has_selection()) {
        return delete_selection();
    }
    if (cursor_pos_ == 0) {
        return false;
    }

    size_t prev = prev_codepoint(cursor_pos_);
    UndoAction action{
        .kind = UndoAction::Kind::Delete,
        .start_pos = prev,
        .deleted_text = text_.substr(prev, cursor_pos_ - prev),
        .timestamp_ms = current_time_ms()
    };

    text_.erase(prev, cursor_pos_ - prev);
    cursor_pos_ = prev;
    selection_anchor_ = cursor_pos_;

    record_action(std::move(action));
    rebuild_line_starts();
    update_preferred_column();
    return true;
}

bool TextEditor::delete_forward() {
    if (has_selection()) {
        return delete_selection();
    }
    if (cursor_pos_ >= text_.size()) {
        return false;
    }

    size_t next = next_codepoint(cursor_pos_);
    UndoAction action{
        .kind = UndoAction::Kind::Delete,
        .start_pos = cursor_pos_,
        .deleted_text = text_.substr(cursor_pos_, next - cursor_pos_),
        .timestamp_ms = current_time_ms()
    };

    text_.erase(cursor_pos_, next - cursor_pos_);
    selection_anchor_ = cursor_pos_;

    record_action(std::move(action));
    rebuild_line_starts();
    update_preferred_column();
    return true;
}

void TextEditor::move_cursor_left(bool select) {
    if (!select && has_selection()) {
        cursor_pos_ = selection_range().first;
        selection_anchor_ = cursor_pos_;
        update_preferred_column();
        return;
    }
    cursor_pos_ = prev_codepoint(cursor_pos_);
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
    update_preferred_column();
}

void TextEditor::move_cursor_right(bool select) {
    if (!select && has_selection()) {
        cursor_pos_ = selection_range().second;
        selection_anchor_ = cursor_pos_;
        update_preferred_column();
        return;
    }
    cursor_pos_ = next_codepoint(cursor_pos_);
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
    update_preferred_column();
}

void TextEditor::move_cursor_up(bool select) {
    size_t cur_line = line_of(cursor_pos_);
    if (cur_line > 0) {
        cursor_pos_ = byte_pos_from_line_col(cur_line - 1, preferred_column_);
    } else {
        cursor_pos_ = 0;
    }
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
}

void TextEditor::move_cursor_down(bool select) {
    size_t cur_line = line_of(cursor_pos_);
    if (cur_line + 1 < line_count()) {
        cursor_pos_ = byte_pos_from_line_col(cur_line + 1, preferred_column_);
    } else {
        cursor_pos_ = text_.size();
    }
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
}

void TextEditor::move_to_line_start(bool select) {
    size_t line = line_of(cursor_pos_);
    cursor_pos_ = line_start(line);
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
    update_preferred_column();
}

void TextEditor::move_to_line_end(bool select) {
    size_t line = line_of(cursor_pos_);
    cursor_pos_ = line_end(line);
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
    update_preferred_column();
}

void TextEditor::move_word_left(bool select) {
    size_t pos = cursor_pos_;
    while (pos > 0 && !is_word_char(text_[pos - 1])) {
        pos = prev_codepoint(pos);
    }
    while (pos > 0 && is_word_char(text_[pos - 1])) {
        pos = prev_codepoint(pos);
    }
    cursor_pos_ = pos;
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
    update_preferred_column();
}

void TextEditor::move_word_right(bool select) {
    size_t pos = cursor_pos_;
    if (pos < text_.size() && !is_word_char(text_[pos])) {
        while (pos < text_.size() && !is_word_char(text_[pos])) {
            pos = next_codepoint(pos);
        }
    }
    while (pos < text_.size() && is_word_char(text_[pos])) {
        pos = next_codepoint(pos);
    }
    cursor_pos_ = pos;
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
    update_preferred_column();
}

void TextEditor::move_to_document_start(bool select) {
    cursor_pos_ = 0;
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
    update_preferred_column();
}

void TextEditor::move_to_document_end(bool select) {
    cursor_pos_ = text_.size();
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
    update_preferred_column();
}

void TextEditor::set_cursor_pos(size_t byte_pos, bool select) {
    cursor_pos_ = (std::min)(byte_pos, text_.size());
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
    update_preferred_column();
}

void TextEditor::set_cursor_line_col(size_t line, size_t col, bool select) {
    cursor_pos_ = byte_pos_from_line_col(line, col);
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
    update_preferred_column();
}

bool TextEditor::undo() {
    if (undo_stack_.empty()) {
        return false;
    }
    UndoAction action = std::move(undo_stack_.back());
    undo_stack_.pop_back();

    if (action.kind == UndoAction::Kind::Insert) {
        text_.erase(action.start_pos, action.inserted_text.size());
        cursor_pos_ = action.start_pos;
        selection_anchor_ = cursor_pos_;
    } else if (action.kind == UndoAction::Kind::Delete) {
        text_.insert(action.start_pos, action.deleted_text);
        cursor_pos_ = action.start_pos + action.deleted_text.size();
        selection_anchor_ = cursor_pos_;
    } else if (action.kind == UndoAction::Kind::Replace) {
        text_.erase(action.start_pos, action.inserted_text.size());
        text_.insert(action.start_pos, action.deleted_text);
        cursor_pos_ = action.start_pos + action.deleted_text.size();
        selection_anchor_ = action.start_pos;
    }

    redo_stack_.push_back(std::move(action));
    rebuild_line_starts();
    update_preferred_column();
    return true;
}

bool TextEditor::redo() {
    if (redo_stack_.empty()) {
        return false;
    }
    UndoAction action = std::move(redo_stack_.back());
    redo_stack_.pop_back();

    if (action.kind == UndoAction::Kind::Insert) {
        text_.insert(action.start_pos, action.inserted_text);
        cursor_pos_ = action.start_pos + action.inserted_text.size();
        selection_anchor_ = cursor_pos_;
    } else if (action.kind == UndoAction::Kind::Delete) {
        text_.erase(action.start_pos, action.deleted_text.size());
        cursor_pos_ = action.start_pos;
        selection_anchor_ = cursor_pos_;
    } else if (action.kind == UndoAction::Kind::Replace) {
        text_.erase(action.start_pos, action.deleted_text.size());
        text_.insert(action.start_pos, action.inserted_text);
        cursor_pos_ = action.start_pos + action.inserted_text.size();
        selection_anchor_ = cursor_pos_;
    }

    undo_stack_.push_back(std::move(action));
    rebuild_line_starts();
    update_preferred_column();
    return true;
}

void TextEditor::clear_history() noexcept {
    undo_stack_.clear();
    redo_stack_.clear();
}

bool TextEditor::copy_to_clipboard() const {
    std::string_view sel = selected_text();
    if (sel.empty()) {
        return false;
    }
    return platform::Clipboard::set_text(sel);
}

bool TextEditor::cut_to_clipboard() {
    if (copy_to_clipboard()) {
        delete_selection();
        return true;
    }
    return false;
}

bool TextEditor::paste_from_clipboard() {
    std::string pasted = platform::Clipboard::get_text();
    if (pasted.empty()) {
        return false;
    }
    return insert(pasted);
}

} // namespace north::text
