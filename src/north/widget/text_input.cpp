#include "north/widget/text_input.hpp"

#include "north/render/display_list.hpp"
#include "north/text/font_manager.hpp"
#include "north/text/shaper.hpp"

#include <algorithm>

namespace north::widget {

namespace {

core::Size text_input_layout_fn(core::NodeTree&, core::Handle<core::Node>,
                                const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<TextInputData*>(user_data);
    if (!data) {
        return constraints.constrain({0.0f, 0.0f});
    }

    core::Edges pad = data->padding;
    float line_h = data->font ? data->font->line_height(data->font_size) : (data->font_size * 1.3f);
    float w = (data->width > 0.0f) ? data->width : (160.0f + pad.horizontal());
    float h = (data->height > 0.0f) ? data->height : (line_h + pad.vertical());

    return constraints.constrain({w, h});
}

void text_input_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                         void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    if (!dl)
        return;
    auto* data = reinterpret_cast<TextInputData*>(user_data);
    if (!data)
        return;

    auto* node = tree.get(handle);
    if (!node)
        return;

    core::Rect bounds{origin.x, origin.y, node->layout.size.width, node->layout.size.height};
    const style::BoxStyle& active_style = data->is_focused ? data->focus_style : data->normal_style;

    if (active_style.shadow_color.a > 0.0f && active_style.shadow_radius > 0.0f) {
        dl->push_shadow(bounds, active_style.corner_radii, active_style.shadow_color,
                        active_style.shadow_radius, active_style.shadow_spread,
                        active_style.shadow_offset);
    }

    if (active_style.background.a > 0.0f ||
        (active_style.border_color.a > 0.0f &&
         (active_style.border_width.top > 0.0f || active_style.border_width.right > 0.0f ||
          active_style.border_width.bottom > 0.0f || active_style.border_width.left > 0.0f))) {
        dl->push_quad(bounds, active_style.corner_radii, active_style.background,
                      active_style.border_color, active_style.border_width);
    }

    if (!data->font)
        return;

    float line_h = data->font->line_height(data->font_size);
    float content_area_w = (std::max)(0.0f, node->layout.size.width - data->padding.horizontal());
    float content_area_h = (std::max)(0.0f, node->layout.size.height - data->padding.vertical());
    float vert_offset = std::floor((std::max)(0.0f, (content_area_h - line_h) * 0.5f));

    bool has_text = !data->buffer.empty();
    bool has_comp = !data->composition_text.empty();

    float caret_x = 0.0f;
    if (has_comp) {
        float pre_comp_x = text::Shaper::shape_text(std::string_view(data->buffer.text().data(),
                                                                     data->buffer.cursor_pos()),
                                                    data->font, data->font_size)
                               .total_advance;
        size_t comp_pos = (std::min)(static_cast<size_t>((std::max)(0, data->composition_cursor)),
                                     data->composition_text.size());
        float comp_caret_x =
            text::Shaper::shape_text(std::string_view(data->composition_text.data(), comp_pos),
                                     data->font, data->font_size)
                .total_advance;
        caret_x = pre_comp_x + comp_caret_x;
    } else if (has_text) {
        caret_x = text::Shaper::shape_text(
                      std::string_view(data->buffer.text().data(), data->buffer.cursor_pos()),
                      data->font, data->font_size)
                      .total_advance;
    }

    if (data->is_focused) {
        if (caret_x - data->scroll_offset > content_area_w) {
            data->scroll_offset = caret_x - content_area_w;
        } else if (caret_x < data->scroll_offset) {
            data->scroll_offset = caret_x;
        }
    } else {
        float total_text_w =
            has_text ? text::Shaper::shape_text(data->buffer.text(), data->font, data->font_size)
                           .total_advance
                     : 0.0f;
        if (total_text_w <= content_area_w) {
            data->scroll_offset = 0.0f;
        }
    }

    if (data->scroll_offset < 0.0f) {
        data->scroll_offset = 0.0f;
    }

    core::Rect content_clip{origin.x + data->padding.left, origin.y + data->padding.top,
                            content_area_w, content_area_h};
    dl->push_clip(content_clip);

    core::Point text_origin{origin.x + data->padding.left - data->scroll_offset,
                            origin.y + data->padding.top + vert_offset};

