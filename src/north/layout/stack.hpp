#pragma once

#include "north/core/handle.hpp"
#include "north/core/types.hpp"
#include "north/layout/constraints.hpp"

#include <cstdint>
#include <optional>

namespace north::core {
class NodeTree;
class Node;
} // namespace north::core

namespace north::layout {

enum class Alignment : uint8_t {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

struct PositionedData {
    std::optional<float> top;
    std::optional<float> right;
    std::optional<float> bottom;
    std::optional<float> left;
    std::optional<float> width;
    std::optional<float> height;

    [[nodiscard]] bool is_positioned() const noexcept {
        return top.has_value() || right.has_value() || bottom.has_value() || left.has_value() ||
               width.has_value() || height.has_value();
    }
};

struct StackStyle {
    Alignment default_alignment{Alignment::TopLeft};
};

core::Size stack_layout(core::NodeTree& tree, core::Handle<core::Node> container,
                        const BoxConstraints& constraints, const StackStyle& style);

} // namespace north::layout
