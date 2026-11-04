#include "north/widget/scroll_area.hpp"

#include "north/render/display_list.hpp"

#include <algorithm>

namespace north::widget {

namespace {

constexpr float kUnboundedDimension = 1000000.0f;

core::Size scroll_area_layout_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                                 const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<ScrollAreaData*>(user_data);
    auto* node = tree.get(handle);
    if (!node || !data) {
        return constraints.constrain({0.0f, 0.0f});
    }

    float max_allowed_w =
        (data->viewport_size.width > 0.0f) ? data->viewport_size.width : constraints.max_width;
    float max_allowed_h =
        (data->viewport_size.height > 0.0f) ? data->viewport_size.height : constraints.max_height;

    layout::BoxConstraints child_constraints{
        0.0f,
        (data->direction == ScrollDirection::Horizontal || data->direction == ScrollDirection::Both)
            ? kUnboundedDimension
            : max_allowed_w,
        0.0f,
        (data->direction == ScrollDirection::Vertical || data->direction == ScrollDirection::Both)
            ? kUnboundedDimension
            : max_allowed_h};

    core::Size max_child_size{0.0f, 0.0f};
    for (auto ch : node->children) {
        auto* ch_node = tree.get(ch);
        if (!ch_node)
            continue;

        core::Size ch_size{0.0f, 0.0f};
        if (ch_node->layout_fn) {
            ch_size = ch_node->layout_fn(tree, ch, child_constraints, ch_node->user_data);
        } else {
            ch_size = child_constraints.constrain({0.0f, 0.0f});
        }
        ch_node->layout.size = ch_size;
        max_child_size.width = (std::max)(max_child_size.width, ch_size.width);
        max_child_size.height = (std::max)(max_child_size.height, ch_size.height);
    }

    data->content_size = max_child_size;

    float resolved_w = (data->viewport_size.width > 0.0f)
                           ? data->viewport_size.width
                           : (std::min)(max_child_size.width, constraints.max_width);
    float resolved_h = (data->viewport_size.height > 0.0f)
                           ? data->viewport_size.height
                           : (std::min)(max_child_size.height, constraints.max_height);

    core::Size viewport = constraints.constrain({resolved_w, resolved_h});

    data->max_scroll.x = (std::max)(0.0f, data->content_size.width - viewport.width);
    data->max_scroll.y = (std::max)(0.0f, data->content_size.height - viewport.height);

    data->scroll_offset.x = std::clamp(data->scroll_offset.x, 0.0f, data->max_scroll.x);
    data->scroll_offset.y = std::clamp(data->scroll_offset.y, 0.0f, data->max_scroll.y);

    for (auto ch : node->children) {
        auto* ch_node = tree.get(ch);
        if (ch_node) {
            ch_node->layout.offset = core::Point{-data->scroll_offset.x, -data->scroll_offset.y};
        }
    }

    return viewport;
}

void scroll_area_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                          void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    if (!dl)
        return;
    auto* data = reinterpret_cast<ScrollAreaData*>(user_data);
    auto* node = tree.get(handle);
    if (!node || !data)
        return;

    core::Rect viewport{origin.x, origin.y, node->layout.size.width, node->layout.size.height};

    dl->push_clip(viewport);
    tree.paint_children(handle, origin, stream);
    dl->pop_clip();

    const auto& s = data->scrollbar_style;

    if ((data->direction == ScrollDirection::Vertical ||
         data->direction == ScrollDirection::Both) &&
        data->max_scroll.y > 0.0f) {
        float track_h = viewport.height;
        float thumb_h =
            (std::max)(s.min_thumb_length, (viewport.height / data->content_size.height) * track_h);
        float scroll_ratio =
            (data->max_scroll.y > 0.0f) ? (data->scroll_offset.y / data->max_scroll.y) : 0.0f;
        float thumb_y = origin.y + scroll_ratio * (track_h - thumb_h);
        float thickness = data->animated_v_thickness.get();
        if (thickness <= 0.0f) {
            thickness = (data->is_v_thumb_hovered || data->is_v_thumb_dragging) ? s.hover_thickness
                                                                                : s.thickness;
        }
        float thumb_x = origin.x + viewport.width - thickness - 2.0f;

        if (s.track_color.a > 0.0f) {
            core::Rect track_rect{origin.x + viewport.width - thickness - 2.0f, origin.y, thickness,
                                  track_h};
            dl->push_quad(track_rect, core::CornerRadii::uniform(thickness * 0.5f), s.track_color);
        }

        core::Color thumb_col =
            data->is_v_thumb_dragging
                ? s.thumb_drag_color
                : (data->is_v_thumb_hovered ? s.thumb_hover_color : s.thumb_color);
        core::Rect thumb_rect{thumb_x, thumb_y, thickness, thumb_h};
        dl->push_quad(thumb_rect, core::CornerRadii::uniform(thickness * 0.5f), thumb_col);
    }

