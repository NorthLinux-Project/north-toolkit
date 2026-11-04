#pragma once

#include "north/core/handle.hpp"
#include "north/core/signal.hpp"
#include "north/core/slab_pool.hpp"
#include "north/core/types.hpp"
#include "north/layout/constraints.hpp"
#include "north/layout/flex.hpp"
#include "north/layout/stack.hpp"

#include <vector>

namespace north::core {

class NodeTree;

struct LayoutResult {
    Size size{0.0f, 0.0f};
    Point offset{0.0f, 0.0f};
};

struct NodePaintData {
    Color background{};
    Color border_color{};
    CornerRadii radii{};
    Edges border_width{};
    Edges padding{};
    float shadow_radius{0.0f};
    Color shadow_color{};
};

using LayoutFn = Size (*)(NodeTree& tree, Handle<class Node> handle,
                          const layout::BoxConstraints& constraints, void* user_data);
using PaintFn = void (*)(NodeTree& tree, Handle<class Node> handle, Point origin, void* stream,
                         void* user_data);
using HitTestFn = bool (*)(NodeTree& tree, Handle<class Node> handle, Point local_point,
                           void* user_data);
using DestroyFn = void (*)(void* user_data);

class Node {
public:
    using HandleType = Handle<Node>;

    HandleType parent{};
    std::vector<HandleType> children;

    uint32_t dirty{kAll};
    LayoutResult layout{};
    Rect clip_rect{};
    NodePaintData paint{};
    layout::FlexItemData flex_item{};
    layout::PositionedData positioned{};

    LayoutFn layout_fn{nullptr};
    PaintFn paint_fn{nullptr};
    HitTestFn hit_test_fn{nullptr};
    DestroyFn destroy_fn{nullptr};
    void* user_data{nullptr};
    bool custom_child_paint{false};

    Node() = default;
    ~Node() {
        if (destroy_fn && user_data) {
            destroy_fn(user_data);
            user_data = nullptr;
        }
    }
    Node(Node&&) noexcept = default;
    Node& operator=(Node&&) noexcept = default;
    Node(const Node&) = default;
    Node& operator=(const Node&) = default;

    [[nodiscard]] bool is_dirty(uint32_t mask) const noexcept { return (dirty & mask) != 0; }

    void mark_dirty(uint32_t mask) noexcept { dirty |= mask; }

    void clear_dirty(uint32_t mask) noexcept { dirty &= ~mask; }
};

} // namespace north::core
