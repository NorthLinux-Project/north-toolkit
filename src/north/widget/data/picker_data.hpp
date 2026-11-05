#pragma once

#include "north/overlay/overlay_types.hpp"
#include "north/widget/data/core_data.hpp"

#include <vector>

namespace north::widget {

struct CommandPaletteData;

using PaletteActionFn = void (*)(uint32_t item_id, void* user_data);
using PaletteDismissFn = void (*)(void* user_data);
using CommandPaletteThemeFn = void (*)(const style::Theme& theme, CommandPaletteData& data);

struct PaletteItem {
    uint32_t id{0};
    std::string title;
    std::string category;
    std::string shortcut_hint;
    render::Path icon_path;
    PaletteActionFn on_action{nullptr};
    void* user_data{nullptr};
};

struct PaletteItemMatch {
    size_t original_index{0};
    const PaletteItem* item{nullptr};
    int32_t score{0};
    std::vector<uint32_t> matched_indices;
};

struct CommandPaletteData {
    WidgetKind kind{WidgetKind::CommandPalette};
    std::string query;
    std::string placeholder{"Type a command or search..."};
    char active_mode_prefix{'\0'};
    std::vector<PaletteItem> items;
    std::vector<PaletteItemMatch> filtered_matches;
    size_t selected_index{0};
    size_t hovered_index{SIZE_MAX};
    float row_height{36.0f};
    float scroll_y{0.0f};
    float max_scroll_y{0.0f};
    bool is_open{true};

    std::shared_ptr<text::Font> font;
    float font_size{13.0f};
    float width{600.0f};
    float max_height{420.0f};

    core::Color background_color{0.14f, 0.15f, 0.18f, 1.0f};
    core::Color input_bg{0.18f, 0.19f, 0.22f, 1.0f};
    core::Color border_color{0.24f, 0.26f, 0.30f, 1.0f};
    core::Color text_color{0.88f, 0.90f, 0.94f, 1.0f};
    core::Color secondary_text_color{0.60f, 0.63f, 0.70f, 1.0f};
    core::Color highlight_color{0.45f, 0.65f, 1.0f, 1.0f};
    core::Color hover_row_bg{1.0f, 1.0f, 1.0f, 0.05f};
    core::Color selected_row_bg{0.24f, 0.27f, 0.33f, 1.0f};
    core::Color category_header_color{0.50f, 0.54f, 0.62f, 1.0f};
    core::Color shortcut_badge_bg{0.20f, 0.22f, 0.26f, 1.0f};
    core::Color shortcut_badge_text{0.70f, 0.73f, 0.80f, 1.0f};

    PaletteActionFn on_action{nullptr};
    void* action_user_data{nullptr};
    PaletteDismissFn on_dismiss{nullptr};
    void* dismiss_user_data{nullptr};

    core::Signal<style::Theme>* theme_signal{nullptr};
    CommandPaletteThemeFn on_theme{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    overlay::OverlayManager* overlay_manager{nullptr};
    overlay::OverlayId active_overlay_id{overlay::kInvalidOverlayId};

    CommandPaletteData() = default;
    CommandPaletteData(CommandPaletteData&&) noexcept = default;
    CommandPaletteData& operator=(CommandPaletteData&&) noexcept = default;
    ~CommandPaletteData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    CommandPaletteData(const CommandPaletteData& other)
        : kind(other.kind), query(other.query), placeholder(other.placeholder),
          active_mode_prefix(other.active_mode_prefix), items(other.items),
          filtered_matches(other.filtered_matches), selected_index(other.selected_index),
          hovered_index(other.hovered_index), row_height(other.row_height),
          scroll_y(other.scroll_y), max_scroll_y(other.max_scroll_y), is_open(other.is_open),
          font(other.font), font_size(other.font_size), width(other.width),
          max_height(other.max_height), background_color(other.background_color),
          input_bg(other.input_bg), border_color(other.border_color),
          text_color(other.text_color), secondary_text_color(other.secondary_text_color),
          highlight_color(other.highlight_color), hover_row_bg(other.hover_row_bg),
          selected_row_bg(other.selected_row_bg),
          category_header_color(other.category_header_color),
          shortcut_badge_bg(other.shortcut_badge_bg),
          shortcut_badge_text(other.shortcut_badge_text), on_action(other.on_action),
          action_user_data(other.action_user_data), on_dismiss(other.on_dismiss),
          dismiss_user_data(other.dismiss_user_data), theme_signal(other.theme_signal),
          on_theme(other.on_theme), tree(other.tree), node_handle(other.node_handle),
          overlay_manager(other.overlay_manager), active_overlay_id(other.active_overlay_id) {}

    void refilter();
    void select_next();
    void select_prev();
    void select_index(size_t index);
    void execute_selected();
    void set_query(std::string_view new_query);
    void scroll_into_view(size_t idx, float viewport_h);
    void apply_theme(const style::Theme& theme);
};

struct SelectData;

using SelectChangeCallbackFn = void (*)(uint32_t selected_id, size_t selected_index, void* user_data);
using SelectThemeFn = void (*)(const style::Theme& theme, SelectData& data);

struct SelectItem {
    uint32_t id{0};
    std::string label;
    std::string secondary_label;
    render::Path icon_path;
    bool disabled{false};
    void* user_data{nullptr};
};

struct SelectData {
    WidgetKind kind{WidgetKind::Select};
    std::vector<SelectItem> items;
    uint32_t selected_id{0};
    size_t selected_index{0};
    size_t hovered_index{SIZE_MAX};
    bool is_open{false};
    bool is_focused{false};
    bool is_hovered{false};
    overlay::OverlayId active_overlay_id{overlay::kInvalidOverlayId};

