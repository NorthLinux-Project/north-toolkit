#include "north/widget/segmented_button.hpp"

#include "north/render/display_list.hpp"
#include "north/render/svg_path.hpp"
#include "north/text/font_manager.hpp"
#include "north/text/paragraph.hpp"
#include "north/text/shaper.hpp"

#include <algorithm>
#include <cmath>

namespace north::widget {

namespace {

core::Size segmented_button_layout_fn(core::NodeTree&, core::Handle<core::Node>,
                                      const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<SegmentedButtonData*>(user_data);
    if (!data || data->segments.empty()) {
        return constraints.constrain({0.0f, 36.0f});
    }

    if (data->selected.size() < data->segments.size()) {
        data->selected.resize(data->segments.size(), false);
    }

    if (!data->multi_select) {
        bool any_selected = false;
        for (bool s : data->selected) {
            if (s) {
                any_selected = true;
                break;
            }
        }
        if (!any_selected && !data->segments.empty()) {
            data->selected[0] = true;
        }
    }

    float natural_total_w = 0.0f;
    for (auto& seg : data->segments) {
        float text_w = 0.0f;
        if (data->font && !seg.label.empty()) {
            text_w = text::Shaper::shape_text(seg.label, data->font, data->font_size).total_advance;
        } else {
            text_w = static_cast<float>(seg.label.size()) * (data->font_size * 0.55f);
        }
        float icon_w = !seg.icon_path.empty() ? 20.0f : 0.0f;
        float seg_w = (std::max)(56.0f, text_w + icon_w + data->segment_padding.horizontal());
        seg.computed_width = seg_w;
        natural_total_w += seg_w;
    }

    float desired_w = (data->width > 0.0f) ? data->width : natural_total_w;
    core::Size resolved = constraints.constrain({desired_w, data->height});

    if (natural_total_w > 0.0f && std::abs(resolved.width - natural_total_w) > 0.5f) {
        float scale = resolved.width / natural_total_w;
        float cur_x = 0.0f;
        for (auto& seg : data->segments) {
            seg.computed_x = cur_x;
            seg.computed_width *= scale;
            cur_x += seg.computed_width;
        }
    } else {
        float cur_x = 0.0f;
        for (auto& seg : data->segments) {
            seg.computed_x = cur_x;
            cur_x += seg.computed_width;
        }
    }

    if (!data->multi_select) {
        size_t sel_idx = data->selected_index();
        if (sel_idx < data->segments.size()) {
            const auto& seg = data->segments[sel_idx];
            if (!data->indicator_initialized) {
                data->indicator_x.set_immediate(seg.computed_x);
                data->indicator_width.set_immediate(seg.computed_width);
                data->indicator_initialized = true;
            } else {
                if (std::abs(data->indicator_x.target() - seg.computed_x) > 0.5f) {
                    data->indicator_x.tween_to(seg.computed_x, 0.16f,
                                               animation::CubicBezier::ease_out());
                }
                if (std::abs(data->indicator_width.target() - seg.computed_width) > 0.5f) {
                    data->indicator_width.tween_to(seg.computed_width, 0.16f,
                                                   animation::CubicBezier::ease_out());
                }
            }
        }
    }

    return resolved;
}

void segmented_button_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                               core::Point origin, void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    auto* data = reinterpret_cast<SegmentedButtonData*>(user_data);
    auto* node = tree.get(handle);
    if (!dl || !data || !node || data->segments.empty()) {
        return;
    }

    core::Rect bounds{origin.x, origin.y, node->layout.size.width, node->layout.size.height};
    dl->push_quad(bounds, data->corner_radii, data->container_bg, data->border_color,
                  core::Edges::uniform(1.0f));

