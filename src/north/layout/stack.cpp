#include "north/layout/stack.hpp"

#include "north/core/tree.hpp"

#include <algorithm>

namespace north::layout {

namespace {

core::Point align_rect(Alignment align, core::Size container, core::Size item) noexcept {
    float x = 0.0f;
    float y = 0.0f;

    switch (align) {
    case Alignment::TopLeft:
        x = 0.0f;
        y = 0.0f;
        break;
    case Alignment::TopCenter:
        x = (container.width - item.width) * 0.5f;
        y = 0.0f;
        break;
    case Alignment::TopRight:
        x = container.width - item.width;
        y = 0.0f;
        break;
    case Alignment::CenterLeft:
        x = 0.0f;
        y = (container.height - item.height) * 0.5f;
        break;
    case Alignment::Center:
        x = (container.width - item.width) * 0.5f;
        y = (container.height - item.height) * 0.5f;
        break;
    case Alignment::CenterRight:
        x = container.width - item.width;
        y = (container.height - item.height) * 0.5f;
        break;
    case Alignment::BottomLeft:
        x = 0.0f;
        y = container.height - item.height;
        break;
    case Alignment::BottomCenter:
        x = (container.width - item.width) * 0.5f;
        y = container.height - item.height;
        break;
    case Alignment::BottomRight:
        x = container.width - item.width;
        y = container.height - item.height;
        break;
    }

    return {x, y};
}

} // namespace

core::Size stack_layout(core::NodeTree& tree, core::Handle<core::Node> container,
                        const BoxConstraints& constraints, const StackStyle& style) {
    core::Node* container_node = tree.get(container);
    if (!container_node) {
        return {0.0f, 0.0f};
    }

    const auto& children = container_node->children;
    if (children.empty()) {
        return constraints.constrain({0.0f, 0.0f});
    }

    float max_child_w = 0.0f;
    float max_child_h = 0.0f;

    // Pass 1: Measure unpositioned children
    for (core::Handle<core::Node> child_handle : children) {
        core::Node* child_node = tree.get(child_handle);
        if (!child_node)
            continue;

        if (!child_node->positioned.is_positioned()) {
            BoxConstraints loose_c = constraints.loosen();
            core::Size sz =
                child_node->layout_fn
                    ? child_node->layout_fn(tree, child_handle, loose_c, child_node->user_data)
                    : child_node->layout.size;

            child_node->layout.size = sz;
            max_child_w = std::max(max_child_w, sz.width);
            max_child_h = std::max(max_child_h, sz.height);
        }
    }

    core::Size stack_size = constraints.constrain({max_child_w, max_child_h});

    // Pass 2: Position all children
    for (core::Handle<core::Node> child_handle : children) {
        core::Node* child_node = tree.get(child_handle);
        if (!child_node)
            continue;

        const PositionedData& pos = child_node->positioned;

        if (!pos.is_positioned()) {
            child_node->layout.offset =
                align_rect(style.default_alignment, stack_size, child_node->layout.size);
        } else {
            float w = pos.width.value_or(child_node->layout.size.width);
            float h = pos.height.value_or(child_node->layout.size.height);
            float x = 0.0f;
            float y = 0.0f;

            if (pos.left.has_value() && pos.right.has_value()) {
                w = std::max(0.0f, stack_size.width - *pos.left - *pos.right);
                x = *pos.left;
            } else if (pos.left.has_value()) {
                x = *pos.left;
            } else if (pos.right.has_value()) {
                x = stack_size.width - *pos.right - w;
            } else {
                x = align_rect(style.default_alignment, stack_size, {w, h}).x;
            }

            if (pos.top.has_value() && pos.bottom.has_value()) {
                h = std::max(0.0f, stack_size.height - *pos.top - *pos.bottom);
                y = *pos.top;
            } else if (pos.top.has_value()) {
                y = *pos.top;
            } else if (pos.bottom.has_value()) {
                y = stack_size.height - *pos.bottom - h;
            } else {
                y = align_rect(style.default_alignment, stack_size, {w, h}).y;
            }

            BoxConstraints child_c = BoxConstraints::tight({w, h});
            if (child_node->layout_fn) {
                child_node->layout_fn(tree, child_handle, child_c, child_node->user_data);
            }

            child_node->layout.size = {w, h};
            child_node->layout.offset = {x, y};
        }
    }

    container_node->layout.size = stack_size;
    return stack_size;
}

} // namespace north::layout
