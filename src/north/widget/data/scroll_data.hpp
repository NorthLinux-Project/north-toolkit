#pragma once

#include "north/widget/data/core_data.hpp"

namespace north::widget {

enum class ScrollDirection : uint8_t { Vertical, Horizontal, Both };

struct ScrollbarStyle {
    float thickness{4.0f};
    float hover_thickness{8.0f};
    float min_thumb_length{24.0f};
    core::Color track_color{0.0f, 0.0f, 0.0f, 0.0f};
    core::Color thumb_color{1.0f, 1.0f, 1.0f, 0.45f};
    core::Color thumb_hover_color{1.0f, 1.0f, 1.0f, 0.70f};
    core::Color thumb_drag_color{1.0f, 1.0f, 1.0f, 0.90f};

    [[nodiscard]] static ScrollbarStyle from_theme(const style::Theme& theme) noexcept {
        const auto& c = theme.colors();
        ScrollbarStyle style;
        style.track_color = core::Color{0.0f, 0.0f, 0.0f, 0.0f};
        style.thumb_color = core::Color{c.on_surface.r, c.on_surface.g, c.on_surface.b, 0.45f};
        style.thumb_hover_color =
            core::Color{c.on_surface.r, c.on_surface.g, c.on_surface.b, 0.70f};
        style.thumb_drag_color = core::Color{c.on_surface.r, c.on_surface.g, c.on_surface.b, 0.90f};
        return style;
    }
};

struct ScrollAreaData {
    WidgetKind kind{WidgetKind::ScrollArea};
    ScrollDirection direction{ScrollDirection::Vertical};
    core::Point scroll_offset{0.0f, 0.0f};
    core::Point target_scroll{0.0f, 0.0f};
    core::Point max_scroll{0.0f, 0.0f};
    core::Size content_size{0.0f, 0.0f};
    core::Size viewport_size{0.0f, 0.0f};
    ScrollbarStyle scrollbar_style{};
    bool smooth_scroll{false};
    bool is_hovered{false};
    bool is_v_thumb_hovered{false};
    bool is_h_thumb_hovered{false};
    bool is_v_thumb_dragging{false};
    bool is_h_thumb_dragging{false};
    float drag_start_pointer_y{0.0f};
    float drag_start_scroll_y{0.0f};
    float drag_start_pointer_x{0.0f};
    float drag_start_scroll_x{0.0f};
    void (*on_scroll)(core::Point offset, void* user_data){nullptr};
    void* scroll_user_data{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    animation::AnimatedValue<core::Point> animated_scroll;
    animation::AnimatedValue<float> animated_v_thickness;
    animation::AnimatedValue<float> animated_h_thickness;
    animation::AnimatedValue<core::Color> animated_thumb_color;
    core::Signal<style::Theme>* theme_signal{nullptr};

    ScrollAreaData() = default;
    ScrollAreaData(ScrollAreaData&&) noexcept = default;
    ScrollAreaData& operator=(ScrollAreaData&&) noexcept = default;
    ~ScrollAreaData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    ScrollAreaData(const ScrollAreaData& other)
        : kind(other.kind), direction(other.direction), scroll_offset(other.scroll_offset),
          target_scroll(other.target_scroll), max_scroll(other.max_scroll),
          content_size(other.content_size), viewport_size(other.viewport_size),
          scrollbar_style(other.scrollbar_style), smooth_scroll(other.smooth_scroll),
          is_hovered(other.is_hovered), is_v_thumb_hovered(other.is_v_thumb_hovered),
          is_h_thumb_hovered(other.is_h_thumb_hovered),
          is_v_thumb_dragging(other.is_v_thumb_dragging),
          is_h_thumb_dragging(other.is_h_thumb_dragging),
          drag_start_pointer_y(other.drag_start_pointer_y),
          drag_start_scroll_y(other.drag_start_scroll_y),
          drag_start_pointer_x(other.drag_start_pointer_x),
          drag_start_scroll_x(other.drag_start_scroll_x), on_scroll(other.on_scroll),
          scroll_user_data(other.scroll_user_data), tree(other.tree),
          node_handle(other.node_handle), theme_signal(other.theme_signal) {}

    void apply_theme(const style::Theme& theme);
};

using ItemBuilderFn = core::Handle<core::Node> (*)(core::NodeTree& tree, size_t index,
                                                   void* user_data);
using ItemExtentFn = float (*)(size_t index, void* user_data);

struct VirtualListData {
    WidgetKind kind{WidgetKind::VirtualList};
    size_t item_count{0};
    float fixed_item_extent{40.0f};
    ItemExtentFn item_extent_fn{nullptr};
    void* extent_user_data{nullptr};
    ItemBuilderFn item_builder{nullptr};
    void* builder_user_data{nullptr};
    core::Point scroll_offset{0.0f, 0.0f};
    float target_scroll_y{0.0f};
    core::Point max_scroll{0.0f, 0.0f};
    core::Size viewport_size{0.0f, 0.0f};
    size_t first_visible_idx{0};
    size_t last_visible_idx{0};
    bool window_initialized{false};
    ScrollbarStyle scrollbar_style{};
    bool smooth_scroll{false};
    bool is_hovered{false};
    bool is_thumb_hovered{false};
    bool is_thumb_dragging{false};
    float drag_start_pointer_y{0.0f};
    float drag_start_scroll_y{0.0f};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    animation::AnimatedValue<float> animated_scroll_y;
    animation::AnimatedValue<float> animated_thickness;
    animation::AnimatedValue<core::Color> animated_thumb_color;
    core::Signal<style::Theme>* theme_signal{nullptr};

    VirtualListData() = default;
    VirtualListData(VirtualListData&&) noexcept = default;
    VirtualListData& operator=(VirtualListData&&) noexcept = default;
    ~VirtualListData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    VirtualListData(const VirtualListData& other)
        : kind(other.kind), item_count(other.item_count),
          fixed_item_extent(other.fixed_item_extent), item_extent_fn(other.item_extent_fn),
          extent_user_data(other.extent_user_data), item_builder(other.item_builder),
          builder_user_data(other.builder_user_data), scroll_offset(other.scroll_offset),
          target_scroll_y(other.target_scroll_y), max_scroll(other.max_scroll),
          viewport_size(other.viewport_size), first_visible_idx(other.first_visible_idx),
          last_visible_idx(other.last_visible_idx), window_initialized(other.window_initialized),
          scrollbar_style(other.scrollbar_style),
          smooth_scroll(other.smooth_scroll), is_hovered(other.is_hovered),
          is_thumb_hovered(other.is_thumb_hovered), is_thumb_dragging(other.is_thumb_dragging),
          drag_start_pointer_y(other.drag_start_pointer_y),
          drag_start_scroll_y(other.drag_start_scroll_y), tree(other.tree),
          node_handle(other.node_handle), theme_signal(other.theme_signal) {}

    void apply_theme(const style::Theme& theme);
};

} // namespace north::widget
