#pragma once

#include "north/core/node.hpp"
#include "north/core/slab_pool.hpp"

namespace north::core {

class NodeTree {
public:
    NodeTree() = default;

    [[nodiscard]] Node::HandleType create_node();
    void destroy_node(Node::HandleType handle) noexcept;

    void set_root(Node::HandleType root) noexcept;
    [[nodiscard]] Node::HandleType root() const noexcept;

    [[nodiscard]] Node* get(Node::HandleType handle) noexcept;
    [[nodiscard]] const Node* get(Node::HandleType handle) const noexcept;

    void append_child(Node::HandleType parent, Node::HandleType child);
    void remove_child(Node::HandleType parent, Node::HandleType child);

    void mark_dirty(Node::HandleType handle, uint32_t flags) noexcept;

    void layout(const layout::BoxConstraints& root_constraints);
    Size layout_subtree(Node::HandleType handle, const layout::BoxConstraints& constraints);
    void paint(void* stream);
    void paint_subtree(Node::HandleType handle, Point origin, void* stream);
    void paint_children(Node::HandleType handle, Point origin, void* stream);

    void set_default_paint(PaintFn fn) noexcept { default_paint_fn_ = fn; }
    [[nodiscard]] PaintFn default_paint() const noexcept { return default_paint_fn_; }

    [[nodiscard]] size_t node_count() const noexcept;

private:
    SlabPool<Node, 64> pool_;
    Node::HandleType root_{};
    PaintFn default_paint_fn_{nullptr};

    Size layout_recursive(Node::HandleType handle, const layout::BoxConstraints& constraints);
    void paint_recursive(Node::HandleType handle, Point origin, void* stream);
    void destroy_descendants(Node::HandleType handle) noexcept;
};

} // namespace north::core
