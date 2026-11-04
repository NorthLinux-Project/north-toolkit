#include "north/layout/flex.hpp"

#include "north/core/tree.hpp"

#include <algorithm>
#include <numeric>
#include <vector>

namespace north::layout {

core::Size flex_layout(core::NodeTree& tree, core::Handle<core::Node> container,
                       const BoxConstraints& constraints, const FlexStyle& style) {
    core::Node* container_node = tree.get(container);
    if (!container_node) {
        return {0.0f, 0.0f};
    }

    const auto& children = container_node->children;
    if (children.empty()) {
        return constraints.constrain({0.0f, 0.0f});
    }

    const bool is_row = (style.direction == FlexDirection::Row);

    const float max_main_constraint = is_row ? constraints.max_width : constraints.max_height;
    const float min_main_constraint = is_row ? constraints.min_width : constraints.min_height;
    const float max_cross_constraint = is_row ? constraints.max_height : constraints.max_width;
    const float min_cross_constraint = is_row ? constraints.min_height : constraints.min_width;

    const size_t child_count = children.size();
    const size_t gap_count = child_count > 1 ? (child_count - 1) : 0;
    const float total_gap = static_cast<float>(gap_count) * style.gap;

    struct ItemInfo {
        core::Handle<core::Node> handle;
        core::Node* node{nullptr};
        float main_size{0.0f};
        float cross_size{0.0f};
        float grow{0.0f};
        float shrink{1.0f};
    };

    std::vector<ItemInfo> items;
    items.reserve(child_count);

    float total_grow = 0.0f;
    float total_shrink = 0.0f;
    float occupied_main_space = total_gap;
    float max_child_cross = 0.0f;

    for (core::Handle<core::Node> child_handle : children) {
        core::Node* child_node = tree.get(child_handle);
        if (!child_node)
            continue;

        ItemInfo item;
        item.handle = child_handle;
        item.node = child_node;
        item.grow = std::max(0.0f, child_node->flex_item.grow);
        item.shrink = std::max(0.0f, child_node->flex_item.shrink);

        total_grow += item.grow;
        total_shrink += item.shrink;

        if (item.grow == 0.0f) {
            float measured_main = 0.0f;
            float measured_cross = 0.0f;

            if (child_node->flex_item.basis >= 0.0f) {
                measured_main = child_node->flex_item.basis;
                BoxConstraints child_c =
                    is_row
                        ? BoxConstraints{measured_main, measured_main, 0.0f, max_cross_constraint}
                        : BoxConstraints{0.0f, max_cross_constraint, measured_main, measured_main};

                core::Size sz =
                    child_node->layout_fn
                        ? child_node->layout_fn(tree, child_handle, child_c, child_node->user_data)
                        : child_node->layout.size;

                child_node->layout.size = sz;
                measured_cross = is_row ? sz.height : sz.width;
            } else {
                BoxConstraints child_c =
                    is_row ? BoxConstraints{0.0f, std::numeric_limits<float>::infinity(), 0.0f,
                                            max_cross_constraint}
                           : BoxConstraints{0.0f, max_cross_constraint, 0.0f,
                                            std::numeric_limits<float>::infinity()};

                core::Size sz =
                    child_node->layout_fn
                        ? child_node->layout_fn(tree, child_handle, child_c, child_node->user_data)
                        : child_node->layout.size;

                child_node->layout.size = sz;
                measured_main = is_row ? sz.width : sz.height;
                measured_cross = is_row ? sz.height : sz.width;
            }

            item.main_size = measured_main;
            item.cross_size = measured_cross;
            occupied_main_space += measured_main;
            max_child_cross = std::max(max_child_cross, measured_cross);
        } else {
            item.main_size = std::max(0.0f, child_node->flex_item.basis);
            occupied_main_space += item.main_size;
        }

        items.push_back(item);
    }

    float available_main = max_main_constraint < std::numeric_limits<float>::infinity()
                               ? max_main_constraint
                               : occupied_main_space;

    if (available_main < min_main_constraint) {
        available_main = min_main_constraint;
    }

    float free_main_space = available_main - occupied_main_space;

    if (total_grow > 0.0f && free_main_space > 0.0f) {
        for (ItemInfo& item : items) {
            if (item.grow > 0.0f) {
                float share = (item.grow / total_grow) * free_main_space;
                item.main_size += share;

                BoxConstraints child_c = is_row ? BoxConstraints{item.main_size, item.main_size,
                                                                 0.0f, max_cross_constraint}
                                                : BoxConstraints{0.0f, max_cross_constraint,
                                                                 item.main_size, item.main_size};

                core::Size sz =
                    item.node->layout_fn
                        ? item.node->layout_fn(tree, item.handle, child_c, item.node->user_data)
                        : core::Size{is_row ? item.main_size : 0.0f,
                                     is_row ? 0.0f : item.main_size};

                item.node->layout.size = sz;
                item.cross_size = is_row ? sz.height : sz.width;
                max_child_cross = std::max(max_child_cross, item.cross_size);
            }
        }
        occupied_main_space = available_main;
    } else if (total_shrink > 0.0f && free_main_space < 0.0f) {
        float overflow = -free_main_space;
        for (ItemInfo& item : items) {
            if (item.shrink > 0.0f) {
                float shrink_amount = (item.shrink / total_shrink) * overflow;
                item.main_size = std::max(0.0f, item.main_size - shrink_amount);

                BoxConstraints child_c = is_row ? BoxConstraints{item.main_size, item.main_size,
                                                                 0.0f, max_cross_constraint}
                                                : BoxConstraints{0.0f, max_cross_constraint,
                                                                 item.main_size, item.main_size};

                core::Size sz =
                    item.node->layout_fn
                        ? item.node->layout_fn(tree, item.handle, child_c, item.node->user_data)
                        : core::Size{is_row ? item.main_size : 0.0f,
                                     is_row ? 0.0f : item.main_size};

                item.node->layout.size = sz;
                item.cross_size = is_row ? sz.height : sz.width;
                max_child_cross = std::max(max_child_cross, item.cross_size);
            }
        }
        occupied_main_space = available_main;
    }

    float final_main_size =
        std::clamp(occupied_main_space, min_main_constraint, max_main_constraint);
    float final_cross_size =
        std::clamp(max_child_cross, min_cross_constraint, max_cross_constraint);

    float items_total_main = total_gap;
    for (const ItemInfo& item : items) {
        items_total_main += item.main_size;
    }

    float remaining_main = std::max(0.0f, final_main_size - items_total_main);
    float current_main_pos = 0.0f;
    float current_gap = style.gap;

    switch (style.main_align) {
    case MainAxisAlignment::Start:
        current_main_pos = 0.0f;
        break;
    case MainAxisAlignment::Center:
        current_main_pos = remaining_main * 0.5f;
        break;
    case MainAxisAlignment::End:
        current_main_pos = remaining_main;
        break;
    case MainAxisAlignment::SpaceBetween:
        current_main_pos = 0.0f;
        if (gap_count > 0) {
            current_gap = style.gap + (remaining_main / static_cast<float>(gap_count));
        }
        break;
    case MainAxisAlignment::SpaceAround:
        if (!items.empty()) {
            float unit = remaining_main / static_cast<float>(items.size());
            current_main_pos = unit * 0.5f;
            current_gap = style.gap + unit;
        }
        break;
    case MainAxisAlignment::SpaceEvenly:
        if (!items.empty()) {
            float unit = remaining_main / static_cast<float>(items.size() + 1);
            current_main_pos = unit;
            current_gap = style.gap + unit;
        }
        break;
    }

    for (ItemInfo& item : items) {
        float cross_pos = 0.0f;

        switch (style.cross_align) {
        case CrossAxisAlignment::Start:
            cross_pos = 0.0f;
            break;
        case CrossAxisAlignment::Center:
            cross_pos = (final_cross_size - item.cross_size) * 0.5f;
            break;
        case CrossAxisAlignment::End:
            cross_pos = final_cross_size - item.cross_size;
            break;
        case CrossAxisAlignment::Stretch:
            cross_pos = 0.0f;
            item.cross_size = final_cross_size;
            if (is_row) {
                item.node->layout.size.height = final_cross_size;
            } else {
                item.node->layout.size.width = final_cross_size;
            }
            break;
        }

        if (is_row) {
            item.node->layout.offset = core::Point{current_main_pos, cross_pos};
            item.node->layout.size.width = item.main_size;
        } else {
            item.node->layout.offset = core::Point{cross_pos, current_main_pos};
            item.node->layout.size.height = item.main_size;
        }

        current_main_pos += item.main_size + current_gap;
    }

    core::Size result = is_row ? core::Size{final_main_size, final_cross_size}
                               : core::Size{final_cross_size, final_main_size};

    container_node->layout.size = result;
    return result;
}

} // namespace north::layout
