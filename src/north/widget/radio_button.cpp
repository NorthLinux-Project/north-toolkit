#include "north/widget/radio_button.hpp"

#include "north/render/display_list.hpp"
#include "north/text/font_manager.hpp"

#include <algorithm>

namespace north::widget {

void RadioButtonData::update_targets() {
    if (selected) {
        dot_scale.spring_to(1.0f, animation::SpringDesc::snappy());
        ring_color.tween_to(ring_active_color, 0.12f, animation::CubicBezier::ease_out());
    } else {
        dot_scale.spring_to(0.0f, animation::SpringDesc::snappy());
        ring_color.tween_to(ring_inactive_color, 0.12f, animation::CubicBezier::ease_out());
    }
}

void RadioButtonData::select() {
    selected = true;
    update_targets();
    if (on_select) {
        on_select(value, select_user_data);
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
    auto* grp = group_signal;
    auto* bnd = bound_signal;
    int val = value;
    if (bnd) {
        bnd->set(true);
    } else {
        internal_signal.set(true);
    }
    if (grp) {
        grp->set(val);
    }
}

namespace {

core::Size radio_layout_fn(core::NodeTree&, core::Handle<core::Node>,
                           const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<RadioButtonData*>(user_data);
    if (!data) {
        return constraints.constrain({0.0f, 0.0f});
    }

    float ring_dim = 18.0f;
    float total_w = ring_dim;
    float total_h = ring_dim;

    if (!data->paragraph && data->font && !data->label.empty()) {
        text::ParagraphBuilder pb(text::TextAlign::Left);
        text::TextStyle style{
            .font = data->font, .font_size = data->font_size, .color = data->text_color};
        pb.add_text(data->label, style);
        data->paragraph = pb.build();
    }

    if (data->paragraph) {
        layout::BoxConstraints text_constraints{
            0.0f, (std::max)(0.0f, constraints.max_width - ring_dim - 8.0f), 0.0f,
            constraints.max_height};
        data->paragraph->layout(text_constraints);
        total_w += 8.0f + data->paragraph->size().width;
        total_h = (std::max)(ring_dim, data->paragraph->size().height);
    }

    return constraints.constrain({total_w, total_h});
}

void radio_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                    void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    if (!dl)
        return;
    auto* data = reinterpret_cast<RadioButtonData*>(user_data);
    if (!data)
        return;

    auto* node = tree.get(handle);
    if (!node)
        return;

    float ring_dim = 18.0f;
    float ring_x = origin.x;
    float ring_y = origin.y + (node->layout.size.height - ring_dim) * 0.5f;
    core::Rect ring_rect{ring_x, ring_y, ring_dim, ring_dim};

    // Focus ring
    if (data->is_focused && data->focus_ring_color.a > 0.0f) {
        core::Rect focus_rect{ring_x - 3.0f, ring_y - 3.0f, ring_dim + 6.0f, ring_dim + 6.0f};
        dl->push_quad(focus_rect, core::CornerRadii::uniform(12.0f),
                      core::Color{0.0f, 0.0f, 0.0f, 0.0f}, data->focus_ring_color,
                      core::Edges::uniform(2.0f));
    }

    // Outer Ring
    core::Color current_ring = data->ring_color.get();
    dl->push_quad(ring_rect, core::CornerRadii::uniform(9.0f), core::Color{0.0f, 0.0f, 0.0f, 0.0f},
                  current_ring, core::Edges::uniform(1.5f));

    // Inner Dot (Analytical circular quad scaled by dot_scale)
    float s = data->dot_scale.get();
    if (s > 0.01f) {
        float r = 4.5f * s;
        float cx = ring_x + 9.0f;
        float cy = ring_y + 9.0f;
        core::Color dot_col = data->dot_color;
        dot_col.a *= std::clamp(s, 0.0f, 1.0f);

        dl->push_quad(core::Rect{cx - r, cy - r, r * 2.0f, r * 2.0f}, core::CornerRadii::uniform(r),
                      dot_col);
    }

    // Label
    if (data->paragraph && dl->glyph_atlas()) {
        float text_x = ring_x + ring_dim + 8.0f;
        float text_y =
            origin.y + (node->layout.size.height - data->paragraph->size().height) * 0.5f;
        data->paragraph->paint(*dl, *dl->glyph_atlas(), {text_x, text_y});
    }
}

bool radio_hit_test_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                       core::Point local_point, void*) {
    auto* node = tree.get(handle);
    if (!node)
        return false;
    core::Rect local_bounds{0.0f, 0.0f, node->layout.size.width, node->layout.size.height};
    return local_bounds.contains(local_point);
}

void radio_destroy_fn(void* user_data) {
    delete reinterpret_cast<RadioButtonData*>(user_data);
}

} // namespace