    std::shared_ptr<text::Font> font;
    float font_size{13.0f};
    float width{180.0f};
    float height{32.0f};
    float dropdown_max_height{220.0f};
    float item_height{30.0f};
    float scroll_y{0.0f};
    float max_scroll_y{0.0f};

    core::Color background_color{0.16f, 0.17f, 0.20f, 1.0f};
    core::Color hover_bg{0.20f, 0.21f, 0.25f, 1.0f};
    core::Color border_color{0.26f, 0.28f, 0.32f, 1.0f};
    core::Color focus_border_color{0.45f, 0.65f, 1.0f, 1.0f};
    core::Color text_color{0.88f, 0.90f, 0.94f, 1.0f};
    core::Color secondary_text_color{0.60f, 0.63f, 0.70f, 1.0f};
    core::Color chevron_color{0.65f, 0.68f, 0.75f, 1.0f};
    core::Color dropdown_bg{0.14f, 0.15f, 0.18f, 1.0f};
    core::Color item_hover_bg{1.0f, 1.0f, 1.0f, 0.06f};
    core::Color item_selected_bg{0.24f, 0.27f, 0.33f, 1.0f};

    SelectChangeCallbackFn on_change{nullptr};
    void* change_user_data{nullptr};

    core::Signal<style::Theme>* theme_signal{nullptr};
    SelectThemeFn on_theme{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    core::Handle<core::Node> popup_node{};
    overlay::OverlayManager* overlay_manager{nullptr};

    SelectData() = default;
    SelectData(SelectData&&) noexcept = default;
    SelectData& operator=(SelectData&&) noexcept = default;
    ~SelectData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    SelectData(const SelectData& other)
        : kind(other.kind), items(other.items), selected_id(other.selected_id),
          selected_index(other.selected_index), hovered_index(other.hovered_index),
          is_open(other.is_open), is_focused(other.is_focused), is_hovered(other.is_hovered),
          active_overlay_id(other.active_overlay_id), font(other.font),
          font_size(other.font_size), width(other.width), height(other.height),
          dropdown_max_height(other.dropdown_max_height), item_height(other.item_height),
          scroll_y(other.scroll_y), max_scroll_y(other.max_scroll_y),
          background_color(other.background_color), hover_bg(other.hover_bg),
          border_color(other.border_color), focus_border_color(other.focus_border_color),
          text_color(other.text_color), secondary_text_color(other.secondary_text_color),
          chevron_color(other.chevron_color), dropdown_bg(other.dropdown_bg),
          item_hover_bg(other.item_hover_bg), item_selected_bg(other.item_selected_bg),
          on_change(other.on_change), change_user_data(other.change_user_data),
          theme_signal(other.theme_signal), on_theme(other.on_theme), tree(other.tree),
          node_handle(other.node_handle), popup_node(other.popup_node),
          overlay_manager(other.overlay_manager) {}

    void open_dropdown();
    void close_dropdown();
    void toggle_dropdown();
    void select_by_index(size_t index);
    void select_by_id(uint32_t id);
    void apply_theme(const style::Theme& theme);
};

struct ComboBoxData;

using ComboBoxSelectCallbackFn = void (*)(uint32_t selected_id, std::string_view text, void* user_data);
using ComboBoxTextCallbackFn = void (*)(std::string_view text, void* user_data);
using ComboBoxThemeFn = void (*)(const style::Theme& theme, ComboBoxData& data);

struct ComboBoxData {
    WidgetKind kind{WidgetKind::ComboBox};
    std::vector<SelectItem> items;
    std::vector<size_t> filtered_indices;
    std::string text;
    std::string placeholder{"Select or type..."};
    size_t cursor_pos{0};
    uint32_t selected_id{0};
    size_t hovered_index{SIZE_MAX};
    size_t dropdown_selected_index{0};
    bool is_open{false};
    bool is_focused{false};
    bool is_hovered{false};
    overlay::OverlayId active_overlay_id{overlay::kInvalidOverlayId};

