#include "north/core/tree.hpp"
#include "north/layout/flex.hpp"

#include <gtest/gtest.h>

namespace north::layout::testing {

namespace {

core::Size fixed_child_layout(core::NodeTree&, core::Handle<core::Node>,
                              const BoxConstraints& constraints, void* user_data) {
    auto* sz = static_cast<core::Size*>(user_data);
    return constraints.constrain(*sz);
}

} // namespace

class FlexLayoutTestFixture : public ::testing::Test {
protected:
    core::NodeTree tree;
};

TEST_F(FlexLayoutTestFixture, RowWithFixedChildrenAndGap) {
    auto container = tree.create_node();
    auto child1 = tree.create_node();
    auto child2 = tree.create_node();
    auto child3 = tree.create_node();

    tree.set_root(container);
    tree.append_child(container, child1);
    tree.append_child(container, child2);
    tree.append_child(container, child3);

    core::Size s1{100.0f, 40.0f};
    core::Size s2{150.0f, 60.0f};
    core::Size s3{80.0f, 50.0f};

    tree.get(child1)->layout_fn = &fixed_child_layout;
    tree.get(child1)->user_data = &s1;

    tree.get(child2)->layout_fn = &fixed_child_layout;
    tree.get(child2)->user_data = &s2;

    tree.get(child3)->layout_fn = &fixed_child_layout;
    tree.get(child3)->user_data = &s3;

    FlexStyle style{.direction = FlexDirection::Row,
                    .main_align = MainAxisAlignment::Start,
                    .cross_align = CrossAxisAlignment::Start,
                    .gap = 10.0f};

    BoxConstraints constraints{0.0f, 1000.0f, 0.0f, 1000.0f};
    core::Size container_size = flex_layout(tree, container, constraints, style);

    // Total width = 100 + 150 + 80 + (2 * 10) = 350
    // Max height = 60
    EXPECT_FLOAT_EQ(container_size.width, 350.0f);
    EXPECT_FLOAT_EQ(container_size.height, 60.0f);

    EXPECT_FLOAT_EQ(tree.get(child1)->layout.offset.x, 0.0f);
    EXPECT_FLOAT_EQ(tree.get(child2)->layout.offset.x, 110.0f);
    EXPECT_FLOAT_EQ(tree.get(child3)->layout.offset.x, 270.0f);
}

TEST_F(FlexLayoutTestFixture, ColumnWithFixedChildrenAndGap) {
    auto container = tree.create_node();
    auto child1 = tree.create_node();
    auto child2 = tree.create_node();

    tree.set_root(container);
    tree.append_child(container, child1);
    tree.append_child(container, child2);

    core::Size s1{200.0f, 50.0f};
    core::Size s2{180.0f, 70.0f};

    tree.get(child1)->layout_fn = &fixed_child_layout;
    tree.get(child1)->user_data = &s1;

    tree.get(child2)->layout_fn = &fixed_child_layout;
    tree.get(child2)->user_data = &s2;

    FlexStyle style{.direction = FlexDirection::Column,
                    .main_align = MainAxisAlignment::Start,
                    .cross_align = CrossAxisAlignment::Start,
                    .gap = 15.0f};

    BoxConstraints constraints{0.0f, 1000.0f, 0.0f, 1000.0f};
    core::Size container_size = flex_layout(tree, container, constraints, style);

    // Total height = 50 + 70 + 15 = 135
    // Max width = 200
    EXPECT_FLOAT_EQ(container_size.height, 135.0f);
    EXPECT_FLOAT_EQ(container_size.width, 200.0f);

    EXPECT_FLOAT_EQ(tree.get(child1)->layout.offset.y, 0.0f);
    EXPECT_FLOAT_EQ(tree.get(child2)->layout.offset.y, 65.0f);
}

TEST_F(FlexLayoutTestFixture, FlexGrowExpandsRemainingSpaceProportionally) {
    auto container = tree.create_node();
    auto fixed_child = tree.create_node();
    auto flex_child1 = tree.create_node();
    auto flex_child2 = tree.create_node();

    tree.set_root(container);
    tree.append_child(container, fixed_child);
    tree.append_child(container, flex_child1);
    tree.append_child(container, flex_child2);

    core::Size fixed_sz{100.0f, 40.0f};
    tree.get(fixed_child)->layout_fn = &fixed_child_layout;
    tree.get(fixed_child)->user_data = &fixed_sz;

    tree.get(flex_child1)->flex_item.grow = 1.0f;
    tree.get(flex_child2)->flex_item.grow = 2.0f;

    FlexStyle style{.direction = FlexDirection::Row,
                    .main_align = MainAxisAlignment::Start,
                    .cross_align = CrossAxisAlignment::Start,
                    .gap = 0.0f};

    // Container constrained tightly to 400px width
    BoxConstraints constraints = BoxConstraints::tight(core::Size{400.0f, 100.0f});
    core::Size result = flex_layout(tree, container, constraints, style);

    EXPECT_FLOAT_EQ(result.width, 400.0f);

    // Remaining space = 400 - 100 = 300px
    // flex_child1 (grow 1) gets 100px -> width 100
    // flex_child2 (grow 2) gets 200px -> width 200
    EXPECT_FLOAT_EQ(tree.get(fixed_child)->layout.size.width, 100.0f);
    EXPECT_FLOAT_EQ(tree.get(flex_child1)->layout.size.width, 100.0f);
    EXPECT_FLOAT_EQ(tree.get(flex_child2)->layout.size.width, 200.0f);

    EXPECT_FLOAT_EQ(tree.get(fixed_child)->layout.offset.x, 0.0f);
    EXPECT_FLOAT_EQ(tree.get(flex_child1)->layout.offset.x, 100.0f);
    EXPECT_FLOAT_EQ(tree.get(flex_child2)->layout.offset.x, 200.0f);
}

TEST_F(FlexLayoutTestFixture, MainAxisAlignmentCenterAndEnd) {
    auto container = tree.create_node();
    auto child = tree.create_node();

    tree.set_root(container);
    tree.append_child(container, child);

    core::Size sz{100.0f, 50.0f};
    tree.get(child)->layout_fn = &fixed_child_layout;
    tree.get(child)->user_data = &sz;

    BoxConstraints constraints = BoxConstraints::tight(core::Size{300.0f, 100.0f});

    // Center
    FlexStyle style_center{.direction = FlexDirection::Row,
                           .main_align = MainAxisAlignment::Center};
    flex_layout(tree, container, constraints, style_center);
    EXPECT_FLOAT_EQ(tree.get(child)->layout.offset.x, 100.0f); // (300 - 100) / 2

    // End
    FlexStyle style_end{.direction = FlexDirection::Row, .main_align = MainAxisAlignment::End};
    flex_layout(tree, container, constraints, style_end);
    EXPECT_FLOAT_EQ(tree.get(child)->layout.offset.x, 200.0f); // 300 - 100
}

TEST_F(FlexLayoutTestFixture, CrossAxisAlignmentCenterAndStretch) {
    auto container = tree.create_node();
    auto child = tree.create_node();

    tree.set_root(container);
    tree.append_child(container, child);

    core::Size sz{100.0f, 40.0f};
    tree.get(child)->layout_fn = &fixed_child_layout;
    tree.get(child)->user_data = &sz;

    BoxConstraints constraints = BoxConstraints::tight(core::Size{200.0f, 100.0f});

    // Cross Center
    FlexStyle style_center{.direction = FlexDirection::Row,
                           .cross_align = CrossAxisAlignment::Center};
    flex_layout(tree, container, constraints, style_center);
    EXPECT_FLOAT_EQ(tree.get(child)->layout.offset.y, 30.0f); // (100 - 40) / 2

    // Cross Stretch
    FlexStyle style_stretch{.direction = FlexDirection::Row,
                            .cross_align = CrossAxisAlignment::Stretch};
    flex_layout(tree, container, constraints, style_stretch);
    EXPECT_FLOAT_EQ(tree.get(child)->layout.offset.y, 0.0f);
    EXPECT_FLOAT_EQ(tree.get(child)->layout.size.height, 100.0f);
}

} // namespace north::layout::testing
