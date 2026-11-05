#include "north/widget/box.hpp"

#include "north/render/display_list.hpp"

#include <algorithm>

namespace north::widget {

namespace {

core::Size box_layout_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                         const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<BoxData*>(user_data);
    auto* node = tree.get(handle);
    if (!node || !data) {
        return constraints.constrain({0.0f, 0.0f});
    }

    core::Edges pad = data->padding;
    float pad_w = pad.horizontal();
    float pad_h = pad.vertical();

    layout::BoxConstraints effective_constraints = constraints;
    if (data->width >= 0.0f) {
        effective_constraints.min_width = (std::min)(data->width, effective_constraints.max_width);
        effective_constraints.max_width = effective_constraints.min_width;
    }
    if (data->height >= 0.0f) {
        effective_constraints.min_height =
            (std::min)(data->height, effective_constraints.max_height);
        effective_constraints.max_height = effective_constraints.min_height;
    }

    layout::BoxConstraints inner_constraints = effective_constraints.deflate(pad);

    if (data->is_flex) {
        core::Size inner_size =
            layout::flex_layout(tree, handle, inner_constraints, data->flex_style);
        for (auto ch : node->children) {
            auto* ch_node = tree.get(ch);
            if (ch_node) {
                ch_node->layout.offset.x += pad.left;
                ch_node->layout.offset.y += pad.top;
            }
        }
        float w = (data->width >= 0.0f) ? data->width : (inner_size.width + pad_w);
        float h = (data->height >= 0.0f) ? data->height : (inner_size.height + pad_h);
        return effective_constraints.constrain({w, h});
    }

    float max_w = 0.0f;
    float max_h = 0.0f;
    layout::BoxConstraints child_constraints = inner_constraints.loosen();
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
        ch_node->layout.offset = {pad.left, pad.top};
        max_w = (std::max)(max_w, ch_size.width);
        max_h = (std::max)(max_h, ch_size.height);
    }

    float w = (data->width >= 0.0f) ? data->width : (max_w + pad_w);
    float h = (data->height >= 0.0f) ? data->height : (max_h + pad_h);
    return effective_constraints.constrain({w, h});
}

void box_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                  void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    if (!dl)
        return;
    auto* node = tree.get(handle);
    if (!node)
        return;
    auto* data = reinterpret_cast<BoxData*>(user_data);
    if (!data)
        return;

    core::Rect bounds{origin.x, origin.y, node->layout.size.width, node->layout.size.height};
    const auto& s = data->style;

    if (s.shadow_color.a > 0.0f && s.shadow_radius > 0.0f) {
        dl->push_shadow(bounds, s.corner_radii, s.shadow_color, s.shadow_radius, s.shadow_spread,
                        s.shadow_offset);
    }

    core::Color bg = data->animated_bg.get();
    core::Color border = data->animated_border.get();

    if (bg.a > 0.0f ||
        (border.a > 0.0f && (s.border_width.top > 0.0f || s.border_width.right > 0.0f ||
                             s.border_width.bottom > 0.0f || s.border_width.left > 0.0f))) {
        dl->push_quad(bounds, s.corner_radii, bg, border, s.border_width);
    }
}

void box_destroy_fn(void* user_data) {
    delete reinterpret_cast<BoxData*>(user_data);
}

} // namespace

