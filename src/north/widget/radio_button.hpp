#pragma once

#include "north/core/signal.hpp"
#include "north/core/tree.hpp"
#include "north/style/theme.hpp"
#include "north/text/font.hpp"
#include "north/widget/widget_data.hpp"

#include <memory>
#include <string_view>

namespace north::widget {

class RadioButtonBuilder {
public:
    explicit RadioButtonBuilder(core::NodeTree& tree);

    RadioButtonBuilder& selected(bool is_selected);
    RadioButtonBuilder& value(int val);
    RadioButtonBuilder& group(core::Signal<int>& signal);
    RadioButtonBuilder& bind(core::Signal<bool>& signal);
    RadioButtonBuilder& label(std::string_view text);
    RadioButtonBuilder& font(std::shared_ptr<text::Font> font);
    RadioButtonBuilder& font_size(float size);
    RadioButtonBuilder& theme(const style::Theme& theme);
    RadioButtonBuilder& on_select(void (*fn)(int value, void* user_data),
                                  void* user_data = nullptr);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    RadioButtonData data_;
};

} // namespace north::widget
