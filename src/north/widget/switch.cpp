#include "north/widget/switch.hpp"

#include "north/render/display_list.hpp"
#include "north/text/font_manager.hpp"

#include <algorithm>

namespace north::widget {

void SwitchData::update_targets() {
    float duration = track_color.is_animating() ? 0.05f : 0.12f;
    if (checked) {
        thumb_position.spring_to(1.0f, animation::SpringDesc::snappy());
        track_color.tween_to(track_on_color, duration, animation::CubicBezier::ease_out());
    } else {
        thumb_position.spring_to(0.0f, animation::SpringDesc::snappy());
        track_color.tween_to(track_off_color, duration, animation::CubicBezier::ease_out());
    }
}

void SwitchData::toggle() {
    checked = !checked;
    update_targets();
    if (on_change) {
        on_change(checked, change_user_data);
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
    auto* sig = bound_signal;
    bool new_state = checked;
    if (sig) {
        sig->set(new_state);
    } else {
        internal_signal.set(new_state);
    }
}

void SwitchData::apply_theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    text_color = c.on_surface;
    track_off_color = c.surface_variant;
    track_on_color = c.primary;
    thumb_color = core::Color{1.0f, 1.0f, 1.0f, 1.0f};
    focus_ring_color = c.primary;
    if (paragraph) {
        paragraph = nullptr;
    }
    float duration = theme.transition_duration();
    if (duration <= 0.0f) {
        track_color.set_immediate(checked ? track_on_color : track_off_color);
    } else {
        update_targets();
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

namespace {

core::Size switch_layout_fn(core::NodeTree&, core::Handle<core::Node>,
                            const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<SwitchData*>(user_data);
    if (!data) {
        return constraints.constrain({0.0f, 0.0f});
    }

    float track_w = 44.0f;
    float track_h = 24.0f;
    float total_w = track_w;
    float total_h = track_h;

    if (!data->paragraph && data->font && !data->label.empty()) {
        text::ParagraphBuilder pb(text::TextAlign::Left);
        text::TextStyle style{
            .font = data->font, .font_size = data->font_size, .color = data->text_color};
        pb.add_text(data->label, style);
        data->paragraph = pb.build();
    }

    if (data->paragraph) {
        layout::BoxConstraints text_constraints{
            0.0f, (std::max)(0.0f, constraints.max_width - track_w - 8.0f), 0.0f,
            constraints.max_height};
        data->paragraph->layout(text_constraints);
        total_w += 8.0f + data->paragraph->size().width;
        total_h = (std::max)(track_h, data->paragraph->size().height);
    }

    return constraints.constrain({total_w, total_h});
}

void switch_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                     void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    if (!dl)
        return;
    auto* data = reinterpret_cast<SwitchData*>(user_data);
    if (!data)
        return;

    auto* node = tree.get(handle);
    if (!node)
        return;

    float track_w = 44.0f;
    float track_h = 24.0f;
    float track_x = origin.x;
    float track_y = origin.y + (node->layout.size.height - track_h) * 0.5f;
    core::Rect track_rect{track_x, track_y, track_w, track_h};

    // Focus ring
    if (data->is_focused && data->focus_ring_color.a > 0.0f) {
        core::Rect focus_rect{track_x - 3.0f, track_y - 3.0f, track_w + 6.0f, track_h + 6.0f};
        dl->push_quad(focus_rect, core::CornerRadii::uniform(15.0f),
                      core::Color{0.0f, 0.0f, 0.0f, 0.0f}, data->focus_ring_color,
                      core::Edges::uniform(2.0f));
    }

    // Pill Track
    core::Color active_track = data->track_color.get();
    dl->push_quad(track_rect, core::CornerRadii::uniform(12.0f), active_track);

    // Sliding Thumb
    float p = std::clamp(data->thumb_position.get(), 0.0f, 1.0f);
    float thumb_dim = 18.0f;
    float thumb_x = track_x + 3.0f + 20.0f * p;
    float thumb_y = track_y + 3.0f;
    core::Rect thumb_rect{thumb_x, thumb_y, thumb_dim, thumb_dim};

    dl->push_shadow(thumb_rect, core::CornerRadii::uniform(9.0f),
                    core::Color{0.0f, 0.0f, 0.0f, 0.25f}, 3.0f, 0.0f, core::Point{0.0f, 1.0f});

    dl->push_quad(thumb_rect, core::CornerRadii::uniform(9.0f), data->thumb_color);

    // Label
    if (data->paragraph && dl->glyph_atlas()) {
        float text_x = track_x + track_w + 8.0f;
        float text_y =
            origin.y + (node->layout.size.height - data->paragraph->size().height) * 0.5f;
        data->paragraph->paint(*dl, *dl->glyph_atlas(), {text_x, text_y});
    }
}

bool switch_hit_test_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                        core::Point local_point, void*) {
    auto* node = tree.get(handle);
    if (!node)
        return false;
    core::Rect local_bounds{0.0f, 0.0f, node->layout.size.width, node->layout.size.height};
    return local_bounds.contains(local_point);
}

void switch_destroy_fn(void* user_data) {
    delete reinterpret_cast<SwitchData*>(user_data);
}

} // namespace

SwitchBuilder::SwitchBuilder(core::NodeTree& tree) : tree_(tree) {
    theme(style::Theme::dark());
}

SwitchBuilder& SwitchBuilder::checked(bool is_checked) {
    data_.checked = is_checked;
    return *this;
}

SwitchBuilder& SwitchBuilder::bind(core::Signal<bool>& signal) {
    data_.bound_signal = &signal;
    data_.checked = signal.get();
    return *this;
}

SwitchBuilder& SwitchBuilder::label(std::string_view text) {
    data_.label = std::string(text);
    return *this;
}

SwitchBuilder& SwitchBuilder::font(std::shared_ptr<text::Font> font) {
    data_.font = std::move(font);
    return *this;
}

SwitchBuilder& SwitchBuilder::font_size(float size) {
    data_.font_size = size;
    return *this;
}

SwitchBuilder& SwitchBuilder::theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    data_.text_color = c.on_surface;
    data_.track_off_color = c.surface_variant;
    data_.track_on_color = c.primary;
    data_.thumb_color = core::Color{1.0f, 1.0f, 1.0f, 1.0f};
    data_.focus_ring_color = c.primary;
    return *this;
}

