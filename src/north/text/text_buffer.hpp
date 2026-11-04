#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace north::text {

class TextBuffer {
public:
    TextBuffer() = default;
    explicit TextBuffer(std::string_view initial_text);

    [[nodiscard]] const std::string& text() const noexcept { return text_; }
    [[nodiscard]] size_t cursor_pos() const noexcept { return cursor_pos_; }
    [[nodiscard]] size_t selection_anchor() const noexcept { return selection_anchor_; }
    [[nodiscard]] bool empty() const noexcept { return text_.empty(); }
    [[nodiscard]] size_t size() const noexcept { return text_.size(); }

    void set_text(std::string_view text);

    [[nodiscard]] bool has_selection() const noexcept { return cursor_pos_ != selection_anchor_; }

    [[nodiscard]] std::pair<size_t, size_t> selection_range() const noexcept;
    [[nodiscard]] std::string_view selected_text() const noexcept;

    void select_all() noexcept;
    void clear_selection() noexcept;

    bool insert(char32_t codepoint);
    bool insert(std::string_view str);

    bool delete_backward();
    bool delete_forward();
    bool delete_selection();

    void move_cursor_left(bool select = false);
    void move_cursor_right(bool select = false);
    void move_cursor_to_start(bool select = false);
    void move_cursor_to_end(bool select = false);
    void move_word_left(bool select = false);
    void move_word_right(bool select = false);

    void set_cursor_pos(size_t byte_pos, bool select = false);

    bool copy_to_clipboard() const;
    bool cut_to_clipboard();
    bool paste_from_clipboard();

    [[nodiscard]] size_t prev_codepoint(size_t pos) const noexcept;
    [[nodiscard]] size_t next_codepoint(size_t pos) const noexcept;

private:
    std::string text_;
    size_t cursor_pos_{0};
    size_t selection_anchor_{0};
};

} // namespace north::text