void BoxData::apply_theme(const style::Theme& theme) {
    if (on_theme) {
        on_theme(theme, *this);
    }
    float duration = theme.transition_duration();
    if (duration <= 0.0f) {
        animated_bg.set_immediate(style.background);
        animated_border.set_immediate(style.border_color);
    } else {
        animated_bg.tween_to(style.background, duration, animation::CubicBezier::ease_out());
        animated_border.tween_to(style.border_color, duration, animation::CubicBezier::ease_out());
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

BoxBuilder::BoxBuilder(core::NodeTree& tree) : tree_(tree) {}

BoxBuilder& BoxBuilder::style(const style::BoxStyle& style) {
    data_.style = style;
    return *this;
}

BoxBuilder& BoxBuilder::background(core::Color bg) {
    data_.style.background = bg;
    return *this;
}

BoxBuilder& BoxBuilder::border(core::Color color, core::Edges width) {
    data_.style.border_color = color;
    data_.style.border_width = width;
    return *this;
}

BoxBuilder& BoxBuilder::corner_radii(core::CornerRadii radii) {
    data_.style.corner_radii = radii;
    return *this;
}

BoxBuilder& BoxBuilder::shadow(core::Color color, float radius, float spread, core::Point offset) {
    data_.style.shadow_color = color;
    data_.style.shadow_radius = radius;
    data_.style.shadow_spread = spread;
    data_.style.shadow_offset = offset;
    return *this;
}

BoxBuilder& BoxBuilder::padding(core::Edges padding) {
    data_.padding = padding;
    return *this;
}

BoxBuilder& BoxBuilder::margin(core::Edges margin) {
    data_.margin = margin;
    return *this;
}

BoxBuilder& BoxBuilder::width(float w) {
    data_.width = w;
    return *this;
}

BoxBuilder& BoxBuilder::height(float h) {
    data_.height = h;
    return *this;
}

BoxBuilder& BoxBuilder::size(core::Size sz) {
    data_.width = sz.width;
    data_.height = sz.height;
    return *this;
}

BoxBuilder& BoxBuilder::theme_signal(core::Signal<style::Theme>* sig, BoxThemeFn on_theme) {
    data_.theme_signal = sig;
    data_.on_theme = on_theme;
    if (sig && on_theme) {
        on_theme(sig->get(), data_);
    }
    return *this;
}

BoxBuilder& BoxBuilder::flex_grow(float grow) {
    data_.flex_grow = grow;
    return *this;
}

BoxBuilder& BoxBuilder::flex_shrink(float shrink) {
    data_.flex_shrink = shrink;
    return *this;
}

BoxBuilder& BoxBuilder::flex_basis(float basis) {
    data_.flex_basis = basis;
    return *this;
}

BoxBuilder& BoxBuilder::flex(const layout::FlexStyle& style) {
    data_.flex_style = style;
    data_.is_flex = true;
    return *this;
}

BoxBuilder& BoxBuilder::flex(layout::FlexDirection dir, layout::MainAxisAlignment main_align,
                             layout::CrossAxisAlignment cross_align, float gap) {
    data_.flex_style = layout::FlexStyle{
        .direction = dir, .main_align = main_align, .cross_align = cross_align, .gap = gap};
    data_.is_flex = true;
    return *this;
}

BoxBuilder& BoxBuilder::child(core::Handle<core::Node> child) {
    if (child.is_valid()) {
        children_.push_back(child);
    }
    return *this;
}

BoxBuilder& BoxBuilder::children(std::span<const core::Handle<core::Node>> children) {
    for (auto c : children) {
        if (c.is_valid()) {
            children_.push_back(c);
        }
    }
    return *this;
}

core::Handle<core::Node> BoxBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node) {
        return {};
    }

    auto* heap_data = new BoxData(data_);
    heap_data->tree = &tree_;
    heap_data->node_handle = handle;

    heap_data->animated_bg.set_immediate(heap_data->style.background);
    heap_data->animated_border.set_immediate(heap_data->style.border_color);

    heap_data->animated_bg.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* box = static_cast<BoxData*>(ctx);
            if (box->tree && box->node_handle.is_valid()) {
                box->tree->mark_dirty(box->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    heap_data->animated_border.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* box = static_cast<BoxData*>(ctx);
            if (box->tree && box->node_handle.is_valid()) {
                box->tree->mark_dirty(box->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    if (heap_data->theme_signal) {
        heap_data->theme_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* box = static_cast<BoxData*>(ctx);
                if (box && box->theme_signal) {
                    box->apply_theme(box->theme_signal->get());
                }
            },
            heap_data, core::kPaint);
    }

    node->user_data = heap_data;
    node->layout_fn = &box_layout_fn;
    node->paint_fn = &box_paint_fn;
    node->destroy_fn = &box_destroy_fn;

    node->flex_item.grow = data_.flex_grow;
    node->flex_item.shrink = data_.flex_shrink;
    node->flex_item.basis = data_.flex_basis;

    for (auto ch : children_) {
        tree_.append_child(handle, ch);
    }

    return handle;
}

} // namespace north::widget
