#pragma once

#include "north/core/tree.hpp"
#include "north/style/theme.hpp"
#include "north/widget/widget_data.hpp"

#include <string_view>

namespace north::widget {

class TextInputBuilder {
public:
    explicit TextInputBuilder(core::NodeTree& tree);

    TextInputBuilder& text(std::string_view initial_text);
    TextInputBuilder& placeholder(std::string_view placeholder);
    TextInputBuilder& font(std::shared_ptr<text::Font> font);
    TextInputBuilder& font_size(float size);
    TextInputBuilder& padding(core::Edges padding);
    TextInputBuilder& width(float width);
    TextInputBuilder& height(float height);
    TextInputBuilder& theme(const style::Theme& theme);
    TextInputBuilder& selection_color(core::Color color);
    TextInputBuilder& style(const style::BoxStyle& normal, const style::BoxStyle& focused);
    TextInputBuilder& on_change(void (*fn)(std::string_view, void*), void* user_data = nullptr);
    TextInputBuilder& on_submit(void (*fn)(std::string_view, void*), void* user_data = nullptr);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    TextInputData data_;
};

} // namespace north::widget
