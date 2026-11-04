#pragma once

#include "north/core/tree.hpp"
#include "north/widget/widget_data.hpp"

namespace north::widget {

class VirtualListBuilder {
public:
    explicit VirtualListBuilder(core::NodeTree& tree);

    VirtualListBuilder& item_count(size_t count);
    VirtualListBuilder& item_extent(float extent);
    VirtualListBuilder& item_extent_fn(ItemExtentFn fn, void* user_data = nullptr);
    VirtualListBuilder& item_builder(ItemBuilderFn fn, void* user_data = nullptr);
    VirtualListBuilder& width(float w);
    VirtualListBuilder& height(float h);
    VirtualListBuilder& smooth_scroll(bool enable = true);
    VirtualListBuilder& theme(const style::Theme& theme);
    VirtualListBuilder& scrollbar_style(const ScrollbarStyle& style);
    VirtualListBuilder& scroll_offset(float y);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    VirtualListData data_{};
    float width_{-1.0f};
    float height_{-1.0f};
};

} // namespace north::widget
