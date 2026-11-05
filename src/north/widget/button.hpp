#pragma once

#include "north/core/tree.hpp"
#include "north/style/theme.hpp"
#include "north/text/font.hpp"
#include "north/widget/widget_data.hpp"

#include <memory>
#include <string_view>

namespace north::widget {

class ButtonBuilder {
public:
    ButtonBuilder(core::NodeTree& tree, std::string_view label);

    ButtonBuilder& on_click(ClickCallbackFn fn, void* user_data = nullptr);
    ButtonBuilder& font(std::shared_ptr<text::Font> font);
    ButtonBuilder& font_size(float size);
    ButtonBuilder& text_color(core::Color color);
    ButtonBuilder& padding(core::Edges padding);
    ButtonBuilder& corner_radii(core::CornerRadii radii);
    ButtonBuilder& theme(const style::Theme& theme);
    ButtonBuilder& theme_signal(core::Signal<style::Theme>* sig);
    ButtonBuilder& style(const style::BoxStyle& normal, const style::BoxStyle& hover,
                         const style::BoxStyle& pressed);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    ButtonData data_;
};

} // namespace north::widget