    if ((data->direction == ScrollDirection::Horizontal ||
         data->direction == ScrollDirection::Both) &&
        data->max_scroll.x > 0.0f) {
        float track_w = viewport.width;
        float thumb_w =
            (std::max)(s.min_thumb_length, (viewport.width / data->content_size.width) * track_w);
        float scroll_ratio =
            (data->max_scroll.x > 0.0f) ? (data->scroll_offset.x / data->max_scroll.x) : 0.0f;
        float thumb_x = origin.x + scroll_ratio * (track_w - thumb_w);
        float thickness = data->animated_h_thickness.get();
        if (thickness <= 0.0f) {
            thickness = (data->is_h_thumb_hovered || data->is_h_thumb_dragging) ? s.hover_thickness
                                                                                : s.thickness;
        }
        float thumb_y = origin.y + viewport.height - thickness - 2.0f;

        if (s.track_color.a > 0.0f) {
            core::Rect track_rect{origin.x, origin.y + viewport.height - thickness - 2.0f, track_w,
                                  thickness};
            dl->push_quad(track_rect, core::CornerRadii::uniform(thickness * 0.5f), s.track_color);
        }

        core::Color thumb_col =
            data->is_h_thumb_dragging
                ? s.thumb_drag_color
                : (data->is_h_thumb_hovered ? s.thumb_hover_color : s.thumb_color);
        core::Rect thumb_rect{thumb_x, thumb_y, thumb_w, thickness};
        dl->push_quad(thumb_rect, core::CornerRadii::uniform(thickness * 0.5f), thumb_col);
    }
}

bool scroll_area_hit_test_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                             core::Point local_point, void*) {
    auto* node = tree.get(handle);
    if (!node)
        return false;
    core::Rect local_bounds{0.0f, 0.0f, node->layout.size.width, node->layout.size.height};
    return local_bounds.contains(local_point);
}

void scroll_area_destroy_fn(void* user_data) {
    delete reinterpret_cast<ScrollAreaData*>(user_data);
}

} // namespace

ScrollAreaBuilder::ScrollAreaBuilder(core::NodeTree& tree) : tree_(tree) {
    theme(style::Theme::dark());
}

ScrollAreaBuilder& ScrollAreaBuilder::theme(const style::Theme& theme) {
    data_.scrollbar_style = ScrollbarStyle::from_theme(theme);
    return *this;
}

ScrollAreaBuilder& ScrollAreaBuilder::smooth_scroll(bool enable) {
    data_.smooth_scroll = enable;
    return *this;
}

ScrollAreaBuilder& ScrollAreaBuilder::direction(ScrollDirection dir) {
    data_.direction = dir;
    return *this;
}

ScrollAreaBuilder& ScrollAreaBuilder::width(float w) {
    width_ = w;
    return *this;
}

ScrollAreaBuilder& ScrollAreaBuilder::height(float h) {
    height_ = h;
    return *this;
}

ScrollAreaBuilder& ScrollAreaBuilder::scrollbar_style(const ScrollbarStyle& style) {
    data_.scrollbar_style = style;
    return *this;
}

ScrollAreaBuilder& ScrollAreaBuilder::scroll_offset(core::Point offset) {
    data_.scroll_offset = offset;
    return *this;
}

ScrollAreaBuilder& ScrollAreaBuilder::child(core::Handle<core::Node> child_handle) {
    child_ = child_handle;
    return *this;
}

ScrollAreaBuilder& ScrollAreaBuilder::on_scroll(void (*fn)(core::Point, void*), void* user_data) {
    data_.on_scroll = fn;
    data_.scroll_user_data = user_data;
    return *this;
}

core::Handle<core::Node> ScrollAreaBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node)
        return {};

    data_.viewport_size = {width_, height_};

    auto* data_heap = new ScrollAreaData(std::move(data_));
    data_heap->node_handle = handle;
    data_heap->tree = &tree_;
    data_heap->target_scroll = data_heap->scroll_offset;
    data_heap->animated_scroll.set_immediate(data_heap->scroll_offset);
    data_heap->animated_v_thickness.set_immediate(data_heap->scrollbar_style.thickness);
    data_heap->animated_h_thickness.set_immediate(data_heap->scrollbar_style.thickness);

    data_heap->animated_scroll.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* sa = static_cast<ScrollAreaData*>(ctx);
            sa->scroll_offset = sa->animated_scroll.get();
            if (sa->on_scroll) {
                sa->on_scroll(sa->scroll_offset, sa->scroll_user_data);
            }
            if (sa->tree && sa->node_handle.is_valid()) {
                sa->tree->mark_dirty(sa->node_handle, flags);
            }
        },
        data_heap, core::kLayout | core::kPaint);

    data_heap->animated_v_thickness.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* sa = static_cast<ScrollAreaData*>(ctx);
            if (sa->tree && sa->node_handle.is_valid()) {
                sa->tree->mark_dirty(sa->node_handle, flags);
            }
        },
        data_heap, core::kPaint);

    data_heap->animated_h_thickness.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* sa = static_cast<ScrollAreaData*>(ctx);
            if (sa->tree && sa->node_handle.is_valid()) {
                sa->tree->mark_dirty(sa->node_handle, flags);
            }
        },
        data_heap, core::kPaint);

    node->user_data = data_heap;
    node->layout_fn = scroll_area_layout_fn;
    node->paint_fn = scroll_area_paint_fn;
    node->hit_test_fn = scroll_area_hit_test_fn;
    node->destroy_fn = scroll_area_destroy_fn;
    node->custom_child_paint = true;

    if (child_.is_valid()) {
        tree_.append_child(handle, child_);
    }

    return handle;
}

} // namespace north::widget
