#include "north/widget/text_area.hpp"

#include "north/render/display_list.hpp"
#include "north/text/font_manager.hpp"
#include "north/text/shaper.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace north::widget {

namespace {

core::Size text_area_layout_fn(core::NodeTree&, core::Handle<core::Node>,
                                const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<TextAreaData*>(user_data);
    if (!data) {
        return constraints.constrain({0.0f, 0.0f});
    }

    float line_h = data->font ? data->font->line_height(data->font_size) : (data->font_size * 1.4f);

    if (data->show_line_numbers) {
        size_t total_lines = (std::max)(size_t{1}, data->editor.line_count());
        size_t digits = std::to_string(total_lines).size();
        digits = (std::max)(size_t{2}, digits);
        float char_w = data->font_size * 0.6f;
        data->line_numbers_width = static_cast<float>(digits) * char_w + 16.0f;
    } else {
        data->line_numbers_width = 0.0f;
    }

    core::Edges pad = data->padding;
    float w = (data->width > 0.0f) ? data->width : (320.0f + pad.horizontal() + data->line_numbers_width);
    float h = (data->height > 0.0f) ? data->height : (line_h * 6.0f + pad.vertical());

    return constraints.constrain({w, h});
}

void text_area_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                        void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    if (!dl) {
        return;
    }
    auto* data = reinterpret_cast<TextAreaData*>(user_data);
    if (!data) {
        return;
    }

    auto* node = tree.get(handle);
    if (!node) {
        return;
    }

    core::Rect bounds{origin.x, origin.y, node->layout.size.width, node->layout.size.height};
    const style::BoxStyle& active_style = data->is_focused ? data->focus_style : data->normal_style;

    if (active_style.shadow_color.a > 0.0f && active_style.shadow_radius > 0.0f) {
        dl->push_shadow(bounds, active_style.corner_radii, active_style.shadow_color,
                        active_style.shadow_radius, active_style.shadow_spread,
                        active_style.shadow_offset);
    }

    core::Color bg = data->is_focused ? data->focus_style.background : data->animated_bg.get();
    core::Color border = data->is_focused ? data->focus_style.border_color : data->animated_border.get();

    if (bg.a > 0.0f ||
        (border.a > 0.0f &&
         (active_style.border_width.top > 0.0f || active_style.border_width.right > 0.0f ||
          active_style.border_width.bottom > 0.0f || active_style.border_width.left > 0.0f))) {
        dl->push_quad(bounds, active_style.corner_radii, bg, border, active_style.border_width);
    }

    if (!data->font) {
        return;
    }

    float line_h = data->font->line_height(data->font_size);
    float gutter_w = data->line_numbers_width;

    float content_x = origin.x + data->padding.left + gutter_w;
    float content_y = origin.y + data->padding.top;
    float content_w = (std::max)(0.0f, node->layout.size.width - data->padding.horizontal() - gutter_w);
    float content_h = (std::max)(0.0f, node->layout.size.height - data->padding.vertical());

    // Paint line numbers gutter if enabled
    if (data->show_line_numbers && gutter_w > 0.0f) {
        core::Rect gutter_rect{origin.x, origin.y, gutter_w, node->layout.size.height};
        dl->push_quad(gutter_rect, core::CornerRadii{active_style.corner_radii.top_left, 0.0f, 0.0f, active_style.corner_radii.bottom_left},
                      data->line_number_bg, border, core::Edges{0.0f, active_style.border_width.right, 0.0f, 0.0f});

        size_t total_lines = data->editor.line_count();
        for (size_t l = 0; l < total_lines; ++l) {
            float y = content_y + static_cast<float>(l) * line_h - data->scroll_y;
            if (y + line_h < origin.y || y > origin.y + node->layout.size.height) {
                continue;
            }
            std::string line_num = std::to_string(l + 1);
            float num_w = text::Shaper::shape_text(line_num, data->font, data->font_size).total_advance;
            float nx = origin.x + gutter_w - num_w - 8.0f;

            text::ParagraphBuilder pb(text::TextAlign::Left);
            pb.add_text(line_num, text::TextStyle{.font = data->font,
                                                  .font_size = data->font_size,
                                                  .color = data->line_number_color});
            auto p = pb.build();
            p->layout(layout::BoxConstraints::loose({gutter_w, line_h}));
            if (dl->glyph_atlas()) {
                p->paint(*dl, *dl->glyph_atlas(), {nx, y}, data->line_number_color);
            }
        }
    }

