#include "north/widget/button.hpp"

#include "north/render/display_list.hpp"
#include "north/text/font_manager.hpp"

#include <algorithm>

namespace north::widget {

namespace {

core::Size button_layout_fn(core::NodeTree&, core::Handle<core::Node>,
                            const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<ButtonData*>(user_data);
    if (!data) {
        return constraints.constrain({0.0f, 0.0f});
    }

    core::Edges pad = data->padding;
    float pad_w = pad.horizontal();
    float pad_h = pad.vertical();

    if (!data->paragraph && data->font && !data->label.empty()) {
        text::ParagraphBuilder pb(text::TextAlign::Center);
        text::TextStyle style{
            .font = data->font, .font_size = data->font_size, .color = data->text_color};
        pb.add_text(data->label, style);
        data->paragraph = pb.build();
    }

    float content_w = 0.0f;
    float content_h = 0.0f;
    if (data->paragraph) {
        data->paragraph->layout(constraints.deflate(pad));
        content_w = data->paragraph->size().width;
        content_h = data->paragraph->size().height;
    }

    return constraints.constrain({content_w + pad_w, content_h + pad_h});
}

void button_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                     void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    if (!dl)
        return;
    auto* data = reinterpret_cast<ButtonData*>(user_data);
    if (!data)
        return;

    auto* node = tree.get(handle);
    if (!node)
        return;

    core::Rect bounds{origin.x, origin.y, node->layout.size.width, node->layout.size.height};

    ButtonState st = data->state.get();
    const style::BoxStyle& active_style =
        (st == ButtonState::Pressed)
            ? data->pressed_style
            : (st == ButtonState::Hovered ? data->hover_style : data->normal_style);

    core::Color bg_color = data->animated_bg.get();
    core::Color border_color = data->animated_border.get();

    if (active_style.shadow_color.a > 0.0f && active_style.shadow_radius > 0.0f) {
        dl->push_shadow(bounds, active_style.corner_radii, active_style.shadow_color,
                        active_style.shadow_radius, active_style.shadow_spread,
                        active_style.shadow_offset);
    }

    if (bg_color.a > 0.0f ||
        (border_color.a > 0.0f &&
         (active_style.border_width.top > 0.0f || active_style.border_width.right > 0.0f ||
          active_style.border_width.bottom > 0.0f || active_style.border_width.left > 0.0f))) {
        dl->push_quad(bounds, active_style.corner_radii, bg_color, border_color,
                      active_style.border_width);
    }

    if (data->paragraph && dl->glyph_atlas()) {
        float text_x = origin.x + (node->layout.size.width - data->paragraph->size().width) * 0.5f;
        float text_y =
            origin.y + (node->layout.size.height - data->paragraph->size().height) * 0.5f;
        data->paragraph->paint(*dl, *dl->glyph_atlas(), {text_x, text_y});
    }
}

bool button_hit_test_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                        core::Point local_point, void*) {
    auto* node = tree.get(handle);
    if (!node)
        return false;
    core::Rect local_bounds{0.0f, 0.0f, node->layout.size.width, node->layout.size.height};
    return local_bounds.contains(local_point);
}

void button_destroy_fn(void* user_data) {
    delete reinterpret_cast<ButtonData*>(user_data);
}

} // namespace

ButtonBuilder::ButtonBuilder(core::NodeTree& tree, std::string_view label) : tree_(tree) {
    data_.label = std::string(label);
    theme(style::Theme::dark());
}

ButtonBuilder& ButtonBuilder::on_click(ClickCallbackFn fn, void* user_data) {
    data_.on_click = fn;
    data_.click_user_data = user_data;
    return *this;
}

ButtonBuilder& ButtonBuilder::font(std::shared_ptr<text::Font> font) {
    data_.font = std::move(font);
    return *this;
}

ButtonBuilder& ButtonBuilder::font_size(float size) {
    data_.font_size = size;
    return *this;
}

ButtonBuilder& ButtonBuilder::padding(core::Edges padding) {
    data_.padding = padding;
    return *this;
}

