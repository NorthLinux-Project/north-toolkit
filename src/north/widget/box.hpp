#pragma once

#include "north/core/tree.hpp"
#include "north/layout/flex.hpp"
#include "north/style/style.hpp"
#include "north/widget/widget_data.hpp"

#include <span>
#include <vector>

namespace north::widget {

class BoxBuilder {
public:
    explicit BoxBuilder(core::NodeTree& tree);

    BoxBuilder& style(const style::BoxStyle& style);
    BoxBuilder& background(core::Color bg);
    BoxBuilder& border(core::Color color, core::Edges width);
    BoxBuilder& corner_radii(core::CornerRadii radii);
    BoxBuilder& shadow(core::Color color, float radius, float spread = 0.0f,
                       core::Point offset = {});
    BoxBuilder& padding(core::Edges padding);
    BoxBuilder& margin(core::Edges margin);
    BoxBuilder& width(float w);
    BoxBuilder& height(float h);
    BoxBuilder& size(core::Size sz);
    BoxBuilder& flex_grow(float grow);
    BoxBuilder& flex_shrink(float shrink);
    BoxBuilder& flex_basis(float basis);
    BoxBuilder& flex(const layout::FlexStyle& style);
    BoxBuilder& flex(layout::FlexDirection dir,
                     layout::MainAxisAlignment main_align = layout::MainAxisAlignment::Start,
                     layout::CrossAxisAlignment cross_align = layout::CrossAxisAlignment::Stretch,
                     float gap = 0.0f);
    BoxBuilder& child(core::Handle<core::Node> child);
    BoxBuilder& children(std::span<const core::Handle<core::Node>> children);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    BoxData data_;
    std::vector<core::Handle<core::Node>> children_;
};

} // namespace north::widget
