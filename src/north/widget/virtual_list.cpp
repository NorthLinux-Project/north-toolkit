#include "north/widget/virtual_list.hpp"

#include "north/render/display_list.hpp"

#include <algorithm>
#include <cmath>

namespace north::widget {

namespace {

core::Size virtual_list_layout_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                                  const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<VirtualListData*>(user_data);
    auto* node = tree.get(handle);
    if (!node || !data) {
        return constraints.constrain({0.0f, 0.0f});
    }

    float total_h = 0.0f;
    if (data->item_extent_fn) {
        for (size_t i = 0; i < data->item_count; ++i) {
            total_h += data->item_extent_fn(i, data->extent_user_data);
        }
    } else {
        total_h = static_cast<float>(data->item_count) * data->fixed_item_extent;
    }

    float resolved_w =
        (data->viewport_size.width > 0.0f) ? data->viewport_size.width : constraints.max_width;
    float resolved_h =
        (data->viewport_size.height > 0.0f) ? data->viewport_size.height : constraints.max_height;
    core::Size viewport = constraints.constrain({resolved_w, resolved_h});

    data->max_scroll.y = (std::max)(0.0f, total_h - viewport.height);
    data->scroll_offset.y = std::clamp(data->scroll_offset.y, 0.0f, data->max_scroll.y);

    if (data->item_count == 0 || !data->item_builder) {
        while (!node->children.empty()) {
            tree.destroy_node(node->children.back());
        }
        data->window_initialized = false;
        return viewport;
    }

    float item_h = (data->fixed_item_extent > 0.0f) ? data->fixed_item_extent : 40.0f;
    int first = static_cast<int>(std::floor((data->scroll_offset.y - item_h) / item_h));
    first = (std::max)(0, first);
    int last =
        static_cast<int>(std::ceil((data->scroll_offset.y + viewport.height + item_h) / item_h));
    last = (std::min)(static_cast<int>(data->item_count - 1), last);

    if (data->window_initialized && first == static_cast<int>(data->first_visible_idx) &&
        last == static_cast<int>(data->last_visible_idx) &&
        node->children.size() == static_cast<size_t>(last - first + 1)) {
        for (int i = first; i <= last; ++i) {
            auto ch = node->children[static_cast<size_t>(i - first)];
            auto* ch_node = tree.get(ch);
            if (ch_node) {
                ch_node->layout.offset =
                    core::Point{0.0f, static_cast<float>(i) * item_h - data->scroll_offset.y};
            }
        }
        return viewport;
    }

    data->first_visible_idx = static_cast<size_t>(first);
    data->last_visible_idx = static_cast<size_t>(last);
    data->window_initialized = true;

    while (!node->children.empty()) {
        tree.destroy_node(node->children.back());
    }

    layout::BoxConstraints item_constraints =
        layout::BoxConstraints::tight({viewport.width, item_h});

    for (int i = first; i <= last; ++i) {
        auto ch = data->item_builder(tree, static_cast<size_t>(i), data->builder_user_data);
        if (!ch.is_valid())
            continue;

        tree.append_child(handle, ch);
        auto* ch_node = tree.get(ch);
        if (ch_node) {
            ch_node->layout.offset =
                core::Point{0.0f, static_cast<float>(i) * item_h - data->scroll_offset.y};
            if (ch_node->layout_fn) {
                ch_node->layout.size =
                    ch_node->layout_fn(tree, ch, item_constraints, ch_node->user_data);
            } else {
                ch_node->layout.size = item_constraints.constrain({0.0f, 0.0f});
            }
        }
    }

    return viewport;
}

void virtual_list_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                           core::Point origin, void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    if (!dl)
        return;
    auto* data = reinterpret_cast<VirtualListData*>(user_data);
    auto* node = tree.get(handle);
    if (!node || !data)
        return;

    core::Rect viewport{origin.x, origin.y, node->layout.size.width, node->layout.size.height};

    dl->push_clip(viewport);
    tree.paint_children(handle, origin, stream);
    dl->pop_clip();

    const auto& s = data->scrollbar_style;
    if (data->max_scroll.y > 0.0f) {
        float total_h = (data->fixed_item_extent > 0.0f)
                            ? (static_cast<float>(data->item_count) * data->fixed_item_extent)
                            : 1.0f;
        float track_h = viewport.height;
        float thumb_h = (std::max)(s.min_thumb_length, (viewport.height / total_h) * track_h);
        float scroll_ratio =
            (data->max_scroll.y > 0.0f) ? (data->scroll_offset.y / data->max_scroll.y) : 0.0f;
        float thumb_y = origin.y + scroll_ratio * (track_h - thumb_h);
        float thickness = data->animated_thickness.get();
        if (thickness <= 0.0f) {
            thickness = (data->is_thumb_hovered || data->is_thumb_dragging) ? s.hover_thickness
                                                                            : s.thickness;
        }
        float thumb_x = origin.x + viewport.width - thickness - 2.0f;

        if (s.track_color.a > 0.0f) {
            core::Rect track_rect{origin.x + viewport.width - thickness - 2.0f, origin.y, thickness,
                                  track_h};
            dl->push_quad(track_rect, core::CornerRadii::uniform(thickness * 0.5f), s.track_color);
        }

        core::Color thumb_col =
            data->is_thumb_dragging
                ? s.thumb_drag_color
                : (data->is_thumb_hovered ? s.thumb_hover_color
                                          : data->animated_thumb_color.get());
        core::Rect thumb_rect{thumb_x, thumb_y, thickness, thumb_h};
        dl->push_quad(thumb_rect, core::CornerRadii::uniform(thickness * 0.5f), thumb_col);
    }
}

