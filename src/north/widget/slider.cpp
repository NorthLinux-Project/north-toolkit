#include "north/widget/slider.hpp"

#include "north/render/display_list.hpp"

#include <algorithm>
#include <cmath>

namespace north::widget {

void SliderData::set_value(float new_val) {
    new_val = std::clamp(new_val, min_value, max_value);
    if (step > 0.0f) {
        float steps = std::round((new_val - min_value) / step);
        new_val = min_value + steps * step;
        new_val = std::clamp(new_val, min_value, max_value);
    }
    if (new_val != value) {
        value = new_val;
        if (on_change) {
            on_change(value, change_user_data);
        }
        if (tree && node_handle.is_valid()) {
            tree->mark_dirty(node_handle, core::kPaint);
        }
        auto* sig = bound_signal;
        float v = value;
        if (sig) {
            sig->set(v);
        } else {
            internal_signal.set(v);
        }
    }
}

void SliderData::seek_from_pointer(float local_x) {
    float r = 8.0f;
    float w = width;
    float travel = w - 2.0f * r;
    if (travel > 0.0f) {
        float t = (local_x - r) / travel;
        t = std::clamp(t, 0.0f, 1.0f);
        float new_val = min_value + t * (max_value - min_value);
        set_value(new_val);
    }
}

void SliderData::apply_theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    track_inactive_color = c.surface_variant;
    track_active_color = c.primary;
    thumb_color = core::Color{1.0f, 1.0f, 1.0f, 1.0f};
    focus_ring_color = c.primary;
    float duration = theme.transition_duration();
    if (duration <= 0.0f) {
        animated_track_inactive.set_immediate(track_inactive_color);
        animated_track_active.set_immediate(track_active_color);
        animated_thumb.set_immediate(thumb_color);
    } else {
        animated_track_inactive.tween_to(track_inactive_color, duration, animation::CubicBezier::ease_out());
        animated_track_active.tween_to(track_active_color, duration, animation::CubicBezier::ease_out());
        animated_thumb.tween_to(thumb_color, duration, animation::CubicBezier::ease_out());
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

namespace {

core::Size slider_layout_fn(core::NodeTree&, core::Handle<core::Node>,
                            const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<SliderData*>(user_data);
    if (!data) {
        return constraints.constrain({0.0f, 0.0f});
    }

    float resolved_w = (data->width > 0.0f) ? data->width : constraints.max_width;
    core::Size sz = constraints.constrain({resolved_w, 24.0f});
    data->width = sz.width;
    data->height = sz.height;
    return sz;
}

void slider_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                     void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    if (!dl)
        return;
    auto* data = reinterpret_cast<SliderData*>(user_data);
    if (!data)
        return;

    auto* node = tree.get(handle);
    if (!node)
        return;

    float w = node->layout.size.width;
    float h = node->layout.size.height;
    float track_h = 4.0f;
    float track_y = origin.y + (h - track_h) * 0.5f;

    float range_span = (std::max)(1e-4f, data->max_value - data->min_value);
    float t = std::clamp((data->value - data->min_value) / range_span, 0.0f, 1.0f);

    float thumb_r = data->thumb_radius.get();
    float travel = w - 16.0f;
    float thumb_center_x = origin.x + 8.0f + t * travel;

    // Inactive track
    dl->push_quad(core::Rect{origin.x, track_y, w, track_h}, core::CornerRadii::uniform(2.0f),
                  data->animated_track_inactive.get());

    // Active track fill
    float active_w = (std::max)(0.0f, thumb_center_x - origin.x);
    if (active_w > 0.0f) {
        dl->push_quad(core::Rect{origin.x, track_y, active_w, track_h},
                      core::CornerRadii::uniform(2.0f), data->animated_track_active.get());
    }

    // Focus ring around thumb
    if (data->is_focused && data->focus_ring_color.a > 0.0f) {
        float f_r = thumb_r + 3.0f;
        dl->push_quad(
            core::Rect{thumb_center_x - f_r, origin.y + h * 0.5f - f_r, f_r * 2.0f, f_r * 2.0f},
            core::CornerRadii::uniform(f_r), core::Color{0.0f, 0.0f, 0.0f, 0.0f},
            data->focus_ring_color, core::Edges::uniform(2.0f));
    }

    // Thumb shadow
    dl->push_shadow(core::Rect{thumb_center_x - thumb_r, origin.y + h * 0.5f - thumb_r,
                               thumb_r * 2.0f, thumb_r * 2.0f},
                    core::CornerRadii::uniform(thumb_r), core::Color{0.0f, 0.0f, 0.0f, 0.25f}, 4.0f,
                    0.0f, core::Point{0.0f, 1.5f});

    // Thumb quad
    dl->push_quad(core::Rect{thumb_center_x - thumb_r, origin.y + h * 0.5f - thumb_r,
                             thumb_r * 2.0f, thumb_r * 2.0f},
                  core::CornerRadii::uniform(thumb_r), data->animated_thumb.get());
}

bool slider_hit_test_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                        core::Point local_point, void*) {
    auto* node = tree.get(handle);
    if (!node)
        return false;
    core::Rect local_bounds{0.0f, 0.0f, node->layout.size.width, node->layout.size.height};
    return local_bounds.contains(local_point);
}

void slider_destroy_fn(void* user_data) {
    delete reinterpret_cast<SliderData*>(user_data);
}

} // namespace