    // Caret coordinates
    size_t caret_pos = data->editor.cursor_pos();
    size_t caret_line = data->editor.line_of(caret_pos);
    size_t line_st = data->editor.line_start(caret_line);
    size_t prefix_len = caret_pos - line_st;

    std::string_view prefix(data->editor.text().data() + line_st, prefix_len);
    float caret_x = text::Shaper::shape_text(prefix, data->font, data->font_size).total_advance;
    float caret_y = static_cast<float>(caret_line) * line_h;

    // Viewport auto-scrolling
    if (data->is_focused) {
        if (caret_y - data->scroll_y > content_h - line_h) {
            data->scroll_y = caret_y - content_h + line_h;
        } else if (caret_y < data->scroll_y) {
            data->scroll_y = caret_y;
        }

        if (!data->word_wrap) {
            if (caret_x - data->scroll_x > content_w - 8.0f) {
                data->scroll_x = caret_x - content_w + 8.0f;
            } else if (caret_x < data->scroll_x) {
                data->scroll_x = caret_x;
            }
        }
    }

    if (data->scroll_y < 0.0f) {
        data->scroll_y = 0.0f;
    }
    if (data->scroll_x < 0.0f) {
        data->scroll_x = 0.0f;
    }

    core::Rect content_clip{content_x, content_y, content_w, content_h};
    dl->push_clip(content_clip);

    core::Color cur_text_color = data->animated_text.get();

    // Placeholder if empty
    if (data->editor.empty() && !data->placeholder.empty()) {
        text::ParagraphBuilder pb(text::TextAlign::Left);
        pb.add_text(data->placeholder, text::TextStyle{.font = data->font,
                                                       .font_size = data->font_size,
                                                       .color = data->placeholder_color});
        data->placeholder_paragraph = pb.build();
        data->placeholder_paragraph->layout(layout::BoxConstraints::loose({content_w, content_h}));
        if (dl->glyph_atlas()) {
            data->placeholder_paragraph->paint(*dl, *dl->glyph_atlas(), {content_x, content_y}, data->placeholder_color);
        }
    } else {
        // Multi-line selection rendering
        if (data->editor.has_selection()) {
            auto [sel_start, sel_end] = data->editor.selection_range();
            size_t start_line = data->editor.line_of(sel_start);
            size_t end_line = data->editor.line_of(sel_end);

            for (size_t l = start_line; l <= end_line; ++l) {
                size_t l_start = data->editor.line_start(l);
                size_t l_end = data->editor.line_end(l);

                size_t s_in_line = (l == start_line) ? sel_start : l_start;
                size_t e_in_line = (l == end_line) ? sel_end : l_end;

                float sel_x1 = 0.0f;
                if (s_in_line > l_start) {
                    sel_x1 = text::Shaper::shape_text(std::string_view(data->editor.text().data() + l_start, s_in_line - l_start),
                                                      data->font, data->font_size).total_advance;
                }

                float sel_x2 = 0.0f;
                if (e_in_line > l_start) {
                    sel_x2 = text::Shaper::shape_text(std::string_view(data->editor.text().data() + l_start, e_in_line - l_start),
                                                      data->font, data->font_size).total_advance;
                }

                if (l < end_line) {
                    sel_x2 += data->font_size * 0.4f;
                }

                float y = content_y + static_cast<float>(l) * line_h - data->scroll_y;
                float x = content_x + sel_x1 - data->scroll_x;
                float w = (std::max)(2.0f, sel_x2 - sel_x1);

                core::Rect sel_rect{x, y, w, line_h};
                dl->push_quad(sel_rect, core::CornerRadii::uniform(1.0f), data->selection_color,
                              core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
            }
        }

        // Render visible text lines
        size_t total_lines = data->editor.line_count();
        for (size_t l = 0; l < total_lines; ++l) {
            float y = content_y + static_cast<float>(l) * line_h - data->scroll_y;
            if (y + line_h < content_y || y > content_y + content_h) {
                continue;
            }

            std::string_view line_str = data->editor.line_text(l);
            if (!line_str.empty()) {
                text::ParagraphBuilder pb(text::TextAlign::Left);
                pb.add_text(line_str, text::TextStyle{.font = data->font,
                                                      .font_size = data->font_size,
                                                      .color = cur_text_color});
                auto p = pb.build();
                p->layout(layout::BoxConstraints::loose({(std::max)(4096.0f, content_w + 1000.0f), line_h}));
                if (dl->glyph_atlas()) {
                    p->paint(*dl, *dl->glyph_atlas(), {content_x - data->scroll_x, y}, cur_text_color);
                }
            }
        }
    }

    // Caret rendering
    if (data->is_focused && data->cursor_visible) {
        float cx = content_x + caret_x - data->scroll_x;
        float cy = content_y + caret_y - data->scroll_y;
        core::Rect caret_rect{cx, cy, 1.5f, line_h};
        dl->push_quad(caret_rect, core::CornerRadii::uniform(0.5f), data->cursor_color,
                      core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
    }

    // Inline IME composition rendering
    if (!data->composition_text.empty()) {
        float cx = content_x + caret_x - data->scroll_x;
        float cy = content_y + caret_y - data->scroll_y;
        float comp_w = text::Shaper::shape_text(data->composition_text, data->font, data->font_size).total_advance;

        text::ParagraphBuilder pb(text::TextAlign::Left);
        pb.add_text(data->composition_text, text::TextStyle{.font = data->font,
                                                            .font_size = data->font_size,
                                                            .color = cur_text_color});
        auto p = pb.build();
        p->layout(layout::BoxConstraints::loose({comp_w + 20.0f, line_h}));
        if (dl->glyph_atlas()) {
            p->paint(*dl, *dl->glyph_atlas(), {cx, cy}, cur_text_color);
        }

        core::Rect uline{cx, cy + line_h - 1.5f, comp_w, 1.5f};
        dl->push_quad(uline, core::CornerRadii::uniform(0.0f), cur_text_color,
                      core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
    }

    dl->pop_clip();
}

bool text_area_hit_test_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                           core::Point local_point, void*) {
    auto* node = tree.get(handle);
    if (!node) {
        return false;
    }
    core::Rect local_bounds{0.0f, 0.0f, node->layout.size.width, node->layout.size.height};
    return local_bounds.contains(local_point);
}

void text_area_destroy_fn(void* user_data) {
    delete reinterpret_cast<TextAreaData*>(user_data);
}

} // namespace

void TextAreaData::apply_theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    text_color = c.on_surface;
    placeholder_color = c.outline;
    cursor_color = c.on_surface;
    selection_color = core::Color{c.primary.r, c.primary.g, c.primary.b, 0.50f};
    line_number_color = c.outline;
    line_number_bg = c.surface_variant;

