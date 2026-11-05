#include "north/core/tree.hpp"

#include <algorithm>

namespace north::core {

Node::HandleType NodeTree::create_node() {
    return pool_.emplace();
}

void NodeTree::destroy_descendants(Node::HandleType handle) noexcept {
    Node* n = pool_.get(handle);
    if (!n)
        return;

    std::vector<Node::HandleType> children_copy = n->children;
    for (Node::HandleType child : children_copy) {
        destroy_descendants(child);
        pool_.destroy(child);
    }
    n->children.clear();
}

void NodeTree::destroy_node(Node::HandleType handle) noexcept {
    if (!pool_.is_valid(handle))
        return;

    Node* n = pool_.get(handle);
    if (n->parent.is_valid()) {
        Node* p = pool_.get(n->parent);
        if (p) {
            auto it = std::find(p->children.begin(), p->children.end(), handle);
            if (it != p->children.end()) {
                p->children.erase(it);
                mark_dirty(n->parent, kLayout | kChildren);
            }
        }
    }

    destroy_descendants(handle);
    pool_.destroy(handle);

    if (root_ == handle) {
        root_ = {};
    }
}

void NodeTree::set_root(Node::HandleType root) noexcept {
    root_ = root;
    if (root_.is_valid()) {
        mark_dirty(root_, kAll);
    }
}

Node::HandleType NodeTree::root() const noexcept {
    return root_;
}

Node* NodeTree::get(Node::HandleType handle) noexcept {
    return pool_.get(handle);
}

const Node* NodeTree::get(Node::HandleType handle) const noexcept {
    return pool_.get(handle);
}

void NodeTree::append_child(Node::HandleType parent, Node::HandleType child) {
    Node* p = pool_.get(parent);
    Node* c = pool_.get(child);
    if (!p || !c)
        return;

    if (c->parent.is_valid()) {
        remove_child(c->parent, child);
    }

    c->parent = parent;
    p->children.push_back(child);
    mark_dirty(parent, kLayout | kChildren);
}

void NodeTree::remove_child(Node::HandleType parent, Node::HandleType child) {
    Node* p = pool_.get(parent);
    Node* c = pool_.get(child);
    if (!p)
        return;

    auto it = std::find(p->children.begin(), p->children.end(), child);
    if (it != p->children.end()) {
        p->children.erase(it);
        if (c && c->parent == parent) {
            c->parent = {};
        }
        mark_dirty(parent, kLayout | kChildren);
    }
}

void NodeTree::mark_dirty(Node::HandleType handle, uint32_t flags) noexcept {
    Node* n = pool_.get(handle);
    if (!n)
        return;

    n->dirty |= flags;

    if (flags & (kLayout | kPaint | kTransform | kChildren)) {
        Node::HandleType curr = n->parent;
        while (curr.is_valid()) {
            Node* ancestor = pool_.get(curr);
            if (!ancestor)
                break;
            if (ancestor->dirty & kChildren) {
                break;
            }
            ancestor->dirty |= kChildren;
            curr = ancestor->parent;
        }
    }
}

void NodeTree::layout(const layout::BoxConstraints& root_constraints) {
    if (root_.is_valid()) {
        mark_dirty(root_, kLayout);
        layout_recursive(root_, root_constraints);
    }
}

Size NodeTree::layout_subtree(Node::HandleType handle, const layout::BoxConstraints& constraints) {
    if (handle.is_valid()) {
        mark_dirty(handle, kLayout);
        return layout_recursive(handle, constraints);
    }
    return {0.0f, 0.0f};
}

Size NodeTree::layout_recursive(Node::HandleType handle,
                                const layout::BoxConstraints& constraints) {
    Node* node = pool_.get(handle);
    if (!node)
        return {0.0f, 0.0f};

    if (!node->is_dirty(kLayout | kChildren)) {
        return node->layout.size;
    }

    Size computed_size{0.0f, 0.0f};
    if (node->layout_fn) {
        computed_size = node->layout_fn(*this, handle, constraints, node->user_data);
    } else {
        for (Node::HandleType ch : node->children) {
            layout_recursive(ch, constraints.loosen());
        }
        computed_size = constraints.constrain({0.0f, 0.0f});
    }

    node->layout.size = computed_size;
    node->clear_dirty(kLayout | kChildren);
    return computed_size;
}

void NodeTree::paint(void* stream) {
    if (root_.is_valid()) {
        paint_recursive(root_, {0.0f, 0.0f}, stream);
    }
}

void NodeTree::paint_subtree(Node::HandleType handle, Point origin, void* stream) {
    if (handle.is_valid()) {
        paint_recursive(handle, origin, stream);
    }
}

void NodeTree::paint_children(Node::HandleType handle, Point origin, void* stream) {
    Node* node = pool_.get(handle);
    if (!node)
        return;

    for (Node::HandleType ch : node->children) {
        paint_recursive(ch, origin, stream);
    }
}

void NodeTree::paint_recursive(Node::HandleType handle, Point origin, void* stream) {
    Node* node = pool_.get(handle);
    if (!node)
        return;

    Point node_origin = origin + node->layout.offset;
    if (node->paint_fn) {
        node->paint_fn(*this, handle, node_origin, stream, node->user_data);
    } else if (default_paint_fn_) {
        default_paint_fn_(*this, handle, node_origin, stream, nullptr);
    }

    if (!node->custom_child_paint) {
        for (Node::HandleType ch : node->children) {
            paint_recursive(ch, node_origin, stream);
        }
    }

    node->clear_dirty(kPaint);
}

size_t NodeTree::node_count() const noexcept {
    return pool_.size();
}

Point NodeTree::global_offset(Node::HandleType handle) const noexcept {
    Point pt{0.0f, 0.0f};
    Node::HandleType cur = handle;
    while (cur.is_valid()) {
        const Node* n = pool_.get(cur);
        if (!n) {
            break;
        }
        pt.x += n->layout.offset.x;
        pt.y += n->layout.offset.y;
        cur = n->parent;
    }
    return pt;
}

} // namespace north::core
