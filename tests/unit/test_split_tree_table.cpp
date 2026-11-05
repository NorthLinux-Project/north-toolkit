#include <gtest/gtest.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "north/core/tree.hpp"
#include "north/event/input_dispatcher.hpp"
#include "north/widget/box.hpp"
#include "north/widget/data_table.hpp"
#include "north/widget/split_view.hpp"
#include "north/widget/tree_view.hpp"

using namespace north;

TEST(SplitViewTest, HorizontalAndVerticalLayoutWithRatio) {
    core::NodeTree tree;

    auto p1 = widget::BoxBuilder(tree).build();
    auto p2 = widget::BoxBuilder(tree).build();

    auto sv = widget::SplitViewBuilder(tree)
                  .orientation(widget::SplitOrientation::Horizontal)
                  .sizing_mode(widget::SplitSizingMode::Ratio)
                  .split(0.4f)
                  .sash_thickness(10.0f)
                  .first(p1)
                  .second(p2)
                  .width(500.0f)
                  .height(300.0f)
                  .build();

    tree.set_root(sv);
    tree.layout(layout::BoxConstraints::tight({500.0f, 300.0f}));

    auto* sv_node = tree.get(sv);
    ASSERT_NE(sv_node, nullptr);
    EXPECT_FLOAT_EQ(sv_node->layout.size.width, 500.0f);
    EXPECT_FLOAT_EQ(sv_node->layout.size.height, 300.0f);

    auto* n1 = tree.get(p1);
    auto* n2 = tree.get(p2);
    ASSERT_NE(n1, nullptr);
    ASSERT_NE(n2, nullptr);

    float avail = 500.0f - 10.0f;
    EXPECT_FLOAT_EQ(n1->layout.size.width, avail * 0.4f);
    EXPECT_FLOAT_EQ(n1->layout.size.height, 300.0f);
    EXPECT_FLOAT_EQ(n2->layout.size.width, avail * 0.6f);
    EXPECT_FLOAT_EQ(n2->layout.offset.x, avail * 0.4f + 10.0f);

    auto* sv_data = reinterpret_cast<widget::SplitViewData*>(sv_node->user_data);
    EXPECT_FLOAT_EQ(sv_data->sash_rect.x, avail * 0.4f);
    EXPECT_FLOAT_EQ(sv_data->sash_rect.width, 10.0f);
}

TEST(SplitViewTest, FixedSizingAndClamping) {
    core::NodeTree tree;

    auto p1 = widget::BoxBuilder(tree).build();
    auto p2 = widget::BoxBuilder(tree).build();

    auto sv = widget::SplitViewBuilder(tree)
                  .orientation(widget::SplitOrientation::Horizontal)
                  .sizing_mode(widget::SplitSizingMode::FixedFirst)
                  .split(200.0f)
                  .min_first_size(100.0f)
                  .max_first_size(250.0f)
                  .min_second_size(50.0f)
                  .sash_thickness(6.0f)
                  .first(p1)
                  .second(p2)
                  .width(400.0f)
                  .height(200.0f)
                  .build();

    tree.set_root(sv);
    tree.layout(layout::BoxConstraints::tight({400.0f, 200.0f}));

    auto* n1 = tree.get(p1);
    auto* n2 = tree.get(p2);
    ASSERT_NE(n1, nullptr);
    ASSERT_NE(n2, nullptr);

    EXPECT_FLOAT_EQ(n1->layout.size.width, 200.0f);
    EXPECT_FLOAT_EQ(n2->layout.size.width, 400.0f - 6.0f - 200.0f);

    auto* sv_node = tree.get(sv);
    auto* sv_data = reinterpret_cast<widget::SplitViewData*>(sv_node->user_data);

    sv_data->set_split(30.0f);
    tree.layout(layout::BoxConstraints::tight({400.0f, 200.0f}));
    EXPECT_FLOAT_EQ(n1->layout.size.width, 100.0f);

    sv_data->reset_split();
    tree.layout(layout::BoxConstraints::tight({400.0f, 200.0f}));
    EXPECT_FLOAT_EQ(n1->layout.size.width, 200.0f);
}