    std::shared_ptr<text::Font> font;
    float font_size{13.0f};
    float width{200.0f};
    float height{32.0f};
    float dropdown_max_height{220.0f};
    float item_height{30.0f};
    float scroll_y{0.0f};
    float max_scroll_y{0.0f};

    core::Color background_color{0.16f, 0.17f, 0.20f, 1.0f};
    core::Color hover_bg{0.20f, 0.21f, 0.25f, 1.0f};
    core::Color border_color{0.26f, 0.28f, 0.32f, 1.0f};
    core::Color focus_border_color{0.45f, 0.65f, 1.0f, 1.0f};
    core::Color text_color{0.88f, 0.90f, 0.94f, 1.0f};
    core::Color secondary_text_color{0.60f, 0.63f, 0.70f, 1.0f};
    core::Color chevron_color{0.65f, 0.68f, 0.75f, 1.0f};
    core::Color dropdown_bg{0.14f, 0.15f, 0.18f, 1.0f};
    core::Color item_hover_bg{1.0f, 1.0f, 1.0f, 0.06f};
    core::Color item_selected_bg{0.24f, 0.27f, 0.33f, 1.0f};

    ComboBoxSelectCallbackFn on_select{nullptr};
    void* select_user_data{nullptr};
    ComboBoxTextCallbackFn on_text_change{nullptr};
    void* text_user_data{nullptr};

    core::Signal<style::Theme>* theme_signal{nullptr};
    ComboBoxThemeFn on_theme{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    core::Handle<core::Node> popup_node{};
    overlay::OverlayManager* overlay_manager{nullptr};

    ComboBoxData() = default;
    ComboBoxData(ComboBoxData&&) noexcept = default;
    ComboBoxData& operator=(ComboBoxData&&) noexcept = default;
    ~ComboBoxData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    ComboBoxData(const ComboBoxData& other)
        : kind(other.kind), items(other.items), filtered_indices(other.filtered_indices),
          text(other.text), placeholder(other.placeholder), cursor_pos(other.cursor_pos),
          selected_id(other.selected_id), hovered_index(other.hovered_index),
          dropdown_selected_index(other.dropdown_selected_index), is_open(other.is_open),
          is_focused(other.is_focused), is_hovered(other.is_hovered),
          active_overlay_id(other.active_overlay_id), font(other.font),
          font_size(other.font_size), width(other.width), height(other.height),
          dropdown_max_height(other.dropdown_max_height), item_height(other.item_height),
          scroll_y(other.scroll_y), max_scroll_y(other.max_scroll_y),
          background_color(other.background_color), hover_bg(other.hover_bg),
          border_color(other.border_color), focus_border_color(other.focus_border_color),
          text_color(other.text_color), secondary_text_color(other.secondary_text_color),
          chevron_color(other.chevron_color), dropdown_bg(other.dropdown_bg),
          item_hover_bg(other.item_hover_bg), item_selected_bg(other.item_selected_bg),
          on_select(other.on_select), select_user_data(other.select_user_data),
          on_text_change(other.on_text_change), text_user_data(other.text_user_data),
          theme_signal(other.theme_signal), on_theme(other.on_theme), tree(other.tree),
          node_handle(other.node_handle), popup_node(other.popup_node),
          overlay_manager(other.overlay_manager) {}

    void refilter();
    void open_dropdown();
    void close_dropdown();
    void toggle_dropdown();
    void select_by_index(size_t index);
    void set_text(std::string_view new_text);
    void apply_theme(const style::Theme& theme);
};

} // namespace north::widget
