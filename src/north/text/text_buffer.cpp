#include "north/text/text_buffer.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <cctype>

#include <windows.h>

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

} // namespace

TextBuffer::TextBuffer(std::string_view initial_text)
    : text_(initial_text), cursor_pos_(text_.size()), selection_anchor_(cursor_pos_) {}

void TextBuffer::set_text(std::string_view text) {
    text_ = std::string(text);
    cursor_pos_ = text_.size();
    selection_anchor_ = cursor_pos_;
}

std::pair<size_t, size_t> TextBuffer::selection_range() const noexcept {
    return {(std::min)(cursor_pos_, selection_anchor_), (std::max)(cursor_pos_, selection_anchor_)};
}

std::string_view TextBuffer::selected_text() const noexcept {
    auto [start, end] = selection_range();
    if (start >= end || start >= text_.size()) {
        return {};
    }
    return std::string_view(text_.data() + start, end - start);
}

void TextBuffer::select_all() noexcept {
    selection_anchor_ = 0;
    cursor_pos_ = text_.size();
}

void TextBuffer::clear_selection() noexcept {
    selection_anchor_ = cursor_pos_;
}

size_t TextBuffer::prev_codepoint(size_t pos) const noexcept {
    if (pos == 0)
        return 0;
    --pos;
    while (pos > 0 && (static_cast<uint8_t>(text_[pos]) & 0xC0) == 0x80) {
        --pos;
    }
    return pos;
}

size_t TextBuffer::next_codepoint(size_t pos) const noexcept {
    if (pos >= text_.size())
        return text_.size();
    ++pos;
    while (pos < text_.size() && (static_cast<uint8_t>(text_[pos]) & 0xC0) == 0x80) {
        ++pos;
    }
    return pos;
}

bool TextBuffer::delete_selection() {
    if (!has_selection()) {
        return false;
    }
    auto [start, end] = selection_range();
    text_.erase(start, end - start);
    cursor_pos_ = start;
    selection_anchor_ = cursor_pos_;
    return true;
}

bool TextBuffer::insert(char32_t codepoint) {
    if (codepoint == 0)
        return false;
    delete_selection();

    std::string encoded;
    append_utf8(encoded, codepoint);
    text_.insert(cursor_pos_, encoded);
    cursor_pos_ += encoded.size();
    selection_anchor_ = cursor_pos_;
    return true;
}

bool TextBuffer::insert(std::string_view str) {
    if (str.empty())
        return false;
    delete_selection();

    text_.insert(cursor_pos_, str);
    cursor_pos_ += str.size();
    selection_anchor_ = cursor_pos_;
    return true;
}

bool TextBuffer::delete_backward() {
    if (has_selection()) {
        return delete_selection();
    }
    if (cursor_pos_ == 0) {
        return false;
    }
    size_t prev = prev_codepoint(cursor_pos_);
    text_.erase(prev, cursor_pos_ - prev);
    cursor_pos_ = prev;
    selection_anchor_ = cursor_pos_;
    return true;
}

bool TextBuffer::delete_forward() {
    if (has_selection()) {
        return delete_selection();
    }
    if (cursor_pos_ >= text_.size()) {
        return false;
    }
    size_t next = next_codepoint(cursor_pos_);
    text_.erase(cursor_pos_, next - cursor_pos_);
    selection_anchor_ = cursor_pos_;
    return true;
}

void TextBuffer::set_cursor_pos(size_t byte_pos, bool select) {
    cursor_pos_ = (std::min)(byte_pos, text_.size());
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
}

void TextBuffer::move_cursor_left(bool select) {
    if (!select && has_selection()) {
        cursor_pos_ = selection_range().first;
        selection_anchor_ = cursor_pos_;
        return;
    }
    cursor_pos_ = prev_codepoint(cursor_pos_);
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
}

void TextBuffer::move_cursor_right(bool select) {
    if (!select && has_selection()) {
        cursor_pos_ = selection_range().second;
        selection_anchor_ = cursor_pos_;
        return;
    }
    cursor_pos_ = next_codepoint(cursor_pos_);
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
}

void TextBuffer::move_cursor_to_start(bool select) {
    cursor_pos_ = 0;
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
}

void TextBuffer::move_cursor_to_end(bool select) {
    cursor_pos_ = text_.size();
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
}

void TextBuffer::move_word_left(bool select) {
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
}

void TextBuffer::move_word_right(bool select) {
    size_t pos = cursor_pos_;
    while (pos < text_.size() && is_word_char(text_[pos])) {
        pos = next_codepoint(pos);
    }
    while (pos < text_.size() && !is_word_char(text_[pos])) {
        pos = next_codepoint(pos);
    }
    cursor_pos_ = pos;
    if (!select) {
        selection_anchor_ = cursor_pos_;
    }
}

bool TextBuffer::copy_to_clipboard() const {
    std::string_view sel = selected_text();
    if (sel.empty())
        return false;

    int wlen =
        MultiByteToWideChar(CP_UTF8, 0, sel.data(), static_cast<int>(sel.size()), nullptr, 0);
    if (wlen <= 0)
        return false;

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (static_cast<size_t>(wlen) + 1) * sizeof(wchar_t));
    if (!hMem)
        return false;

    auto* dst = static_cast<wchar_t*>(GlobalLock(hMem));
    if (!dst) {
        GlobalFree(hMem);
        return false;
    }

    MultiByteToWideChar(CP_UTF8, 0, sel.data(), static_cast<int>(sel.size()), dst, wlen);
    dst[wlen] = L'\0';
    GlobalUnlock(hMem);

    if (!OpenClipboard(nullptr)) {
        GlobalFree(hMem);
        return false;
    }

    EmptyClipboard();
    if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
        GlobalFree(hMem);
    }
    CloseClipboard();
    return true;
}

bool TextBuffer::cut_to_clipboard() {
    if (copy_to_clipboard()) {
        delete_selection();
        return true;
    }
    return false;
}

bool TextBuffer::paste_from_clipboard() {
    if (!OpenClipboard(nullptr))
        return false;

    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData) {
        CloseClipboard();
        return false;
    }

    const auto* wstr = static_cast<const wchar_t*>(GlobalLock(hData));
    if (!wstr) {
        CloseClipboard();
        return false;
    }

    int u8_len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (u8_len > 1) {
        std::string u8(static_cast<size_t>(u8_len) - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, u8.data(), u8_len, nullptr, nullptr);
        insert(u8);
    }

    GlobalUnlock(hData);
    CloseClipboard();
    return true;
}

} // namespace north::text
