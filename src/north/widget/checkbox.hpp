#pragma once

#include "north/core/signal.hpp"
#include "north/core/tree.hpp"
#include "north/style/theme.hpp"
#include "north/text/font.hpp"
#include "north/widget/widget_data.hpp"

#include <memory>
#include <string_view>

namespace north::widget {

class CheckboxBuilder {
public:
    explicit CheckboxBuilder(core::NodeTree& tree);

    CheckboxBuilder& checked(bool is_checked);
    CheckboxBuilder& bind(core::Signal<bool>& signal);
    CheckboxBuilder& label(std::string_view text);
    CheckboxBuilder& font(std::shared_ptr<text::Font> font);
    CheckboxBuilder& font_size(float size);
    CheckboxBuilder& theme(const style::Theme& theme);
    CheckboxBuilder& theme_signal(core::Signal<style::Theme>* sig);
    CheckboxBuilder& on_change(void (*fn)(bool checked, void* user_data),
                               void* user_data = nullptr);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    CheckboxData data_;
};

} // namespace north::widget
