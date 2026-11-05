#pragma once

#include "north/widget/data/core_data.hpp"

#include <vector>

namespace north::widget {

enum class TabRetentionPolicy : uint8_t {
    KeepAlive = 0,
    Lazy
};

using TabContentFactoryFn = core::Handle<core::Node> (*)(core::NodeTree& tree, size_t index,
                                                         void* user_data);
using TabSelectCallbackFn = void (*)(size_t index, void* user_data);
using TabCloseCallbackFn = void (*)(size_t index, void* user_data);
using TabReorderCallbackFn = void (*)(size_t from_index, size_t to_index, void* user_data);

struct TabItem {
    std::string label;
    render::Path icon_path;
    bool closable{true};
    bool show_badge{false};
    std::string badge_text;
    core::Color badge_color{0.92f, 0.34f, 0.34f, 1.0f};
    core::Handle<core::Node> content_node{};
    TabContentFactoryFn build_fn{nullptr};
    void* user_data{nullptr};

    float computed_x{0.0f};
    float computed_width{0.0f};
    core::Rect close_rect{};
};

struct TabsViewData;
struct TabBarData;
using TabBarThemeFn = void (*)(const style::Theme& theme, TabBarData& data);

struct TabBarData {
    WidgetKind kind{WidgetKind::TabBar};
    std::vector<TabItem> tabs;
    size_t active_index{0};
    size_t hovered_index{SIZE_MAX};
    size_t close_hovered_index{SIZE_MAX};
    bool add_button_hovered{false};
    bool left_scroll_hovered{false};
    bool right_scroll_hovered{false};
    bool is_focused{false};
    bool show_add_button{false};
    bool reorderable{true};

    std::shared_ptr<text::Font> font;
    float font_size{13.0f};
    float bar_height{40.0f};
    float tab_min_width{80.0f};
    float tab_max_width{220.0f};
    core::Edges tab_padding{8.0f, 14.0f, 8.0f, 14.0f};

    float scroll_offset{0.0f};
    float max_scroll_offset{0.0f};
    float total_tabs_width{0.0f};
    bool has_overflow{false};
    core::Rect add_button_rect{};
    core::Rect left_scroll_rect{};
    core::Rect right_scroll_rect{};

    bool is_dragging{false};
    bool drag_threshold_exceeded{false};
    size_t drag_index{SIZE_MAX};
    size_t drop_target_index{SIZE_MAX};
    float drag_start_mouse_x{0.0f};
    float drag_current_mouse_x{0.0f};
    float drag_grab_offset_x{0.0f};

    core::Color background_color{0.10f, 0.11f, 0.13f, 1.0f};
    core::Color active_tab_bg{0.16f, 0.17f, 0.21f, 1.0f};
    core::Color hovered_tab_bg{0.14f, 0.15f, 0.18f, 1.0f};
    core::Color active_text_color{0.95f, 0.96f, 0.98f, 1.0f};
    core::Color inactive_text_color{0.62f, 0.65f, 0.72f, 1.0f};
    core::Color indicator_color{0.35f, 0.58f, 0.98f, 1.0f};
    core::Color border_color{0.22f, 0.24f, 0.28f, 1.0f};
    core::Color close_hover_bg{0.90f, 0.30f, 0.30f, 0.25f};

    animation::AnimatedValue<float> indicator_x{0.0f};
    animation::AnimatedValue<float> indicator_width{0.0f};
    bool indicator_initialized{false};

    TabSelectCallbackFn on_select{nullptr};
    void* select_user_data{nullptr};
    TabCloseCallbackFn on_close{nullptr};
    void* close_user_data{nullptr};
    ClickCallbackFn on_add{nullptr};
    void* add_user_data{nullptr};
    TabReorderCallbackFn on_reorder{nullptr};
    void* reorder_user_data{nullptr};