SwitchBuilder& SwitchBuilder::theme_signal(core::Signal<style::Theme>* sig) {
    data_.theme_signal = sig;
    if (sig) {
        theme(sig->get());
    }
    return *this;
}

SwitchBuilder& SwitchBuilder::on_change(void (*fn)(bool, void*), void* user_data) {
    data_.on_change = fn;
    data_.change_user_data = user_data;
    return *this;
}

core::Handle<core::Node> SwitchBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node)
        return {};

    if (!data_.font) {
        data_.font = text::FontManager::instance().load_system_font();
    }

    auto* heap_data = new SwitchData(std::move(data_));
    heap_data->node_handle = handle;
    heap_data->tree = &tree_;

    float initial_p = heap_data->checked ? 1.0f : 0.0f;
    heap_data->thumb_position.set_immediate(initial_p);

    core::Color initial_track =
        heap_data->checked ? heap_data->track_on_color : heap_data->track_off_color;
    heap_data->track_color.set_immediate(initial_track);

    heap_data->thumb_position.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* sw = static_cast<SwitchData*>(ctx);
            if (sw->tree && sw->node_handle.is_valid()) {
                sw->tree->mark_dirty(sw->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    heap_data->track_color.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* sw = static_cast<SwitchData*>(ctx);
            if (sw->tree && sw->node_handle.is_valid()) {
                sw->tree->mark_dirty(sw->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    if (heap_data->bound_signal) {
        heap_data->bound_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* sw = static_cast<SwitchData*>(ctx);
                if (sw->bound_signal && sw->checked != sw->bound_signal->get()) {
                    sw->checked = sw->bound_signal->get();
                    sw->update_targets();
                    if (sw->tree && sw->node_handle.is_valid()) {
                        sw->tree->mark_dirty(sw->node_handle, core::kPaint);
                    }
                }
            },
            heap_data);
    }

    if (heap_data->theme_signal) {
        heap_data->theme_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* sw = static_cast<SwitchData*>(ctx);
                if (sw && sw->theme_signal) {
                    sw->apply_theme(sw->theme_signal->get());
                }
            },
            heap_data, core::kPaint);
    }

    node->user_data = heap_data;
    node->layout_fn = switch_layout_fn;
    node->paint_fn = switch_paint_fn;
    node->hit_test_fn = switch_hit_test_fn;
    node->destroy_fn = switch_destroy_fn;

    return handle;
}

} // namespace north::widget
