#pragma once

#include "north/widget/data/core_data.hpp"

#include <vector>

namespace north::widget {

enum class SplitOrientation : uint8_t { Horizontal, Vertical };
enum class SplitSizingMode : uint8_t { Ratio, FixedFirst, FixedSecond };

using SplitChangeCallbackFn = void (*)(float split_value, void* user_data);
struct SplitViewData;
using SplitViewThemeFn = void (*)(const style::Theme& theme, SplitViewData& data);

struct SplitViewData {
    WidgetKind kind{WidgetKind::SplitView};
    SplitOrientation orientation{SplitOrientation::Horizontal};
    SplitSizingMode sizing_mode{SplitSizingMode::Ratio};

    float split_value{0.5f};
    float initial_split_value{0.5f};
    float min_first_size{48.0f};
    float max_first_size{100000.0f};
    float min_second_size{48.0f};
    float max_second_size{100000.0f};
    float sash_thickness{6.0f};

    float width{0.0f};
    float height{0.0f};

    core::Handle<core::Node> first_pane{};
    core::Handle<core::Node> second_pane{};
    core::Rect sash_rect{};

    bool sash_hovered{false};
    bool sash_dragging{false};
    float drag_start_pointer_pos{0.0f};
    float drag_start_split_value{0.5f};
    uint64_t last_sash_click_ms{0};

    core::Color sash_bg{0.15f, 0.16f, 0.20f, 1.0f};
    core::Color sash_hover_bg{0.24f, 0.26f, 0.32f, 1.0f};
    core::Color sash_drag_bg{0.30f, 0.35f, 0.45f, 1.0f};
    core::Color sash_grip_color{0.68f, 0.71f, 0.78f, 1.0f};
    core::Color border_color{0.24f, 0.26f, 0.30f, 1.0f};

    SplitChangeCallbackFn on_split_change{nullptr};
    void* split_user_data{nullptr};

    core::Signal<style::Theme>* theme_signal{nullptr};
    SplitViewThemeFn on_theme{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};

    SplitViewData() = default;
    SplitViewData(SplitViewData&&) noexcept = default;
    SplitViewData& operator=(SplitViewData&&) noexcept = default;
    ~SplitViewData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    SplitViewData(const SplitViewData& other)
        : kind(other.kind), orientation(other.orientation), sizing_mode(other.sizing_mode),
          split_value(other.split_value), initial_split_value(other.initial_split_value),
          min_first_size(other.min_first_size), max_first_size(other.max_first_size),
          min_second_size(other.min_second_size), max_second_size(other.max_second_size),
          sash_thickness(other.sash_thickness), width(other.width), height(other.height),
          first_pane(other.first_pane), second_pane(other.second_pane), sash_rect(other.sash_rect),
          sash_hovered(other.sash_hovered), sash_dragging(other.sash_dragging),
          drag_start_pointer_pos(other.drag_start_pointer_pos),
          drag_start_split_value(other.drag_start_split_value),
          last_sash_click_ms(other.last_sash_click_ms), sash_bg(other.sash_bg),
          sash_hover_bg(other.sash_hover_bg), sash_drag_bg(other.sash_drag_bg),
          sash_grip_color(other.sash_grip_color), border_color(other.border_color),
          on_split_change(other.on_split_change), split_user_data(other.split_user_data),
          theme_signal(other.theme_signal), on_theme(other.on_theme), tree(other.tree),
          node_handle(other.node_handle) {}

    void set_split(float val);
    void reset_split();
    void apply_theme(const style::Theme& theme);
};

struct TreeNodeItem {
    uint32_t id{0};
    std::string label;
    render::Path icon_path;
    bool expandable{false};
    bool expanded{false};
    void* user_data{nullptr};
    std::vector<TreeNodeItem> children;
};

struct VisibleTreeRow {
    TreeNodeItem* item{nullptr};
    uint32_t parent_id{0};
    uint16_t depth{0};
};

using TreeSelectCallbackFn = void (*)(uint32_t id, void* user_data);
using TreeToggleCallbackFn = void (*)(uint32_t id, bool expanded, void* user_data);
using TreeActivateCallbackFn = void (*)(uint32_t id, void* user_data);
struct TreeViewData;
using TreeViewThemeFn = void (*)(const style::Theme& theme, TreeViewData& data);

struct TreeViewData {
    WidgetKind kind{WidgetKind::TreeView};
    std::vector<TreeNodeItem> roots;
    std::vector<VisibleTreeRow> visible_rows;

