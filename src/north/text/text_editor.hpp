#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace north::text {

struct TextPosition {
    size_t line{0};
    size_t column{0};

    auto operator<=>(const TextPosition&) const = default;
};

struct UndoAction {
    enum class Kind {
        Insert,
        Delete,
        Replace
    };

    Kind kind{Kind::Insert};
    size_t start_pos{0};
    std::string deleted_text;
    std::string inserted_text;
    uint64_t timestamp_ms{0};

    [[nodiscard]] bool can_coalesce_with(const UndoAction& next) const noexcept;
};

class TextEditor {
public:
    TextEditor();
    explicit TextEditor(std::string_view initial_text);

    [[nodiscard]] const std::string& text() const noexcept { return text_; }
    [[nodiscard]] size_t cursor_pos() const noexcept { return cursor_pos_; }
    [[nodiscard]] size_t selection_anchor() const noexcept { return selection_anchor_; }
    [[nodiscard]] size_t preferred_column() const noexcept { return preferred_column_; }
    [[nodiscard]] bool empty() const noexcept { return text_.empty(); }
    [[nodiscard]] size_t size() const noexcept { return text_.size(); }
    [[nodiscard]] size_t line_count() const noexcept { return line_starts_.size(); }

    void set_text(std::string_view text);

    [[nodiscard]] bool has_selection() const noexcept { return cursor_pos_ != selection_anchor_; }
    [[nodiscard]] std::pair<size_t, size_t> selection_range() const noexcept;
    [[nodiscard]] std::string_view selected_text() const noexcept;

    void select_all() noexcept;
    void clear_selection() noexcept;

    [[nodiscard]] size_t line_of(size_t byte_pos) const noexcept;
    [[nodiscard]] size_t column_of(size_t byte_pos) const noexcept;
    [[nodiscard]] TextPosition position_of(size_t byte_pos) const noexcept;
    [[nodiscard]] size_t byte_pos_from_line_col(size_t line, size_t col) const noexcept;
    [[nodiscard]] size_t line_start(size_t line) const noexcept;
    [[nodiscard]] size_t line_end(size_t line) const noexcept;
    [[nodiscard]] std::string_view line_text(size_t line) const noexcept;

    bool insert(char32_t codepoint);
    bool insert(std::string_view str);
    bool insert_newline();

    bool delete_backward();
    bool delete_forward();
    bool delete_selection();

    void move_cursor_left(bool select = false);
    void move_cursor_right(bool select = false);
    void move_cursor_up(bool select = false);
    void move_cursor_down(bool select = false);
    void move_to_line_start(bool select = false);
    void move_to_line_end(bool select = false);
    void move_word_left(bool select = false);
    void move_word_right(bool select = false);
    void move_to_document_start(bool select = false);
    void move_to_document_end(bool select = false);

    void set_cursor_pos(size_t byte_pos, bool select = false);
    void set_cursor_line_col(size_t line, size_t col, bool select = false);

    [[nodiscard]] bool can_undo() const noexcept { return !undo_stack_.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !redo_stack_.empty(); }
    bool undo();
    bool redo();
    void clear_history() noexcept;

    bool copy_to_clipboard() const;
    bool cut_to_clipboard();
    bool paste_from_clipboard();

    [[nodiscard]] size_t prev_codepoint(size_t pos) const noexcept;
    [[nodiscard]] size_t next_codepoint(size_t pos) const noexcept;

private:
    void rebuild_line_starts();
    void record_action(UndoAction action);
    void update_preferred_column();

    std::string text_;
    std::vector<size_t> line_starts_{0};
    size_t cursor_pos_{0};
    size_t selection_anchor_{0};
    size_t preferred_column_{0};

    std::vector<UndoAction> undo_stack_;
    std::vector<UndoAction> redo_stack_;
};

} // namespace north::text