    core::Signal<style::Theme>* theme_signal{nullptr};
    TabBarThemeFn on_theme{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    TabsViewData* parent_tabs_view{nullptr};

    TabBarData() = default;
    TabBarData(TabBarData&&) noexcept = default;
    TabBarData& operator=(TabBarData&&) noexcept = default;
    ~TabBarData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    TabBarData(const TabBarData& other)
        : kind(other.kind), tabs(other.tabs), active_index(other.active_index),
          hovered_index(other.hovered_index), close_hovered_index(other.close_hovered_index),
          add_button_hovered(other.add_button_hovered),
          left_scroll_hovered(other.left_scroll_hovered),
          right_scroll_hovered(other.right_scroll_hovered), is_focused(other.is_focused),
          show_add_button(other.show_add_button), reorderable(other.reorderable),
          font(other.font), font_size(other.font_size), bar_height(other.bar_height),
          tab_min_width(other.tab_min_width), tab_max_width(other.tab_max_width),
          tab_padding(other.tab_padding), scroll_offset(other.scroll_offset),
          max_scroll_offset(other.max_scroll_offset), total_tabs_width(other.total_tabs_width),
          has_overflow(other.has_overflow), add_button_rect(other.add_button_rect),
          left_scroll_rect(other.left_scroll_rect), right_scroll_rect(other.right_scroll_rect),
          is_dragging(other.is_dragging), drag_threshold_exceeded(other.drag_threshold_exceeded),
          drag_index(other.drag_index), drop_target_index(other.drop_target_index),
          drag_start_mouse_x(other.drag_start_mouse_x),
          drag_current_mouse_x(other.drag_current_mouse_x),
          drag_grab_offset_x(other.drag_grab_offset_x), background_color(other.background_color),
          active_tab_bg(other.active_tab_bg), hovered_tab_bg(other.hovered_tab_bg),
          active_text_color(other.active_text_color),
          inactive_text_color(other.inactive_text_color), indicator_color(other.indicator_color),
          border_color(other.border_color), close_hover_bg(other.close_hover_bg),
          indicator_initialized(other.indicator_initialized), on_select(other.on_select),
          select_user_data(other.select_user_data), on_close(other.on_close),
          close_user_data(other.close_user_data), on_add(other.on_add),
          add_user_data(other.add_user_data), on_reorder(other.on_reorder),
          reorder_user_data(other.reorder_user_data), theme_signal(other.theme_signal),
          on_theme(other.on_theme), tree(other.tree), node_handle(other.node_handle),
          parent_tabs_view(other.parent_tabs_view) {}

    void select_tab(size_t index);
    bool close_tab(size_t index);
    bool reorder_tab(size_t from_index, size_t to_index);
    void apply_theme(const style::Theme& theme);
};

struct TabsViewData {
    WidgetKind kind{WidgetKind::TabsView};
    TabRetentionPolicy retention_policy{TabRetentionPolicy::KeepAlive};
    core::Handle<core::Node> bar_node{};
    TabBarData* bar_data{nullptr};
    core::Color content_bg{0.12f, 0.13f, 0.16f, 1.0f};
    core::Color border_color{0.22f, 0.24f, 0.28f, 1.0f};
    float width{0.0f};
    float height{0.0f};
    core::Signal<style::Theme>* theme_signal{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};

    TabsViewData() = default;
    TabsViewData(TabsViewData&&) noexcept = default;
    TabsViewData& operator=(TabsViewData&&) noexcept = default;
    ~TabsViewData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    TabsViewData(const TabsViewData& other)
        : kind(other.kind), retention_policy(other.retention_policy), bar_node(other.bar_node),
          bar_data(other.bar_data), content_bg(other.content_bg), border_color(other.border_color),
          width(other.width), height(other.height), theme_signal(other.theme_signal),
          tree(other.tree), node_handle(other.node_handle) {}

    void sync_active_content();
    void apply_theme(const style::Theme& theme);
};

using SegmentChangeCallbackFn = void (*)(size_t index, bool selected, void* user_data);

struct SegmentItem {
    std::string label;
    render::Path icon_path;
    bool disabled{false};
    float computed_x{0.0f};
    float computed_width{0.0f};
};

struct SegmentedButtonData;
using SegmentedButtonThemeFn = void (*)(const style::Theme& theme, SegmentedButtonData& data);

struct SegmentedButtonData {
    WidgetKind kind{WidgetKind::SegmentedButton};
    std::vector<SegmentItem> segments;
    std::vector<bool> selected;
    bool multi_select{false};
    size_t hovered_index{SIZE_MAX};
    size_t focused_index{0};
    bool is_focused{false};

    std::shared_ptr<text::Font> font;
    float font_size{13.0f};
    float height{36.0f};
    float width{0.0f};
    core::Edges segment_padding{6.0f, 16.0f, 6.0f, 16.0f};
    core::CornerRadii corner_radii{core::CornerRadii::uniform(8.0f)};

    core::Color container_bg{0.12f, 0.13f, 0.16f, 1.0f};
    core::Color border_color{0.24f, 0.26f, 0.30f, 1.0f};
    core::Color active_pill_bg{0.24f, 0.45f, 0.90f, 1.0f};
    core::Color hovered_segment_bg{0.18f, 0.20f, 0.24f, 0.6f};
    core::Color active_text_color{0.98f, 0.99f, 1.0f, 1.0f};
    core::Color inactive_text_color{0.72f, 0.75f, 0.80f, 1.0f};
    core::Color disabled_text_color{0.42f, 0.44f, 0.48f, 0.6f};

    animation::AnimatedValue<float> indicator_x{0.0f};
    animation::AnimatedValue<float> indicator_width{0.0f};
    bool indicator_initialized{false};

    SegmentChangeCallbackFn on_change{nullptr};
    void* change_user_data{nullptr};