TEST(TreeViewTest, HierarchyProjectionAndExpandCollapse) {
    core::NodeTree tree;

    widget::TreeNodeItem r1;
    r1.id = 1;
    r1.label = "src";
    r1.expandable = true;
    r1.expanded = true;

    widget::TreeNodeItem c1;
    c1.id = 10;
    c1.label = "main.cpp";
    c1.expandable = false;

    widget::TreeNodeItem c2;
    c2.id = 11;
    c2.label = "utils";
    c2.expandable = true;
    c2.expanded = false;

    widget::TreeNodeItem g1;
    g1.id = 100;
    g1.label = "helper.hpp";

    c2.children.push_back(g1);
    r1.children.push_back(c1);
    r1.children.push_back(c2);

    widget::TreeNodeItem r2;
    r2.id = 2;
    r2.label = "README.md";

    auto tv = widget::TreeViewBuilder(tree)
                  .root_item(r1)
                  .root_item(r2)
                  .selected_id(10)
                  .row_height(24.0f)
                  .width(200.0f)
                  .height(300.0f)
                  .build();

    tree.set_root(tv);
    tree.layout(layout::BoxConstraints::tight({200.0f, 300.0f}));

    auto* tv_node = tree.get(tv);
    ASSERT_NE(tv_node, nullptr);
    auto* tv_data = reinterpret_cast<widget::TreeViewData*>(tv_node->user_data);
    ASSERT_NE(tv_data, nullptr);

    EXPECT_EQ(tv_data->visible_rows.size(), 4u);
    EXPECT_EQ(tv_data->visible_rows[0].item->id, 1u);
    EXPECT_EQ(tv_data->visible_rows[0].depth, 0u);
    EXPECT_EQ(tv_data->visible_rows[1].item->id, 10u);
    EXPECT_EQ(tv_data->visible_rows[1].depth, 1u);
    EXPECT_EQ(tv_data->visible_rows[2].item->id, 11u);
    EXPECT_EQ(tv_data->visible_rows[2].depth, 1u);
    EXPECT_EQ(tv_data->visible_rows[3].item->id, 2u);
    EXPECT_EQ(tv_data->visible_rows[3].depth, 0u);

    tv_data->toggle_node(11);
    EXPECT_EQ(tv_data->visible_rows.size(), 5u);
    EXPECT_EQ(tv_data->visible_rows[3].item->id, 100u);
    EXPECT_EQ(tv_data->visible_rows[3].depth, 2u);

    tv_data->toggle_node(1);
    EXPECT_EQ(tv_data->visible_rows.size(), 2u);
    EXPECT_EQ(tv_data->visible_rows[0].item->id, 1u);
    EXPECT_EQ(tv_data->visible_rows[1].item->id, 2u);
}

TEST(TreeViewTest, SelectionAndScrollIntoView) {
    core::NodeTree tree;

    std::vector<widget::TreeNodeItem> items;
    for (uint32_t i = 1; i <= 20; ++i) {
        widget::TreeNodeItem it;
        it.id = i;
        it.label = "Item " + std::to_string(i);
        items.push_back(it);
    }

    uint32_t selected_notified = 0;
    auto tv = widget::TreeViewBuilder(tree)
                  .roots(items)
                  .row_height(20.0f)
                  .width(150.0f)
                  .height(100.0f)
                  .on_select([](uint32_t id, void* ud) {
                      *static_cast<uint32_t*>(ud) = id;
                  }, &selected_notified)
                  .build();

    tree.set_root(tv);
    tree.layout(layout::BoxConstraints::tight({150.0f, 100.0f}));

    auto* tv_node = tree.get(tv);
    auto* tv_data = reinterpret_cast<widget::TreeViewData*>(tv_node->user_data);

    tv_data->select_node(5);
    EXPECT_EQ(tv_data->selected_id, 5u);
    EXPECT_EQ(selected_notified, 5u);

    tv_data->scroll_into_view(15, 100.0f);
    EXPECT_GT(tv_data->scroll_y, 0.0f);
    EXPECT_LE(tv_data->scroll_y, tv_data->max_scroll_y);
}

TEST(DataTableTest, ColumnLayoutAndFlexDistribution) {
    core::NodeTree tree;

    auto dt = widget::DataTableBuilder(tree)
                  .column("ID", 60.0f, text::TextAlign::Right, true, false)
                  .column("Name", 120.0f, text::TextAlign::Left, true, true)
                  .flex_column("Description", 2.0f, 100.0f)
                  .flex_column("Status", 1.0f, 60.0f)
                  .width(600.0f)
                  .height(400.0f)
                  .build();

    tree.set_root(dt);
    tree.layout(layout::BoxConstraints::tight({600.0f, 400.0f}));

    auto* dt_node = tree.get(dt);
    ASSERT_NE(dt_node, nullptr);
    auto* dt_data = reinterpret_cast<widget::DataTableData*>(dt_node->user_data);
    ASSERT_NE(dt_data, nullptr);

    EXPECT_FLOAT_EQ(dt_data->columns[0].computed_width, 60.0f);
    EXPECT_FLOAT_EQ(dt_data->columns[1].computed_width, 120.0f);

    float flex_avail = 600.0f - 180.0f;
    EXPECT_FLOAT_EQ(dt_data->columns[2].computed_width, flex_avail * (2.0f / 3.0f));
    EXPECT_FLOAT_EQ(dt_data->columns[3].computed_width, flex_avail * (1.0f / 3.0f));
}

