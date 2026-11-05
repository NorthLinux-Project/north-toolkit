#pragma once

#include "north/core/tree.hpp"
#include "north/style/theme.hpp"
#include "north/widget/widget_data.hpp"

namespace north::widget {

class SplitViewBuilder {
public:
    explicit SplitViewBuilder(core::NodeTree& tree);

    SplitViewBuilder& orientation(SplitOrientation orientation);
    SplitViewBuilder& sizing_mode(SplitSizingMode mode);
    SplitViewBuilder& split(float value);
    SplitViewBuilder& min_first_size(float min_size);
    SplitViewBuilder& max_first_size(float max_size);
    SplitViewBuilder& min_second_size(float min_size);
    SplitViewBuilder& max_second_size(float max_size);
    SplitViewBuilder& sash_thickness(float thickness);
    SplitViewBuilder& first(core::Handle<core::Node> first_pane);
    SplitViewBuilder& second(core::Handle<core::Node> second_pane);
    SplitViewBuilder& width(float width);
    SplitViewBuilder& height(float height);
    SplitViewBuilder& theme(const style::Theme& theme);
    SplitViewBuilder& theme_signal(core::Signal<style::Theme>* sig, SplitViewThemeFn fn = nullptr);
    SplitViewBuilder& on_change(SplitChangeCallbackFn fn, void* user_data = nullptr);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    SplitViewData data_;
};

} // namespace north::widget
