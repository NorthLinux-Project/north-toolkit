#pragma once

#include "north/core/tree.hpp"
#include "north/style/theme.hpp"
#include "north/widget/widget_data.hpp"

#include <memory>
#include <string_view>

namespace north::widget {

class SegmentedButtonBuilder {
public:
    explicit SegmentedButtonBuilder(core::NodeTree& tree);

    SegmentedButtonBuilder& segment(std::string_view label, std::string_view icon_svg = {},
                                    bool disabled = false);
    SegmentedButtonBuilder& multi_select(bool enabled);
    SegmentedButtonBuilder& selected(size_t index, bool is_selected = true);
    SegmentedButtonBuilder& font(std::shared_ptr<text::Font> font);
    SegmentedButtonBuilder& font_size(float size);
    SegmentedButtonBuilder& height(float height);
    SegmentedButtonBuilder& width(float width);
    SegmentedButtonBuilder& corner_radii(core::CornerRadii radii);
    SegmentedButtonBuilder& theme(const style::Theme& theme);
    SegmentedButtonBuilder& theme_signal(core::Signal<style::Theme>* sig,
                                         SegmentedButtonThemeFn fn = nullptr);
    SegmentedButtonBuilder& on_change(SegmentChangeCallbackFn fn, void* user_data = nullptr);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    SegmentedButtonData data_;
};

} // namespace north::widget
