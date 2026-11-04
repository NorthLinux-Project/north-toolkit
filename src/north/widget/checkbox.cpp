#include "north/widget/checkbox.hpp"

#include "north/render/display_list.hpp"
#include "north/text/font_manager.hpp"

#include <algorithm>

namespace north::widget {

void CheckboxData::update_targets() {
    if (checked) {
        check_progress.spring_to(1.0f, animation::SpringDesc::snappy());
        animated_bg.tween_to(box_color, 0.12f, animation::CubicBezier::ease_out());
        animated_border.tween_to(box_color, 0.12f, animation::CubicBezier::ease_out());
    } else {
        check_progress.spring_to(0.0f, animation::SpringDesc::snappy());
        animated_bg.tween_to(core::Color{0.0f, 0.0f, 0.0f, 0.0f}, 0.12f,
                             animation::CubicBezier::ease_out());
        animated_border.tween_to(box_color, 0.12f, animation::CubicBezier::ease_out());
    }
}

void CheckboxData::toggle() {
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

namespace {

core::Size checkbox_layout_fn(core::NodeTree&, core::Handle<core::Node>,
                              const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<CheckboxData*>(user_data);
    if (!data) {
        return constraints.constrain({0.0f, 0.0f});
    }

    float box_dim = 18.0f;
    float total_w = box_dim;
    float total_h = box_dim;

    if (!data->paragraph && data->font && !data->label.empty()) {
        text::ParagraphBuilder pb(text::TextAlign::Left);
        text::TextStyle style{
            .font = data->font, .font_size = data->font_size, .color = data->text_color};
        pb.add_text(data->label, style);
        data->paragraph = pb.build();
    }

    if (data->paragraph) {
        layout::BoxConstraints text_constraints{
            0.0f, (std::max)(0.0f, constraints.max_width - box_dim - 8.0f), 0.0f,
            constraints.max_height};
        data->paragraph->layout(text_constraints);
        total_w += 8.0f + data->paragraph->size().width;
        total_h = (std::max)(box_dim, data->paragraph->size().height);
    }

    return constraints.constrain({total_w, total_h});
}

void checkbox_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                       void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    if (!dl)
        return;
    auto* data = reinterpret_cast<CheckboxData*>(user_data);
    if (!data)
        return;

    auto* node = tree.get(handle);
    if (!node)
        return;

    float box_dim = 18.0f;
    float box_x = origin.x;
    float box_y = origin.y + (node->layout.size.height - box_dim) * 0.5f;
    core::Rect box_rect{box_x, box_y, box_dim, box_dim};

    // Focus ring
    if (data->is_focused && data->focus_ring_color.a > 0.0f) {
        core::Rect focus_rect{box_x - 3.0f, box_y - 3.0f, box_dim + 6.0f, box_dim + 6.0f};
        dl->push_quad(focus_rect, core::CornerRadii::uniform(6.0f),
                      core::Color{0.0f, 0.0f, 0.0f, 0.0f}, data->focus_ring_color,
                      core::Edges::uniform(2.0f));
    }

    // Checkbox container
    core::Color bg = data->animated_bg.get();
    core::Color border = data->animated_border.get();
    dl->push_quad(box_rect, core::CornerRadii::uniform(4.0f), bg, border,
                  core::Edges::uniform(1.5f));

    // Analytical checkmark lines
    float p = data->check_progress.get();
    if (p > 0.01f) {
        core::Color mark_col = data->check_color;
        mark_col.a *= std::clamp(p, 0.0f, 1.0f);

        float cx = box_x + 9.0f;
        float cy = box_y + 9.0f;

        struct StrokePoint {
            float rel_x;
            float rel_y;
            float w;
            float h;
        };

        static constexpr StrokePoint kCheckStrokes[] = {
            {-5.0f, 0.0f, 2.2f, 2.5f}, {-3.5f, 1.5f, 2.2f, 2.5f}, {-2.0f, 3.0f, 2.2f, 2.5f},
            {-0.5f, 1.5f, 2.2f, 2.5f}, {1.0f, 0.0f, 2.2f, 2.5f},  {2.5f, -1.5f, 2.2f, 2.5f},
            {4.0f, -3.0f, 2.2f, 2.5f}};

        for (const auto& sp : kCheckStrokes) {
            float px = cx + sp.rel_x * p;
            float py = cy + sp.rel_y * p;
            dl->push_quad(core::Rect{px, py, sp.w * p, sp.h * p}, core::CornerRadii::uniform(1.0f),
                          mark_col);
        }
    }

    // Label
    if (data->paragraph && dl->glyph_atlas()) {
        float text_x = box_x + box_dim + 8.0f;
        float text_y =
            origin.y + (node->layout.size.height - data->paragraph->size().height) * 0.5f;
        data->paragraph->paint(*dl, *dl->glyph_atlas(), {text_x, text_y});
    }
}

bool checkbox_hit_test_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                          core::Point local_point, void*) {
    auto* node = tree.get(handle);
    if (!node)
        return false;
    core::Rect local_bounds{0.0f, 0.0f, node->layout.size.width, node->layout.size.height};
    return local_bounds.contains(local_point);
}

void checkbox_destroy_fn(void* user_data) {
    delete reinterpret_cast<CheckboxData*>(user_data);
}

} // namespace

