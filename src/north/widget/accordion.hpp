#pragma once

#include "north/core/tree.hpp"
#include "north/style/theme.hpp"
#include "north/widget/widget_data.hpp"

#include <memory>
#include <string_view>

namespace north::widget {

class AccordionBuilder {
public:
    explicit AccordionBuilder(core::NodeTree& tree);

    AccordionBuilder& item(std::string_view title, core::Handle<core::Node> content_node,
                           bool expanded = false);
    AccordionBuilder& allow_multiple(bool enabled);
    AccordionBuilder& font(std::shared_ptr<text::Font> font);
    AccordionBuilder& font_size(float size);
    AccordionBuilder& header_height(float height);
    AccordionBuilder& width(float width);
    AccordionBuilder& corner_radii(core::CornerRadii radii);
    AccordionBuilder& theme(const style::Theme& theme);
    AccordionBuilder& theme_signal(core::Signal<style::Theme>* sig, AccordionThemeFn fn = nullptr);
    AccordionBuilder& on_toggle(AccordionToggleCallbackFn fn, void* user_data = nullptr);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    AccordionData data_;
};

} // namespace north::widget
