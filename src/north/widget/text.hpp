#pragma once

#include "north/core/tree.hpp"
#include "north/text/font.hpp"
#include "north/text/paragraph.hpp"
#include "north/widget/widget_data.hpp"

#include <memory>
#include <string_view>

namespace north::widget {

class TextBuilder {
public:
    TextBuilder(core::NodeTree& tree, std::string_view text);

    TextBuilder& font(std::shared_ptr<text::Font> font);
    TextBuilder& font_size(float size);
    TextBuilder& color(core::Color color);
    TextBuilder& theme_signal(core::Signal<style::Theme>* sig, TextThemeFn fn);
    TextBuilder& align(text::TextAlign align);
    TextBuilder& max_width(float max_w);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    TextData data_;
};

} // namespace north::widget
