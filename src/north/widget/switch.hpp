#pragma once

#include "north/core/signal.hpp"
#include "north/core/tree.hpp"
#include "north/style/theme.hpp"
#include "north/text/font.hpp"
#include "north/widget/widget_data.hpp"

#include <memory>
#include <string_view>

namespace north::widget {

class SwitchBuilder {
public:
    explicit SwitchBuilder(core::NodeTree& tree);

    SwitchBuilder& checked(bool is_checked);
    SwitchBuilder& bind(core::Signal<bool>& signal);
    SwitchBuilder& label(std::string_view text);
    SwitchBuilder& font(std::shared_ptr<text::Font> font);
    SwitchBuilder& font_size(float size);
    SwitchBuilder& theme(const style::Theme& theme);
    SwitchBuilder& on_change(void (*fn)(bool checked, void* user_data), void* user_data = nullptr);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    SwitchData data_;
};

} // namespace north::widget