    uint32_t selected_id{0};
    size_t hovered_row{SIZE_MAX};
    bool is_focused{false};

    std::shared_ptr<text::Font> font;
    float font_size{13.0f};
    float row_height{28.0f};
    float indent_width{18.0f};
    float scroll_y{0.0f};
    float max_scroll_y{0.0f};
    bool show_indent_guides{true};

    float width{0.0f};
    float height{0.0f};

    core::Color background_color{0.13f, 0.14f, 0.17f, 1.0f};
    core::Color text_color{0.92f, 0.94f, 0.96f, 1.0f};
    core::Color hover_row_bg{0.19f, 0.21f, 0.25f, 1.0f};
    core::Color selected_row_bg{0.26f, 0.35f, 0.48f, 1.0f};
    core::Color indent_guide_color{0.24f, 0.26f, 0.30f, 0.60f};
    core::Color chevron_color{0.68f, 0.71f, 0.78f, 1.0f};
    core::Color icon_color{0.78f, 0.81f, 0.88f, 1.0f};
    core::Color border_color{0.24f, 0.26f, 0.30f, 1.0f};
    core::Color scrollbar_thumb_color{0.40f, 0.43f, 0.50f, 0.80f};

    TreeSelectCallbackFn on_select{nullptr};
    void* select_user_data{nullptr};
    TreeToggleCallbackFn on_toggle{nullptr};
    void* toggle_user_data{nullptr};
    TreeActivateCallbackFn on_activate{nullptr};
    void* activate_user_data{nullptr};

    core::Signal<style::Theme>* theme_signal{nullptr};
    TreeViewThemeFn on_theme{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};

    TreeViewData() = default;
    TreeViewData(TreeViewData&&) noexcept = default;
    TreeViewData& operator=(TreeViewData&&) noexcept = default;
    ~TreeViewData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    TreeViewData(const TreeViewData& other)
        : kind(other.kind), roots(other.roots), visible_rows(other.visible_rows),
          selected_id(other.selected_id), hovered_row(other.hovered_row), is_focused(other.is_focused),
          font(other.font), font_size(other.font_size), row_height(other.row_height),
          indent_width(other.indent_width), scroll_y(other.scroll_y),
          max_scroll_y(other.max_scroll_y), show_indent_guides(other.show_indent_guides),
          width(other.width), height(other.height), background_color(other.background_color),
          text_color(other.text_color), hover_row_bg(other.hover_row_bg),
          selected_row_bg(other.selected_row_bg), indent_guide_color(other.indent_guide_color),
          chevron_color(other.chevron_color), icon_color(other.icon_color),
          border_color(other.border_color), scrollbar_thumb_color(other.scrollbar_thumb_color),
          on_select(other.on_select), select_user_data(other.select_user_data),
          on_toggle(other.on_toggle), toggle_user_data(other.toggle_user_data),
          on_activate(other.on_activate), activate_user_data(other.activate_user_data),
          theme_signal(other.theme_signal), on_theme(other.on_theme), tree(other.tree),
          node_handle(other.node_handle) {}

    void rebuild_visible_rows();
    TreeNodeItem* find_node(uint32_t id);
    void select_node(uint32_t id);
    void toggle_node(uint32_t id);
    void scroll_into_view(size_t row_idx, float viewport_h);
    void apply_theme(const style::Theme& theme);
};

enum class SortDirection : uint8_t { None, Ascending, Descending };

struct TableColumn {
    std::string title;
    float width{120.0f};
    float flex{0.0f};
    float min_width{48.0f};
    float computed_x{0.0f};
    float computed_width{120.0f};
    text::TextAlign align{text::TextAlign::Left};
    bool sortable{true};
    bool resizable{true};
};

struct TableRow {
    uint32_t id{0};
    std::vector<std::string> cells;
    void* user_data{nullptr};
};

using TableSelectCallbackFn = void (*)(size_t row_idx, uint32_t row_id, void* user_data);
using TableSortCallbackFn = void (*)(size_t col_idx, SortDirection dir, void* user_data);
using TableActivateCallbackFn = void (*)(size_t row_idx, uint32_t row_id, void* user_data);
struct DataTableData;
using DataTableThemeFn = void (*)(const style::Theme& theme, DataTableData& data);

struct DataTableData {
    WidgetKind kind{WidgetKind::DataTable};
    std::vector<TableColumn> columns;
    std::vector<TableRow> rows;
    std::vector<size_t> sorted_indices;