    core::Signal<style::Theme>* theme_signal{nullptr};
    SegmentedButtonThemeFn on_theme{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};

    SegmentedButtonData() = default;
    SegmentedButtonData(SegmentedButtonData&&) noexcept = default;
    SegmentedButtonData& operator=(SegmentedButtonData&&) noexcept = default;
    ~SegmentedButtonData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    SegmentedButtonData(const SegmentedButtonData& other)
        : kind(other.kind), segments(other.segments), selected(other.selected),
          multi_select(other.multi_select), hovered_index(other.hovered_index),
          focused_index(other.focused_index), is_focused(other.is_focused), font(other.font),
          font_size(other.font_size), height(other.height), width(other.width),
          segment_padding(other.segment_padding), corner_radii(other.corner_radii),
          container_bg(other.container_bg), border_color(other.border_color),
          active_pill_bg(other.active_pill_bg), hovered_segment_bg(other.hovered_segment_bg),
          active_text_color(other.active_text_color),
          inactive_text_color(other.inactive_text_color),
          disabled_text_color(other.disabled_text_color),
          indicator_initialized(other.indicator_initialized), on_change(other.on_change),
          change_user_data(other.change_user_data), theme_signal(other.theme_signal),
          on_theme(other.on_theme), tree(other.tree), node_handle(other.node_handle) {}

    void toggle_segment(size_t index);
    [[nodiscard]] bool is_selected(size_t index) const noexcept;
    [[nodiscard]] size_t selected_index() const noexcept;
    void apply_theme(const style::Theme& theme);
};

using AccordionToggleCallbackFn = void (*)(size_t index, bool expanded, void* user_data);

struct AccordionItem {
    std::string title;
    core::Handle<core::Node> content_node{};
    bool expanded{false};
    float expand_progress{0.0f};
    float header_y{0.0f};
    float content_y{0.0f};
    float natural_content_height{0.0f};
    float visible_content_height{0.0f};
};

struct AccordionData;
using AccordionThemeFn = void (*)(const style::Theme& theme, AccordionData& data);

struct AccordionData {
    WidgetKind kind{WidgetKind::Accordion};
    std::vector<AccordionItem> items;
    std::vector<std::unique_ptr<animation::AnimatedValue<float>>> item_animations;
    bool allow_multiple{false};
    size_t hovered_header{SIZE_MAX};
    size_t focused_header{0};
    bool is_focused{false};

    std::shared_ptr<text::Font> font;
    float font_size{14.0f};
    float header_height{42.0f};
    float width{0.0f};
    core::Edges header_padding{10.0f, 16.0f, 10.0f, 16.0f};
    core::Edges content_padding{12.0f, 16.0f, 12.0f, 16.0f};
    core::CornerRadii corner_radii{core::CornerRadii::uniform(8.0f)};

    core::Color background_color{0.13f, 0.14f, 0.17f, 1.0f};
    core::Color header_bg{0.15f, 0.16f, 0.20f, 1.0f};
    core::Color header_hover_bg{0.19f, 0.21f, 0.25f, 1.0f};
    core::Color border_color{0.24f, 0.26f, 0.30f, 1.0f};
    core::Color text_color{0.92f, 0.94f, 0.96f, 1.0f};
    core::Color chevron_color{0.68f, 0.71f, 0.78f, 1.0f};

    AccordionToggleCallbackFn on_toggle{nullptr};
    void* toggle_user_data{nullptr};

    core::Signal<style::Theme>* theme_signal{nullptr};
    AccordionThemeFn on_theme{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};

    AccordionData() = default;
    AccordionData(AccordionData&&) noexcept = default;
    AccordionData& operator=(AccordionData&&) noexcept = default;
    ~AccordionData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    AccordionData(const AccordionData& other)
        : kind(other.kind), items(other.items), allow_multiple(other.allow_multiple),
          hovered_header(other.hovered_header), focused_header(other.focused_header),
          is_focused(other.is_focused), font(other.font), font_size(other.font_size),
          header_height(other.header_height), width(other.width),
          header_padding(other.header_padding), content_padding(other.content_padding),
          corner_radii(other.corner_radii), background_color(other.background_color),
          header_bg(other.header_bg), header_hover_bg(other.header_hover_bg),
          border_color(other.border_color), text_color(other.text_color),
          chevron_color(other.chevron_color), on_toggle(other.on_toggle),
          toggle_user_data(other.toggle_user_data), theme_signal(other.theme_signal),
          on_theme(other.on_theme), tree(other.tree), node_handle(other.node_handle) {}

    void toggle_item(size_t index);
    void set_expanded(size_t index, bool expand, bool animate = true);
    [[nodiscard]] bool is_expanded(size_t index) const noexcept;
    void apply_theme(const style::Theme& theme);
};

} // namespace north::widget
