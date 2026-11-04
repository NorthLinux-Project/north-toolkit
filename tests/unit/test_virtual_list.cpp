#include "north/core/tree.hpp"
#include "north/event/input_dispatcher.hpp"
#include "north/widget/box.hpp"
#include "north/widget/virtual_list.hpp"

#include <gtest/gtest.h>

using namespace north;

TEST(VirtualListTest, Measures100kItemTotalVirtualHeight) {
    core::NodeTree tree;

    auto list = widget::VirtualListBuilder(tree)
                    .item_count(100000)
                    .item_extent(40.0f)
                    .width(300.0f)
                    .height(400.0f)
                    .build();

    EXPECT_TRUE(list.is_valid());
    tree.set_root(list);
    tree.layout(layout::BoxConstraints::tight({300.0f, 400.0f}));

    auto* node = tree.get(list);
    ASSERT_NE(node, nullptr);
    ASSERT_NE(node->user_data, nullptr);

    auto* data = reinterpret_cast<widget::VirtualListData*>(node->user_data);
    EXPECT_EQ(data->kind, widget::WidgetKind::VirtualList);
    EXPECT_EQ(data->item_count, 100000u);
    EXPECT_FLOAT_EQ(data->fixed_item_extent, 40.0f);
    EXPECT_FLOAT_EQ(node->layout.size.width, 300.0f);
    EXPECT_FLOAT_EQ(node->layout.size.height, 400.0f);
    EXPECT_FLOAT_EQ(data->max_scroll.y, 100000.0f * 40.0f - 400.0f);
}

TEST(VirtualListTest, InstantiatesOnlyVisibleItemWindow) {
    core::NodeTree tree;

    auto list = widget::VirtualListBuilder(tree)
                    .item_count(100000)
                    .item_extent(40.0f)
                    .width(300.0f)
                    .height(400.0f)
                    .item_builder([](core::NodeTree& t, size_t, void*) -> core::Handle<core::Node> {
                        return widget::BoxBuilder(t).height(40.0f).build();
                    })
                    .build();

    tree.set_root(list);
    tree.layout(layout::BoxConstraints::tight({300.0f, 400.0f}));

    auto* node = tree.get(list);
    ASSERT_NE(node, nullptr);

    // Viewport height 400 / item 40 = 10 items + overscan = ~12-14 items
    EXPECT_LE(node->children.size(), 15u);
    EXPECT_GE(node->children.size(), 10u);

    auto* data = reinterpret_cast<widget::VirtualListData*>(node->user_data);
    EXPECT_EQ(data->first_visible_idx, 0u);
    EXPECT_LE(data->last_visible_idx, 12u);
}

TEST(VirtualListTest, WindowShiftsOnScroll) {
    core::NodeTree tree;

    auto list = widget::VirtualListBuilder(tree)
                    .item_count(50000)
                    .item_extent(50.0f)
                    .width(300.0f)
                    .height(500.0f)
                    .scroll_offset(10000.0f) // index 200
                    .item_builder([](core::NodeTree& t, size_t, void*) -> core::Handle<core::Node> {
                        return widget::BoxBuilder(t).height(50.0f).build();
                    })
                    .build();

    tree.set_root(list);
    tree.layout(layout::BoxConstraints::tight({300.0f, 500.0f}));

    auto* node = tree.get(list);
    ASSERT_NE(node, nullptr);

    auto* data = reinterpret_cast<widget::VirtualListData*>(node->user_data);
    // At scroll 10000: index is 200. With 1-item overscan, first should be 199
    EXPECT_GE(data->first_visible_idx, 198u);
    EXPECT_LE(data->first_visible_idx, 200u);
    EXPECT_GE(data->last_visible_idx, 210u);
    EXPECT_LE(data->last_visible_idx, 213u);

    // Children count still remains small window
    EXPECT_LE(node->children.size(), 15u);
}

TEST(VirtualListTest, WheelScrollUpdatesWindow) {
    core::NodeTree tree;

    auto list = widget::VirtualListBuilder(tree)
                    .item_count(1000)
                    .item_extent(50.0f)
                    .width(300.0f)
                    .height(300.0f)
                    .item_builder([](core::NodeTree& t, size_t, void*) -> core::Handle<core::Node> {
                        return widget::BoxBuilder(t).height(50.0f).build();
                    })
                    .build();

    tree.set_root(list);
    tree.layout(layout::BoxConstraints::tight({300.0f, 300.0f}));

    event::InputDispatcher dispatcher(tree);

    // Scroll down by 200px
    dispatcher.on_scroll(0.0f, -200.0f, {50.0f, 50.0f});

    auto* data = reinterpret_cast<widget::VirtualListData*>(tree.get(list)->user_data);
    EXPECT_FLOAT_EQ(data->scroll_offset.y, 200.0f);

    tree.layout(layout::BoxConstraints::tight({300.0f, 300.0f}));
    EXPECT_GE(data->first_visible_idx, 2u);
}