    size_t sort_column{SIZE_MAX};
    SortDirection sort_direction{SortDirection::None};

    size_t selected_row{SIZE_MAX};
    size_t hovered_row{SIZE_MAX};
    size_t hovered_header{SIZE_MAX};
    size_t hovered_resizer{SIZE_MAX};

    bool is_resizing_column{false};
    size_t resizing_column{SIZE_MAX};
    float resize_start_pointer_x{0.0f};
    float resize_start_column_w{0.0f};

    bool is_focused{false};

    std::shared_ptr<text::Font> font;
    float font_size{13.0f};
    float header_height{34.0f};
    float row_height{28.0f};
    float scroll_y{0.0f};
    float max_scroll_y{0.0f};
    bool alternating_rows{true};

    float width{0.0f};
    float height{0.0f};

    core::Color background_color{0.13f, 0.14f, 0.17f, 1.0f};
    core::Color header_bg{0.15f, 0.16f, 0.20f, 1.0f};
    core::Color header_hover_bg{0.19f, 0.21f, 0.25f, 1.0f};
    core::Color row_alt_bg{0.11f, 0.12f, 0.15f, 1.0f};
    core::Color hover_row_bg{0.19f, 0.21f, 0.25f, 1.0f};
    core::Color selected_row_bg{0.26f, 0.35f, 0.48f, 1.0f};
    core::Color text_color{0.92f, 0.94f, 0.96f, 1.0f};
    core::Color header_text_color{0.96f, 0.97f, 0.98f, 1.0f};
    core::Color border_color{0.24f, 0.26f, 0.30f, 1.0f};
    core::Color resizer_hover_color{0.45f, 0.55f, 0.75f, 1.0f};
    core::Color sort_icon_color{0.68f, 0.71f, 0.78f, 1.0f};
    core::Color scrollbar_thumb_color{0.40f, 0.43f, 0.50f, 0.80f};

    TableSelectCallbackFn on_select{nullptr};
    void* select_user_data{nullptr};
    TableSortCallbackFn on_sort{nullptr};
    void* sort_user_data{nullptr};
    TableActivateCallbackFn on_activate{nullptr};
    void* activate_user_data{nullptr};

    core::Signal<style::Theme>* theme_signal{nullptr};
    DataTableThemeFn on_theme{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};

    DataTableData() = default;
    DataTableData(DataTableData&&) noexcept = default;
    DataTableData& operator=(DataTableData&&) noexcept = default;
    ~DataTableData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    DataTableData(const DataTableData& other)
        : kind(other.kind), columns(other.columns), rows(other.rows),
          sorted_indices(other.sorted_indices), sort_column(other.sort_column),
          sort_direction(other.sort_direction), selected_row(other.selected_row),
          hovered_row(other.hovered_row), hovered_header(other.hovered_header),
          hovered_resizer(other.hovered_resizer), is_resizing_column(other.is_resizing_column),
          resizing_column(other.resizing_column),
          resize_start_pointer_x(other.resize_start_pointer_x),
          resize_start_column_w(other.resize_start_column_w), is_focused(other.is_focused),
          font(other.font), font_size(other.font_size), header_height(other.header_height),
          row_height(other.row_height), scroll_y(other.scroll_y),
          max_scroll_y(other.max_scroll_y), alternating_rows(other.alternating_rows),
          width(other.width), height(other.height), background_color(other.background_color),
          header_bg(other.header_bg), header_hover_bg(other.header_hover_bg),
          row_alt_bg(other.row_alt_bg), hover_row_bg(other.hover_row_bg),
          selected_row_bg(other.selected_row_bg), text_color(other.text_color),
          header_text_color(other.header_text_color), border_color(other.border_color),
          resizer_hover_color(other.resizer_hover_color), sort_icon_color(other.sort_icon_color),
          scrollbar_thumb_color(other.scrollbar_thumb_color), on_select(other.on_select),
          select_user_data(other.select_user_data), on_sort(other.on_sort),
          sort_user_data(other.sort_user_data), on_activate(other.on_activate),
          activate_user_data(other.activate_user_data), theme_signal(other.theme_signal),
          on_theme(other.on_theme), tree(other.tree), node_handle(other.node_handle) {}

    void sort_by_column(size_t col_idx, SortDirection dir);
    void cycle_sort(size_t col_idx);
    void select_row(size_t row_idx);
    void scroll_into_view(size_t row_idx, float viewport_h);
    void apply_theme(const style::Theme& theme);
};

} // namespace north::widget