SliderBuilder::SliderBuilder(core::NodeTree& tree) : tree_(tree) {
    theme(style::Theme::dark());
}

SliderBuilder& SliderBuilder::value(float val) {
    data_.value = val;
    return *this;
}

SliderBuilder& SliderBuilder::range(float min_val, float max_val) {
    data_.min_value = min_val;
    data_.max_value = max_val;
    return *this;
}

SliderBuilder& SliderBuilder::step(float step_size) {
    data_.step = step_size;
    return *this;
}

SliderBuilder& SliderBuilder::bind(core::Signal<float>& signal) {
    data_.bound_signal = &signal;
    data_.value = signal.get();
    return *this;
}

SliderBuilder& SliderBuilder::width(float w) {
    data_.width = w;
    return *this;
}

SliderBuilder& SliderBuilder::theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    data_.track_inactive_color = c.surface_variant;
    data_.track_active_color = c.primary;
    data_.thumb_color = core::Color{1.0f, 1.0f, 1.0f, 1.0f};
    data_.focus_ring_color = c.primary;
    return *this;
}

SliderBuilder& SliderBuilder::theme_signal(core::Signal<style::Theme>* sig) {
    data_.theme_signal = sig;
    if (sig) {
        theme(sig->get());
    }
    return *this;
}

SliderBuilder& SliderBuilder::on_change(void (*fn)(float, void*), void* user_data) {
    data_.on_change = fn;
    data_.change_user_data = user_data;
    return *this;
}

core::Handle<core::Node> SliderBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node)
        return {};

    auto* heap_data = new SliderData(std::move(data_));
    heap_data->node_handle = handle;
    heap_data->tree = &tree_;
    heap_data->thumb_radius.set_immediate(8.0f);
    heap_data->animated_track_inactive.set_immediate(heap_data->track_inactive_color);
    heap_data->animated_track_active.set_immediate(heap_data->track_active_color);
    heap_data->animated_thumb.set_immediate(heap_data->thumb_color);

    heap_data->thumb_radius.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* sl = static_cast<SliderData*>(ctx);
            if (sl->tree && sl->node_handle.is_valid()) {
                sl->tree->mark_dirty(sl->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    heap_data->animated_track_inactive.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* sl = static_cast<SliderData*>(ctx);
            if (sl->tree && sl->node_handle.is_valid()) {
                sl->tree->mark_dirty(sl->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    heap_data->animated_track_active.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* sl = static_cast<SliderData*>(ctx);
            if (sl->tree && sl->node_handle.is_valid()) {
                sl->tree->mark_dirty(sl->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    heap_data->animated_thumb.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* sl = static_cast<SliderData*>(ctx);
            if (sl->tree && sl->node_handle.is_valid()) {
                sl->tree->mark_dirty(sl->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    if (heap_data->bound_signal) {
        heap_data->bound_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* sl = static_cast<SliderData*>(ctx);
                if (sl->bound_signal && sl->value != sl->bound_signal->get()) {
                    sl->value = sl->bound_signal->get();
                    if (sl->tree && sl->node_handle.is_valid()) {
                        sl->tree->mark_dirty(sl->node_handle, core::kPaint);
                    }
                }
            },
            heap_data);
    }

    if (heap_data->theme_signal) {
        heap_data->theme_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* sl = static_cast<SliderData*>(ctx);
                if (sl && sl->theme_signal) {
                    sl->apply_theme(sl->theme_signal->get());
                }
            },
            heap_data, core::kPaint);
    }

    node->user_data = heap_data;
    node->layout_fn = slider_layout_fn;
    node->paint_fn = slider_paint_fn;
    node->hit_test_fn = slider_hit_test_fn;
    node->destroy_fn = slider_destroy_fn;

    return handle;
}

} // namespace north::widget
