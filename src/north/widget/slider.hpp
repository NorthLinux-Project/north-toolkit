#pragma once

#include "north/core/signal.hpp"
#include "north/core/tree.hpp"
#include "north/style/theme.hpp"
#include "north/widget/widget_data.hpp"

namespace north::widget {

class SliderBuilder {
public:
    explicit SliderBuilder(core::NodeTree& tree);

    SliderBuilder& value(float val);
    SliderBuilder& range(float min_val, float max_val);
    SliderBuilder& step(float step_size);
    SliderBuilder& bind(core::Signal<float>& signal);
    SliderBuilder& width(float w);
    SliderBuilder& theme(const style::Theme& theme);
    SliderBuilder& on_change(void (*fn)(float value, void* user_data), void* user_data = nullptr);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    SliderData data_;
};

} // namespace north::widget
