#include "north/widget/text.hpp"

#include "north/render/display_list.hpp"
#include "north/text/font_manager.hpp"

#include <algorithm>

namespace north::widget {

namespace {

core::Size text_layout_fn(core::NodeTree&, core::Handle<core::Node>,
                          const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<TextData*>(user_data);
    if (!data || !data->font) {
        return constraints.constrain({0.0f, 0.0f});
    }

    if (!data->paragraph) {
        text::ParagraphBuilder pb(data->align);
        text::TextStyle style{
            .font = data->font, .font_size = data->font_size, .color = data->color};
        pb.add_text(data->text_string, style);
        data->paragraph = pb.build();
    }

    float eff_max_w = (data->max_width > 0.0f) ? (std::min)(data->max_width, constraints.max_width)
                                               : constraints.max_width;
    layout::BoxConstraints text_constraints{constraints.min_width, eff_max_w,
                                            constraints.min_height, constraints.max_height};

    data->paragraph->layout(text_constraints);
    return constraints.constrain(data->paragraph->size());
}

void text_paint_fn(core::NodeTree&, core::Handle<core::Node>, core::Point origin, void* stream,
                   void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    if (!dl)
        return;
    auto* data = reinterpret_cast<TextData*>(user_data);
    if (!data || !data->paragraph || !dl->glyph_atlas())
        return;

    data->paragraph->paint(*dl, *dl->glyph_atlas(), origin);
}

void text_destroy_fn(void* user_data) {
    delete reinterpret_cast<TextData*>(user_data);
}

} // namespace

TextBuilder::TextBuilder(core::NodeTree& tree, std::string_view text) : tree_(tree) {
    data_.text_string = std::string(text);
}

TextBuilder& TextBuilder::font(std::shared_ptr<text::Font> font) {
    data_.font = std::move(font);
    return *this;
}

TextBuilder& TextBuilder::font_size(float size) {
    data_.font_size = size;
    return *this;
}

TextBuilder& TextBuilder::color(core::Color color) {
    data_.color = color;
    return *this;
}

TextBuilder& TextBuilder::align(text::TextAlign align) {
    data_.align = align;
    return *this;
}

TextBuilder& TextBuilder::max_width(float max_w) {
    data_.max_width = max_w;
    return *this;
}

core::Handle<core::Node> TextBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node) {
        return {};
    }

    if (!data_.font) {
        data_.font = text::FontManager::instance().load_system_font();
    }

    auto* heap_data = new TextData(std::move(data_));
    node->user_data = heap_data;
    node->layout_fn = &text_layout_fn;
    node->paint_fn = &text_paint_fn;
    node->destroy_fn = &text_destroy_fn;

    return handle;
}

} // namespace north::widget