    for (size_t i = 0; i < data->segments.size(); ++i) {
        if (i == data->hovered_index && !data->segments[i].disabled && !data->is_selected(i)) {
            core::Rect h_rect{origin.x + data->segments[i].computed_x + 2.0f, origin.y + 2.0f,
                              (std::max)(0.0f, data->segments[i].computed_width - 4.0f),
                              (std::max)(0.0f, bounds.height - 4.0f)};
            dl->push_quad(h_rect, core::CornerRadii::uniform(6.0f), data->hovered_segment_bg,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        }
    }

    if (!data->multi_select) {
        size_t sel_idx = data->selected_index();
        if (sel_idx < data->segments.size()) {
            float pill_x = data->indicator_x.get();
            float pill_w = data->indicator_width.get();
            core::Rect pill_rect{origin.x + pill_x + 2.0f, origin.y + 2.0f,
                                 (std::max)(4.0f, pill_w - 4.0f),
                                 (std::max)(4.0f, bounds.height - 4.0f)};
            core::CornerRadii pill_radii = core::CornerRadii::uniform(
                (std::max)(0.0f, data->corner_radii.top_left - 2.0f));
            dl->push_quad(pill_rect, pill_radii, data->active_pill_bg,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        }
    } else {
        for (size_t i = 0; i < data->segments.size(); ++i) {
            if (data->is_selected(i)) {
                core::Rect pill_rect{origin.x + data->segments[i].computed_x + 2.0f,
                                     origin.y + 2.0f,
                                     (std::max)(4.0f, data->segments[i].computed_width - 4.0f),
                                     (std::max)(4.0f, bounds.height - 4.0f)};
                core::CornerRadii pill_radii = core::CornerRadii::uniform(
                    (std::max)(0.0f, data->corner_radii.top_left - 2.0f));
                dl->push_quad(pill_rect, pill_radii, data->active_pill_bg,
                              core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
            }
        }
    }

    for (size_t i = 0; i + 1 < data->segments.size(); ++i) {
        if (!data->is_selected(i) && !data->is_selected(i + 1)) {
            float div_x = origin.x + data->segments[i + 1].computed_x;
            core::Rect div_rect{div_x - 0.5f, origin.y + 8.0f, 1.0f,
                                (std::max)(4.0f, bounds.height - 16.0f)};
            dl->push_quad(div_rect, core::CornerRadii::uniform(0.0f), data->border_color,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        }
    }

    for (size_t i = 0; i < data->segments.size(); ++i) {
        const auto& seg = data->segments[i];
        bool is_sel = data->is_selected(i);
        core::Color fg = seg.disabled ? data->disabled_text_color
                                      : (is_sel ? data->active_text_color : data->inactive_text_color);

        float text_w = 0.0f;
        float line_h = data->font ? data->font->line_height(data->font_size) : 18.0f;
        std::unique_ptr<text::Paragraph> para;

        if (data->font && !seg.label.empty()) {
            text::ParagraphBuilder pb(text::TextAlign::Left);
            pb.add_text(seg.label, text::TextStyle{.font = data->font,
                                                   .font_size = data->font_size,
                                                   .color = fg});
            para = pb.build();
            para->layout(layout::BoxConstraints::loose({seg.computed_width, line_h}));
            text_w = para->size().width;
        }

        float icon_w = !seg.icon_path.empty() ? 14.0f : 0.0f;
        float gap = (!seg.icon_path.empty() && !seg.label.empty()) ? 6.0f : 0.0f;
        float total_content_w = icon_w + gap + text_w;
        float start_x =
            origin.x + seg.computed_x + (seg.computed_width - total_content_w) * 0.5f;

        if (!seg.icon_path.empty()) {
            core::Rect b = seg.icon_path.bounds();
            float sx = (b.width > 1e-4f) ? (icon_w / b.width) : 1.0f;
            float sy = (b.height > 1e-4f) ? (icon_w / b.height) : 1.0f;
            render::Path p = seg.icon_path;
            p.translate({-b.x, -b.y});
            p.scale(sx, sy);
            p.translate({start_x, origin.y + (bounds.height - icon_w) * 0.5f});
            dl->push_path_fill(p, fg);
            start_x += icon_w + gap;
        }

        if (para && dl->glyph_atlas()) {
            float text_y = origin.y + std::floor((bounds.height - line_h) * 0.5f);
            para->paint(*dl, *dl->glyph_atlas(), {start_x, text_y}, fg);
        }
    }
}

void segmented_button_destroy_fn(void* user_data) {
    delete reinterpret_cast<SegmentedButtonData*>(user_data);
}

} // namespace

void SegmentedButtonData::toggle_segment(size_t index) {
    if (index >= segments.size() || segments[index].disabled) {
        return;
    }
    if (selected.size() < segments.size()) {
        selected.resize(segments.size(), false);
    }

    if (multi_select) {
        selected[index] = !selected[index];
        focused_index = index;
        if (tree && node_handle.is_valid()) {
            tree->mark_dirty(node_handle, core::kPaint);
        }
        if (on_change) {
            on_change(index, selected[index], change_user_data);
        }
    } else {
        bool was_selected = selected[index];
        for (size_t i = 0; i < selected.size(); ++i) {
            selected[i] = (i == index);
        }
        focused_index = index;
        indicator_x.tween_to(segments[index].computed_x, 0.16f, animation::CubicBezier::ease_out());
        indicator_width.tween_to(segments[index].computed_width, 0.16f,
                                 animation::CubicBezier::ease_out());

        if (tree && node_handle.is_valid()) {
            tree->mark_dirty(node_handle, core::kPaint);
        }
        if (!was_selected && on_change) {
            on_change(index, true, change_user_data);
        }
    }
}

bool SegmentedButtonData::is_selected(size_t index) const noexcept {
    return index < selected.size() && selected[index];
}

size_t SegmentedButtonData::selected_index() const noexcept {
    for (size_t i = 0; i < selected.size(); ++i) {
        if (selected[i]) {
            return i;
        }
    }
    return SIZE_MAX;
}

void SegmentedButtonData::apply_theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    container_bg = c.surface_variant;
    border_color = c.outline_variant;
    active_pill_bg = c.primary;
    hovered_segment_bg = core::Color{c.on_surface.r, c.on_surface.g, c.on_surface.b, 0.08f};
    active_text_color = c.on_primary;
    inactive_text_color = c.on_surface;
    disabled_text_color = core::Color{c.on_surface_variant.r, c.on_surface_variant.g,
                                      c.on_surface_variant.b, 0.4f};

    if (on_theme) {
        on_theme(theme, *this);
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

SegmentedButtonBuilder::SegmentedButtonBuilder(core::NodeTree& tree) : tree_(tree) {}

SegmentedButtonBuilder& SegmentedButtonBuilder::segment(std::string_view label,
                                                        std::string_view icon_svg, bool disabled) {
    SegmentItem item;
    item.label = std::string(label);
    item.disabled = disabled;
    if (!icon_svg.empty()) {
        (void)render::parse_svg_path(icon_svg, item.icon_path);
    }
    data_.segments.push_back(std::move(item));
    data_.selected.push_back(false);
    return *this;
}

SegmentedButtonBuilder& SegmentedButtonBuilder::multi_select(bool enabled) {
    data_.multi_select = enabled;
    return *this;
}

SegmentedButtonBuilder& SegmentedButtonBuilder::selected(size_t index, bool is_selected) {
    if (index >= data_.selected.size()) {
        data_.selected.resize(index + 1, false);
    }
    if (!data_.multi_select && is_selected) {
        std::fill(data_.selected.begin(), data_.selected.end(), false);
    }
    data_.selected[index] = is_selected;
    return *this;
}

SegmentedButtonBuilder& SegmentedButtonBuilder::font(std::shared_ptr<text::Font> font) {
    data_.font = std::move(font);
    return *this;
}

SegmentedButtonBuilder& SegmentedButtonBuilder::font_size(float size) {
    data_.font_size = size;
    return *this;
}

SegmentedButtonBuilder& SegmentedButtonBuilder::height(float height) {
    data_.height = height;
    return *this;
}

SegmentedButtonBuilder& SegmentedButtonBuilder::width(float width) {
    data_.width = width;
    return *this;
}

SegmentedButtonBuilder& SegmentedButtonBuilder::corner_radii(core::CornerRadii radii) {
    data_.corner_radii = radii;
    return *this;
}

SegmentedButtonBuilder& SegmentedButtonBuilder::theme(const style::Theme& theme) {
    data_.apply_theme(theme);
    return *this;
}

SegmentedButtonBuilder& SegmentedButtonBuilder::theme_signal(core::Signal<style::Theme>* sig,
                                                             SegmentedButtonThemeFn fn) {
    data_.theme_signal = sig;
    data_.on_theme = fn;
    if (sig) {
        data_.apply_theme(sig->get());
    }
    return *this;
}

SegmentedButtonBuilder& SegmentedButtonBuilder::on_change(SegmentChangeCallbackFn fn,
                                                          void* user_data) {
    data_.on_change = fn;
    data_.change_user_data = user_data;
    return *this;
}

core::Handle<core::Node> SegmentedButtonBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node) {
        return {};
    }

    if (!data_.font) {
        data_.font = text::FontManager::instance().load_system_font();
    }

    auto* heap_data = new SegmentedButtonData(std::move(data_));
    heap_data->tree = &tree_;
    heap_data->node_handle = handle;

    heap_data->indicator_x.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* sb = static_cast<SegmentedButtonData*>(ctx);
            if (sb->tree && sb->node_handle.is_valid()) {
                sb->tree->mark_dirty(sb->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    heap_data->indicator_width.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* sb = static_cast<SegmentedButtonData*>(ctx);
            if (sb->tree && sb->node_handle.is_valid()) {
                sb->tree->mark_dirty(sb->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    if (heap_data->theme_signal) {
        heap_data->theme_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* sb = static_cast<SegmentedButtonData*>(ctx);
                if (sb->theme_signal) {
                    sb->apply_theme(sb->theme_signal->get());
                }
            },
            heap_data, core::kPaint);
    }

    node->user_data = heap_data;
    node->layout_fn = &segmented_button_layout_fn;
    node->paint_fn = &segmented_button_paint_fn;
    node->destroy_fn = &segmented_button_destroy_fn;

    return handle;
}

} // namespace north::widget