ButtonBuilder& ButtonBuilder::corner_radii(core::CornerRadii radii) {
    data_.normal_style.corner_radii = radii;
    data_.hover_style.corner_radii = radii;
    data_.pressed_style.corner_radii = radii;
    return *this;
}

ButtonBuilder& ButtonBuilder::theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    data_.text_color = c.on_primary;

    core::CornerRadii radii = core::CornerRadii::uniform(6.0f);

    data_.normal_style = style::BoxStyle{.background = c.primary,
                                         .border_color = c.outline_variant,
                                         .border_width = core::Edges::uniform(1.0f),
                                         .corner_radii = radii,
                                         .padding = data_.padding,
                                         .shadow_color = core::Color{0.0f, 0.0f, 0.0f, 0.25f},
                                         .shadow_radius = 4.0f,
                                         .shadow_spread = 0.0f,
                                         .shadow_offset = core::Point{0.0f, 2.0f}};

    core::Color hover_bg{(std::min)(c.primary.r * 1.15f, 1.0f),
                         (std::min)(c.primary.g * 1.15f, 1.0f),
                         (std::min)(c.primary.b * 1.15f, 1.0f), c.primary.a};

    data_.hover_style = style::BoxStyle{.background = hover_bg,
                                        .border_color = c.outline,
                                        .border_width = core::Edges::uniform(1.0f),
                                        .corner_radii = radii,
                                        .padding = data_.padding,
                                        .shadow_color = core::Color{0.0f, 0.0f, 0.0f, 0.35f},
                                        .shadow_radius = 6.0f,
                                        .shadow_spread = 0.0f,
                                        .shadow_offset = core::Point{0.0f, 3.0f}};

    core::Color pressed_bg{c.primary.r * 0.85f, c.primary.g * 0.85f, c.primary.b * 0.85f,
                           c.primary.a};

    data_.pressed_style = style::BoxStyle{.background = pressed_bg,
                                          .border_color = c.outline,
                                          .border_width = core::Edges::uniform(1.0f),
                                          .corner_radii = radii,
                                          .padding = data_.padding,
                                          .shadow_color = core::Color{0.0f, 0.0f, 0.0f, 0.1f},
                                          .shadow_radius = 2.0f,
                                          .shadow_spread = 0.0f,
                                          .shadow_offset = core::Point{0.0f, 1.0f}};

    return *this;
}

ButtonBuilder& ButtonBuilder::style(const style::BoxStyle& normal, const style::BoxStyle& hover,
                                    const style::BoxStyle& pressed) {
    data_.normal_style = normal;
    data_.hover_style = hover;
    data_.pressed_style = pressed;
    return *this;
}

core::Handle<core::Node> ButtonBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node) {
        return {};
    }

    if (!data_.font) {
        data_.font = text::FontManager::instance().load_system_font();
    }

    auto* heap_data = new ButtonData(std::move(data_));
    heap_data->node_handle = handle;
    heap_data->tree = &tree_;
    heap_data->animated_bg.set_immediate(heap_data->normal_style.background);
    heap_data->animated_border.set_immediate(heap_data->normal_style.border_color);

    heap_data->animated_bg.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* btn = static_cast<ButtonData*>(ctx);
            if (btn->tree && btn->node_handle.is_valid()) {
                btn->tree->mark_dirty(btn->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    heap_data->animated_border.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* btn = static_cast<ButtonData*>(ctx);
            if (btn->tree && btn->node_handle.is_valid()) {
                btn->tree->mark_dirty(btn->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    heap_data->state.bind_dirty(
        [](void* ctx, uint32_t) {
            auto* btn = static_cast<ButtonData*>(ctx);
            btn->update_animation_targets();
        },
        heap_data);

    node->user_data = heap_data;
    node->layout_fn = &button_layout_fn;
    node->paint_fn = &button_paint_fn;
    node->hit_test_fn = &button_hit_test_fn;
    node->destroy_fn = &button_destroy_fn;

    return handle;
}

} // namespace north::widget