RadioButtonBuilder::RadioButtonBuilder(core::NodeTree& tree) : tree_(tree) {
    theme(style::Theme::dark());
}

RadioButtonBuilder& RadioButtonBuilder::selected(bool is_selected) {
    data_.selected = is_selected;
    return *this;
}

RadioButtonBuilder& RadioButtonBuilder::value(int val) {
    data_.value = val;
    return *this;
}

RadioButtonBuilder& RadioButtonBuilder::group(core::Signal<int>& signal) {
    data_.group_signal = &signal;
    data_.selected = (signal.get() == data_.value);
    return *this;
}

RadioButtonBuilder& RadioButtonBuilder::bind(core::Signal<bool>& signal) {
    data_.bound_signal = &signal;
    data_.selected = signal.get();
    return *this;
}

RadioButtonBuilder& RadioButtonBuilder::label(std::string_view text) {
    data_.label = std::string(text);
    return *this;
}

RadioButtonBuilder& RadioButtonBuilder::font(std::shared_ptr<text::Font> font) {
    data_.font = std::move(font);
    return *this;
}

RadioButtonBuilder& RadioButtonBuilder::font_size(float size) {
    data_.font_size = size;
    return *this;
}

RadioButtonBuilder& RadioButtonBuilder::theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    data_.text_color = c.on_surface;
    data_.ring_inactive_color = c.outline;
    data_.ring_active_color = c.primary;
    data_.dot_color = c.primary;
    data_.focus_ring_color = c.primary;
    return *this;
}

RadioButtonBuilder& RadioButtonBuilder::on_select(void (*fn)(int, void*), void* user_data) {
    data_.on_select = fn;
    data_.select_user_data = user_data;
    return *this;
}

core::Handle<core::Node> RadioButtonBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node)
        return {};

    if (!data_.font) {
        data_.font = text::FontManager::instance().load_system_font();
    }

    auto* heap_data = new RadioButtonData(std::move(data_));
    heap_data->node_handle = handle;
    heap_data->tree = &tree_;

    float initial_scale = heap_data->selected ? 1.0f : 0.0f;
    heap_data->dot_scale.set_immediate(initial_scale);

    core::Color initial_ring =
        heap_data->selected ? heap_data->ring_active_color : heap_data->ring_inactive_color;
    heap_data->ring_color.set_immediate(initial_ring);

    heap_data->dot_scale.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* rb = static_cast<RadioButtonData*>(ctx);
            if (rb->tree && rb->node_handle.is_valid()) {
                rb->tree->mark_dirty(rb->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    heap_data->ring_color.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* rb = static_cast<RadioButtonData*>(ctx);
            if (rb->tree && rb->node_handle.is_valid()) {
                rb->tree->mark_dirty(rb->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    if (heap_data->group_signal) {
        heap_data->group_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* rb = static_cast<RadioButtonData*>(ctx);
                if (rb->group_signal) {
                    bool should_be_sel = (rb->group_signal->get() == rb->value);
                    if (rb->selected != should_be_sel) {
                        rb->selected = should_be_sel;
                        rb->update_targets();
                        if (rb->tree && rb->node_handle.is_valid()) {
                            rb->tree->mark_dirty(rb->node_handle, core::kPaint);
                        }
                    }
                }
            },
            heap_data);
    }

    if (heap_data->bound_signal) {
        heap_data->bound_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* rb = static_cast<RadioButtonData*>(ctx);
                if (rb->bound_signal && rb->selected != rb->bound_signal->get()) {
                    rb->selected = rb->bound_signal->get();
                    rb->update_targets();
                    if (rb->tree && rb->node_handle.is_valid()) {
                        rb->tree->mark_dirty(rb->node_handle, core::kPaint);
                    }
                }
            },
            heap_data);
    }

    node->user_data = heap_data;
    node->layout_fn = radio_layout_fn;
    node->paint_fn = radio_paint_fn;
    node->hit_test_fn = radio_hit_test_fn;
    node->destroy_fn = radio_destroy_fn;

    return handle;
}

} // namespace north::widget