TEST(DataTableTest, SortingAndRowSelection) {
    core::NodeTree tree;

    size_t sorted_col = SIZE_MAX;
    widget::SortDirection sorted_dir = widget::SortDirection::None;

    auto dt = widget::DataTableBuilder(tree)
                  .column("ID", 50.0f)
                  .column("Score", 80.0f)
                  .row(1, {"101", "45.5"})
                  .row(2, {"102", "120.0"})
                  .row(3, {"103", "12.3"})
                  .on_sort([](size_t col, widget::SortDirection dir, void* ud) {
                      auto* p = static_cast<std::pair<size_t*, widget::SortDirection*>*>(ud);
                      *p->first = col;
                      *p->second = dir;
                  }, new std::pair<size_t*, widget::SortDirection*>(&sorted_col, &sorted_dir))
                  .width(300.0f)
                  .height(200.0f)
                  .build();

    tree.set_root(dt);
    tree.layout(layout::BoxConstraints::tight({300.0f, 200.0f}));

    auto* dt_node = tree.get(dt);
    auto* dt_data = reinterpret_cast<widget::DataTableData*>(dt_node->user_data);

    dt_data->cycle_sort(1);
    EXPECT_EQ(dt_data->sort_direction, widget::SortDirection::Ascending);
    EXPECT_EQ(dt_data->sorted_indices[0], 2u);
    EXPECT_EQ(dt_data->sorted_indices[1], 0u);
    EXPECT_EQ(dt_data->sorted_indices[2], 1u);

    dt_data->cycle_sort(1);
    EXPECT_EQ(dt_data->sort_direction, widget::SortDirection::Descending);
    EXPECT_EQ(dt_data->sorted_indices[0], 1u);
    EXPECT_EQ(dt_data->sorted_indices[1], 0u);
    EXPECT_EQ(dt_data->sorted_indices[2], 2u);

    dt_data->cycle_sort(1);
    EXPECT_EQ(dt_data->sort_direction, widget::SortDirection::None);
    EXPECT_EQ(dt_data->sorted_indices[0], 0u);
    EXPECT_EQ(dt_data->sorted_indices[1], 1u);
    EXPECT_EQ(dt_data->sorted_indices[2], 2u);

    dt_data->select_row(1);
    EXPECT_EQ(dt_data->selected_row, 1u);
}

TEST(InputDispatcherTest, SplitViewAndTreeKeyboardRouting) {
    core::NodeTree tree;
    event::InputDispatcher dispatcher(tree);

    widget::TreeNodeItem root;
    root.id = 1;
    root.label = "Root";
    root.expandable = true;
    root.expanded = false;

    widget::TreeNodeItem ch;
    ch.id = 2;
    ch.label = "Child";
    root.children.push_back(ch);

    auto tv = widget::TreeViewBuilder(tree)
                  .root_item(root)
                  .selected_id(1)
                  .width(200.0f)
                  .height(300.0f)
                  .build();

    tree.set_root(tv);
    tree.layout(layout::BoxConstraints::tight({200.0f, 300.0f}));

    dispatcher.set_focus(tv);

    platform::KeyEvent right_key;
    right_key.is_down = true;
    right_key.key_code = VK_RIGHT;
    dispatcher.on_key(right_key);

    auto* tv_node = tree.get(tv);
    auto* tv_data = reinterpret_cast<widget::TreeViewData*>(tv_node->user_data);
    EXPECT_TRUE(tv_data->roots[0].expanded);
    EXPECT_EQ(tv_data->visible_rows.size(), 2u);

    platform::KeyEvent down_key;
    down_key.is_down = true;
    down_key.key_code = VK_DOWN;
    dispatcher.on_key(down_key);

    EXPECT_EQ(tv_data->selected_id, 2u);

    platform::KeyEvent left_key;
    left_key.is_down = true;
    left_key.key_code = VK_LEFT;
    dispatcher.on_key(left_key);

    EXPECT_EQ(tv_data->selected_id, 1u);
}