CheckboxBuilder::CheckboxBuilder(core::NodeTree& tree) : tree_(tree) {
    theme(style::Theme::dark());
}

CheckboxBuilder& CheckboxBuilder::checked(bool is_checked) {
    data_.checked = is_checked;
    return *this;
}

CheckboxBuilder& CheckboxBuilder::bind(core::Signal<bool>& signal) {
    data_.bound_signal = &signal;
    data_.checked = signal.get();
    return *this;
}

CheckboxBuilder& CheckboxBuilder::label(std::string_view text) {
    data_.label = std::string(text);
    return *this;
}

CheckboxBuilder& CheckboxBuilder::font(std::shared_ptr<text::Font> font) {
    data_.font = std::move(font);
    return *this;
}

CheckboxBuilder& CheckboxBuilder::font_size(float size) {
    data_.font_size = size;
    return *this;
}

CheckboxBuilder& CheckboxBuilder::theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    data_.text_color = c.on_surface;
    data_.box_color = c.primary;
    data_.check_color = c.on_primary;
    data_.focus_ring_color = c.primary;
    return *this;
}

CheckboxBuilder& CheckboxBuilder::on_change(void (*fn)(bool, void*), void* user_data) {
    data_.on_change = fn;
    data_.change_user_data = user_data;
    return *this;
}

core::Handle<core::Node> CheckboxBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node)
        return {};

    if (!data_.font) {
        data_.font = text::FontManager::instance().load_system_font();
    }

    auto* heap_data = new CheckboxData(std::move(data_));
    heap_data->node_handle = handle;
    heap_data->tree = &tree_;

    float initial_p = heap_data->checked ? 1.0f : 0.0f;
    heap_data->check_progress.set_immediate(initial_p);

    core::Color initial_bg =
        heap_data->checked ? heap_data->box_color : core::Color{0.0f, 0.0f, 0.0f, 0.0f};
    heap_data->animated_bg.set_immediate(initial_bg);
    heap_data->animated_border.set_immediate(heap_data->box_color);

    heap_data->check_progress.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* cb = static_cast<CheckboxData*>(ctx);
            if (cb->tree && cb->node_handle.is_valid()) {
                cb->tree->mark_dirty(cb->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    heap_data->animated_bg.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* cb = static_cast<CheckboxData*>(ctx);
            if (cb->tree && cb->node_handle.is_valid()) {
                cb->tree->mark_dirty(cb->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    heap_data->animated_border.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* cb = static_cast<CheckboxData*>(ctx);
            if (cb->tree && cb->node_handle.is_valid()) {
                cb->tree->mark_dirty(cb->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    if (heap_data->bound_signal) {
        heap_data->bound_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* cb = static_cast<CheckboxData*>(ctx);
                if (cb->bound_signal && cb->checked != cb->bound_signal->get()) {
                    cb->checked = cb->bound_signal->get();
                    cb->update_targets();
                    if (cb->tree && cb->node_handle.is_valid()) {
                        cb->tree->mark_dirty(cb->node_handle, core::kPaint);
                    }
                }
            },
            heap_data);
    }

    node->user_data = heap_data;
    node->layout_fn = checkbox_layout_fn;
    node->paint_fn = checkbox_paint_fn;
    node->hit_test_fn = checkbox_hit_test_fn;
    node->destroy_fn = checkbox_destroy_fn;

    return handle;
}

} // namespace north::widget