    core::CornerRadii radii = normal_style.corner_radii;

    normal_style = style::BoxStyle{.background = c.surface_variant,
                                   .border_color = c.outline_variant,
                                   .border_width = normal_style.border_width,
                                   .corner_radii = radii,
                                   .padding = padding,
                                   .shadow_color = normal_style.shadow_color,
                                   .shadow_radius = normal_style.shadow_radius,
                                   .shadow_spread = normal_style.shadow_spread,
                                   .shadow_offset = normal_style.shadow_offset};

    focus_style = style::BoxStyle{.background = c.surface_variant,
                                  .border_color = c.outline,
                                  .border_width = focus_style.border_width,
                                  .corner_radii = radii,
                                  .padding = padding,
                                  .shadow_color = focus_style.shadow_color,
                                  .shadow_radius = focus_style.shadow_radius,
                                  .shadow_spread = focus_style.shadow_spread,
                                  .shadow_offset = focus_style.shadow_offset};

    const auto& active = is_focused ? focus_style : normal_style;
    float duration = theme.transition_duration();
    if (duration <= 0.0f) {
        animated_bg.set_immediate(active.background);
        animated_border.set_immediate(active.border_color);
        animated_text.set_immediate(text_color);
    } else {
        animated_bg.tween_to(active.background, duration, animation::CubicBezier::ease_out());
        animated_border.tween_to(active.border_color, duration, animation::CubicBezier::ease_out());
        animated_text.tween_to(text_color, duration, animation::CubicBezier::ease_out());
    }

    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

TextAreaBuilder::TextAreaBuilder(core::NodeTree& tree) : tree_(tree) {
    data_.font_size = 14.0f;
    data_.padding = core::Edges{8.0f, 12.0f, 8.0f, 12.0f};
    theme(style::Theme::dark());
}

TextAreaBuilder& TextAreaBuilder::text(std::string_view initial_text) {
    data_.editor.set_text(initial_text);
    return *this;
}

TextAreaBuilder& TextAreaBuilder::placeholder(std::string_view placeholder) {
    data_.placeholder = std::string(placeholder);
    return *this;
}

TextAreaBuilder& TextAreaBuilder::font(std::shared_ptr<text::Font> font) {
    data_.font = std::move(font);
    return *this;
}

TextAreaBuilder& TextAreaBuilder::font_size(float size) {
    data_.font_size = size;
    return *this;
}

TextAreaBuilder& TextAreaBuilder::padding(core::Edges padding) {
    data_.padding = padding;
    return *this;
}

TextAreaBuilder& TextAreaBuilder::width(float width) {
    data_.width = width;
    return *this;
}

TextAreaBuilder& TextAreaBuilder::height(float height) {
    data_.height = height;
    return *this;
}

TextAreaBuilder& TextAreaBuilder::word_wrap(bool wrap) {
    data_.word_wrap = wrap;
    return *this;
}

TextAreaBuilder& TextAreaBuilder::line_numbers(bool show) {
    data_.show_line_numbers = show;
    return *this;
}

TextAreaBuilder& TextAreaBuilder::theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    data_.text_color = c.on_surface;
    data_.placeholder_color = c.outline;
    data_.cursor_color = c.on_surface;
    data_.selection_color = core::Color{c.primary.r, c.primary.g, c.primary.b, 0.50f};
    data_.line_number_color = c.outline;
    data_.line_number_bg = c.surface_variant;

