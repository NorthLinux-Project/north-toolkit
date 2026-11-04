#pragma once

#include "north/core/tree.hpp"
#include "north/widget/widget_data.hpp"

namespace north::widget {

class ScrollAreaBuilder {
public:
    explicit ScrollAreaBuilder(core::NodeTree& tree);

    ScrollAreaBuilder& direction(ScrollDirection dir);
    ScrollAreaBuilder& width(float w);
    ScrollAreaBuilder& height(float h);
    ScrollAreaBuilder& smooth_scroll(bool enable = true);
    ScrollAreaBuilder& theme(const style::Theme& theme);
    ScrollAreaBuilder& scrollbar_style(const ScrollbarStyle& style);
    ScrollAreaBuilder& scroll_offset(core::Point offset);
    ScrollAreaBuilder& child(core::Handle<core::Node> child_handle);
    ScrollAreaBuilder& on_scroll(void (*fn)(core::Point offset, void* user_data),
                                 void* user_data = nullptr);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    ScrollAreaData data_{};
    float width_{-1.0f};
    float height_{-1.0f};
    core::Handle<core::Node> child_{};
};

} // namespace north::widget
