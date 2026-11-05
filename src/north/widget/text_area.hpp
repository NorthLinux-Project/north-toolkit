#pragma once

#include "north/core/tree.hpp"
#include "north/style/theme.hpp"
#include "north/widget/widget_data.hpp"

#include <memory>
#include <string_view>

namespace north::widget {

class TextAreaBuilder {
public:
    explicit TextAreaBuilder(core::NodeTree& tree);

    TextAreaBuilder& text(std::string_view initial_text);
    TextAreaBuilder& placeholder(std::string_view placeholder);
    TextAreaBuilder& font(std::shared_ptr<text::Font> font);
    TextAreaBuilder& font_size(float size);
    TextAreaBuilder& padding(core::Edges padding);
    TextAreaBuilder& width(float width);
    TextAreaBuilder& height(float height);
    TextAreaBuilder& word_wrap(bool wrap);
    TextAreaBuilder& line_numbers(bool show);
    TextAreaBuilder& theme(const style::Theme& theme);
    TextAreaBuilder& theme_signal(core::Signal<style::Theme>* sig);
    TextAreaBuilder& selection_color(core::Color color);
    TextAreaBuilder& style(const style::BoxStyle& normal, const style::BoxStyle& focused);
    TextAreaBuilder& on_change(void (*fn)(std::string_view, void*), void* user_data = nullptr);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    TextAreaData data_;
};

} // namespace north::widget
