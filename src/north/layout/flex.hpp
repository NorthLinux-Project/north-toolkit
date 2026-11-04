#pragma once

#include "north/core/handle.hpp"
#include "north/core/types.hpp"
#include "north/layout/constraints.hpp"

#include <cstdint>

namespace north::core {
class NodeTree;
class Node;
} // namespace north::core

namespace north::layout {

enum class FlexDirection : uint8_t { Row, Column };

enum class MainAxisAlignment : uint8_t {
    Start,
    Center,
    End,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly
};

enum class CrossAxisAlignment : uint8_t { Start, Center, End, Stretch };

struct FlexItemData {
    float grow{0.0f};
    float shrink{1.0f};
    float basis{-1.0f}; // -1.0f = use intrinsic size
};

struct FlexStyle {
    FlexDirection direction{FlexDirection::Row};
    MainAxisAlignment main_align{MainAxisAlignment::Start};
    CrossAxisAlignment cross_align{CrossAxisAlignment::Start};
    float gap{0.0f};
};

core::Size flex_layout(core::NodeTree& tree, core::Handle<core::Node> container,
                       const BoxConstraints& constraints, const FlexStyle& style);

} // namespace north::layout