bool virtual_list_hit_test_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                              core::Point local_point, void*) {
    auto* node = tree.get(handle);
    if (!node)
        return false;
    core::Rect local_bounds{0.0f, 0.0f, node->layout.size.width, node->layout.size.height};
    return local_bounds.contains(local_point);
}

void virtual_list_destroy_fn(void* user_data) {
    delete reinterpret_cast<VirtualListData*>(user_data);
}

} // namespace

void VirtualListData::apply_theme(const style::Theme& theme) {
    scrollbar_style = ScrollbarStyle::from_theme(theme);
    float duration = theme.transition_duration();
    if (duration <= 0.0f) {
        animated_thumb_color.set_immediate(scrollbar_style.thumb_color);
    } else {
        animated_thumb_color.tween_to(scrollbar_style.thumb_color, duration, animation::CubicBezier::ease_out());
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

VirtualListBuilder::VirtualListBuilder(core::NodeTree& tree) : tree_(tree) {
    theme(style::Theme::dark());
}

VirtualListBuilder& VirtualListBuilder::theme(const style::Theme& theme) {
    data_.scrollbar_style = ScrollbarStyle::from_theme(theme);
    return *this;
}

VirtualListBuilder& VirtualListBuilder::theme_signal(core::Signal<style::Theme>* sig) {
    data_.theme_signal = sig;
    if (sig) {
        theme(sig->get());
    }
    return *this;
}

VirtualListBuilder& VirtualListBuilder::item_count(size_t count) {
    data_.item_count = count;
    return *this;
}

VirtualListBuilder& VirtualListBuilder::item_extent(float extent) {
    data_.fixed_item_extent = extent;
    return *this;
}

VirtualListBuilder& VirtualListBuilder::item_extent_fn(ItemExtentFn fn, void* user_data) {
    data_.item_extent_fn = fn;
    data_.extent_user_data = user_data;
    return *this;
}

VirtualListBuilder& VirtualListBuilder::item_builder(ItemBuilderFn fn, void* user_data) {
    data_.item_builder = fn;
    data_.builder_user_data = user_data;
    return *this;
}

VirtualListBuilder& VirtualListBuilder::width(float w) {
    width_ = w;
    return *this;
}

VirtualListBuilder& VirtualListBuilder::height(float h) {
    height_ = h;
    return *this;
}

VirtualListBuilder& VirtualListBuilder::smooth_scroll(bool enable) {
    data_.smooth_scroll = enable;
    return *this;
}

VirtualListBuilder& VirtualListBuilder::scrollbar_style(const ScrollbarStyle& style) {
    data_.scrollbar_style = style;
    return *this;
}

VirtualListBuilder& VirtualListBuilder::scroll_offset(float y) {
    data_.scroll_offset.y = y;
    return *this;
}

core::Handle<core::Node> VirtualListBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node)
        return {};

    data_.viewport_size = {width_, height_};

    auto* data_heap = new VirtualListData(std::move(data_));
    data_heap->node_handle = handle;
    data_heap->tree = &tree_;
    data_heap->target_scroll_y = data_heap->scroll_offset.y;
    data_heap->animated_scroll_y.set_immediate(data_heap->scroll_offset.y);
    data_heap->animated_thickness.set_immediate(data_heap->scrollbar_style.thickness);
    data_heap->animated_thumb_color.set_immediate(data_heap->scrollbar_style.thumb_color);

    data_heap->animated_scroll_y.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* vl = static_cast<VirtualListData*>(ctx);
            vl->scroll_offset.y = vl->animated_scroll_y.get();
            if (vl->tree && vl->node_handle.is_valid()) {
                vl->tree->mark_dirty(vl->node_handle, flags);
            }
        },
        data_heap, core::kLayout | core::kPaint);

    data_heap->animated_thickness.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* vl = static_cast<VirtualListData*>(ctx);
            if (vl->tree && vl->node_handle.is_valid()) {
                vl->tree->mark_dirty(vl->node_handle, flags);
            }
        },
        data_heap, core::kPaint);

    data_heap->animated_thumb_color.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* vl = static_cast<VirtualListData*>(ctx);
            if (vl->tree && vl->node_handle.is_valid()) {
                vl->tree->mark_dirty(vl->node_handle, flags);
            }
        },
        data_heap, core::kPaint);

    if (data_heap->theme_signal) {
        data_heap->theme_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* vl = static_cast<VirtualListData*>(ctx);
                if (vl && vl->theme_signal) {
                    vl->apply_theme(vl->theme_signal->get());
                }
            },
            data_heap, core::kPaint);
    }

    node->user_data = data_heap;
    node->layout_fn = virtual_list_layout_fn;
    node->paint_fn = virtual_list_paint_fn;
    node->hit_test_fn = virtual_list_hit_test_fn;
    node->destroy_fn = virtual_list_destroy_fn;
    node->custom_child_paint = true;

    return handle;
}

} // namespace north::widget