    core::CornerRadii radii = core::CornerRadii::uniform(6.0f);

    data_.normal_style = style::BoxStyle{.background = c.surface_variant,
                                         .border_color = c.outline_variant,
                                         .border_width = core::Edges::uniform(1.0f),
                                         .corner_radii = radii,
                                         .padding = data_.padding,
                                         .shadow_color = core::Color{0.0f, 0.0f, 0.0f, 0.0f},
                                         .shadow_radius = 0.0f,
                                         .shadow_spread = 0.0f,
                                         .shadow_offset = core::Point{0.0f, 0.0f}};

    data_.focus_style = style::BoxStyle{.background = c.surface_variant,
                                        .border_color = c.outline,
                                        .border_width = core::Edges::uniform(1.0f),
                                        .corner_radii = radii,
                                        .padding = data_.padding,
                                        .shadow_color = core::Color{0.0f, 0.0f, 0.0f, 0.0f},
                                        .shadow_radius = 0.0f,
                                        .shadow_spread = 0.0f,
                                        .shadow_offset = core::Point{0.0f, 0.0f}};
    return *this;
}

TextAreaBuilder& TextAreaBuilder::theme_signal(core::Signal<style::Theme>* sig) {
    data_.theme_signal = sig;
    if (sig) {
        theme(sig->get());
    }
    return *this;
}

TextAreaBuilder& TextAreaBuilder::selection_color(core::Color color) {
    data_.selection_color = color;
    return *this;
}

TextAreaBuilder& TextAreaBuilder::style(const style::BoxStyle& normal,
                                        const style::BoxStyle& focused) {
    data_.normal_style = normal;
    data_.focus_style = focused;
    return *this;
}

TextAreaBuilder& TextAreaBuilder::on_change(void (*fn)(std::string_view, void*), void* user_data) {
    data_.on_change = fn;
    data_.change_user_data = user_data;
    return *this;
}

core::Handle<core::Node> TextAreaBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node) {
        return core::Handle<core::Node>{};
    }

    if (!data_.font) {
        data_.font = text::FontManager::instance().load_system_font();
    }

    auto* heap_data = new TextAreaData(std::move(data_));
    heap_data->tree = &tree_;
    heap_data->node_handle = handle;

    heap_data->animated_bg.set_immediate(heap_data->normal_style.background);
    heap_data->animated_border.set_immediate(heap_data->normal_style.border_color);
    heap_data->animated_text.set_immediate(heap_data->text_color);

    heap_data->animated_bg.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* area = static_cast<TextAreaData*>(ctx);
            if (area->tree && area->node_handle.is_valid()) {
                area->tree->mark_dirty(area->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    heap_data->animated_border.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* area = static_cast<TextAreaData*>(ctx);
            if (area->tree && area->node_handle.is_valid()) {
                area->tree->mark_dirty(area->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    heap_data->animated_text.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* area = static_cast<TextAreaData*>(ctx);
            if (area->tree && area->node_handle.is_valid()) {
                area->tree->mark_dirty(area->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    if (heap_data->theme_signal) {
        heap_data->theme_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* area = static_cast<TextAreaData*>(ctx);
                if (area && area->theme_signal) {
                    area->apply_theme(area->theme_signal->get());
                }
            },
            heap_data, core::kPaint);
    }

    node->user_data = heap_data;
    node->layout_fn = &text_area_layout_fn;
    node->paint_fn = &text_area_paint_fn;
    node->hit_test_fn = &text_area_hit_test_fn;
    node->destroy_fn = &text_area_destroy_fn;

    return handle;
}

} // namespace north::widget
