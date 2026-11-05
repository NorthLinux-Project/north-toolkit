#include "north/widget/icon.hpp"

#include "north/render/display_list.hpp"

namespace north::widget {

namespace {

core::Size icon_layout_fn(core::NodeTree&, core::Handle<core::Node>,
                          const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<IconData*>(user_data);
    if (!data) {
        return constraints.constrain(core::Size{24.0f, 24.0f});
    }
    return constraints.constrain(data->size);
}

void icon_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                   void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    auto* data = reinterpret_cast<IconData*>(user_data);
    auto* node = tree.get(handle);
    if (!dl || !data || !node) {
        return;
    }

    core::Rect b = data->path.bounds();
    core::Size render_sz = node->layout.size;
    if (render_sz.width <= 0.0f || render_sz.height <= 0.0f) {
        render_sz = data->size;
    }

    float sx = (b.width > 1e-4f) ? (render_sz.width / b.width) : 1.0f;
    float sy = (b.height > 1e-4f) ? (render_sz.height / b.height) : 1.0f;

    render::Path p = data->path;
    p.translate(core::Point{-b.x, -b.y});
    p.scale(sx, sy);
    p.translate(origin);

    core::Color c = data->animated_color.get();
    if (data->is_stroke) {
        dl->push_path_stroke(p, c, data->stroke_style);
    } else {
        dl->push_path_fill(p, c);
    }
}

void icon_destroy_fn(void* user_data) {
    delete reinterpret_cast<IconData*>(user_data);
}

} // namespace

void IconData::apply_theme(const style::Theme& theme) {
    if (on_theme) {
        on_theme(theme, *this);
    }
    float duration = theme.transition_duration();
    if (duration <= 0.0f) {
        animated_color.set_immediate(color);
    } else {
        animated_color.tween_to(color, duration, animation::CubicBezier::ease_out());
    }
}

IconBuilder::IconBuilder(core::NodeTree& tree, const render::Path& path) : tree_(tree) {
    data_.path = path;
}

IconBuilder::IconBuilder(core::NodeTree& tree, std::string_view svg_path_d) : tree_(tree) {
    (void)render::parse_svg_path(svg_path_d, data_.path);
}

IconBuilder& IconBuilder::size(float width, float height) {
    data_.size = core::Size{width, height};
    return *this;
}

IconBuilder& IconBuilder::size(core::Size sz) {
    data_.size = sz;
    return *this;
}

IconBuilder& IconBuilder::color(core::Color c) {
    data_.color = c;
    return *this;
}

IconBuilder& IconBuilder::stroke(const render::StrokeStyle& style) {
    data_.is_stroke = true;
    data_.stroke_style = style;
    return *this;
}

IconBuilder& IconBuilder::theme_signal(core::Signal<style::Theme>* sig, IconThemeFn fn) {
    data_.theme_signal = sig;
    data_.on_theme = fn;
    return *this;
}

core::Handle<core::Node> IconBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node) {
        return {};
    }

    auto* heap_data = new IconData(data_);
    heap_data->tree = &tree_;
    heap_data->node_handle = handle;

    heap_data->animated_color.set_immediate(heap_data->color);
    heap_data->animated_color.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* icon = static_cast<IconData*>(ctx);
            if (icon->tree && icon->node_handle.is_valid()) {
                icon->tree->mark_dirty(icon->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    if (heap_data->theme_signal) {
        heap_data->theme_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* icon = static_cast<IconData*>(ctx);
                if (icon && icon->theme_signal) {
                    icon->apply_theme(icon->theme_signal->get());
                }
            },
            heap_data, core::kPaint);
    }

    node->user_data = heap_data;
    node->layout_fn = &icon_layout_fn;
    node->paint_fn = &icon_paint_fn;
    node->destroy_fn = &icon_destroy_fn;

    return handle;
}

} // namespace north::widget