    if (!has_text && !has_comp) {
        if (!data->placeholder.empty()) {
            text::ParagraphBuilder pb(text::TextAlign::Left);
            pb.add_text(data->placeholder, text::TextStyle{.font = data->font,
                                                           .font_size = data->font_size,
                                                           .color = data->placeholder_color});
            data->placeholder_paragraph = pb.build();
            data->placeholder_paragraph->layout(layout::BoxConstraints::loose(
                {node->layout.size.width - data->padding.horizontal(), content_area_h}));
            if (dl->glyph_atlas()) {
                data->placeholder_paragraph->paint(
                    *dl, *dl->glyph_atlas(),
                    {origin.x + data->padding.left, origin.y + data->padding.top + vert_offset});
            }
        }
    } else {
        if (data->buffer.has_selection() && !has_comp) {
            auto [start, end] = data->buffer.selection_range();
            float sel_start_x =
                text::Shaper::shape_text(std::string_view(data->buffer.text().data(), start),
                                         data->font, data->font_size)
                    .total_advance;
            float sel_end_x =
                text::Shaper::shape_text(std::string_view(data->buffer.text().data(), end),
                                         data->font, data->font_size)
                    .total_advance;
            core::Rect sel_rect{text_origin.x + sel_start_x, text_origin.y,
                                (std::max)(1.0f, sel_end_x - sel_start_x), line_h};
            dl->push_quad(sel_rect, core::CornerRadii::uniform(2.0f), data->selection_color,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        }

        text::ParagraphBuilder pb(text::TextAlign::Left);
        if (!has_comp) {
            pb.add_text(data->buffer.text(), text::TextStyle{.font = data->font,
                                                             .font_size = data->font_size,
                                                             .color = data->text_color});
        } else {
            size_t cur = data->buffer.cursor_pos();
            if (cur > 0) {
                pb.add_text(data->buffer.text().substr(0, cur),
                            text::TextStyle{.font = data->font,
                                            .font_size = data->font_size,
                                            .color = data->text_color});
            }
            pb.add_text(data->composition_text, text::TextStyle{.font = data->font,
                                                                .font_size = data->font_size,
                                                                .color = data->text_color});
            if (cur < data->buffer.text().size()) {
                pb.add_text(data->buffer.text().substr(cur),
                            text::TextStyle{.font = data->font,
                                            .font_size = data->font_size,
                                            .color = data->text_color});
            }
        }

        data->paragraph = pb.build();
        data->paragraph->layout(layout::BoxConstraints::loose(
            {(std::max)(4096.0f, caret_x + 1000.0f), content_area_h}));
        if (dl->glyph_atlas()) {
            data->paragraph->paint(*dl, *dl->glyph_atlas(), text_origin);
        }

        if (has_comp) {
            float pre_comp_x = text::Shaper::shape_text(std::string_view(data->buffer.text().data(),
                                                                         data->buffer.cursor_pos()),
                                                        data->font, data->font_size)
                                   .total_advance;
            float comp_w =
                text::Shaper::shape_text(data->composition_text, data->font, data->font_size)
                    .total_advance;
            core::Rect uline{text_origin.x + pre_comp_x, text_origin.y + line_h - 1.0f, comp_w,
                             1.5f};
            dl->push_quad(uline, core::CornerRadii::uniform(0.0f), data->text_color,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        }
    }

    if (data->is_focused && data->cursor_visible) {
        core::Rect caret_rect{text_origin.x + caret_x, text_origin.y, 1.5f, line_h};
        dl->push_quad(caret_rect, core::CornerRadii::uniform(0.5f), data->cursor_color,
                      core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
    }

    dl->pop_clip();
}

bool text_input_hit_test_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                            core::Point local_point, void*) {
    auto* node = tree.get(handle);
    if (!node)
        return false;
    core::Rect local_bounds{0.0f, 0.0f, node->layout.size.width, node->layout.size.height};
    return local_bounds.contains(local_point);
}

void text_input_destroy_fn(void* user_data) {
    delete reinterpret_cast<TextInputData*>(user_data);
}

} // namespace

TextInputBuilder::TextInputBuilder(core::NodeTree& tree) : tree_(tree) {
    data_.font_size = 14.0f;
    data_.padding = core::Edges{6.0f, 10.0f, 6.0f, 10.0f};
    theme(style::Theme::dark());
}

TextInputBuilder& TextInputBuilder::text(std::string_view initial_text) {
    data_.buffer.set_text(initial_text);
    return *this;
}

TextInputBuilder& TextInputBuilder::placeholder(std::string_view placeholder) {
    data_.placeholder = std::string(placeholder);
    return *this;
}

TextInputBuilder& TextInputBuilder::font(std::shared_ptr<text::Font> font) {
    data_.font = std::move(font);
    return *this;
}

TextInputBuilder& TextInputBuilder::font_size(float size) {
    data_.font_size = size;
    return *this;
}

TextInputBuilder& TextInputBuilder::padding(core::Edges padding) {
    data_.padding = padding;
    return *this;
}

TextInputBuilder& TextInputBuilder::width(float width) {
    data_.width = width;
    return *this;
}

TextInputBuilder& TextInputBuilder::height(float height) {
    data_.height = height;
    return *this;
}

TextInputBuilder& TextInputBuilder::theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    data_.text_color = c.on_surface;
    data_.placeholder_color = c.outline;
    data_.cursor_color = c.on_surface;
    data_.selection_color = core::Color{c.primary.r, c.primary.g, c.primary.b, 0.50f};

    core::CornerRadii radii = core::CornerRadii::uniform(4.0f);

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

TextInputBuilder& TextInputBuilder::selection_color(core::Color color) {
    data_.selection_color = color;
    return *this;
}

TextInputBuilder& TextInputBuilder::style(const style::BoxStyle& normal,
                                          const style::BoxStyle& focused) {
    data_.normal_style = normal;
    data_.focus_style = focused;
    return *this;
}

TextInputBuilder& TextInputBuilder::on_change(void (*fn)(std::string_view, void*),
                                              void* user_data) {
    data_.on_change = fn;
    data_.change_user_data = user_data;
    return *this;
}

TextInputBuilder& TextInputBuilder::on_submit(void (*fn)(std::string_view, void*),
                                              void* user_data) {
    data_.on_submit = fn;
    data_.submit_user_data = user_data;
    return *this;
}

core::Handle<core::Node> TextInputBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node) {
        return {};
    }

    if (!data_.font) {
        data_.font = text::FontManager::instance().load_system_font();
    }

    auto* heap_data = new TextInputData(std::move(data_));
    node->user_data = heap_data;
    node->layout_fn = &text_input_layout_fn;
    node->paint_fn = &text_input_paint_fn;
    node->hit_test_fn = &text_input_hit_test_fn;
    node->destroy_fn = &text_input_destroy_fn;

    return handle;
}

} // namespace north::widget
