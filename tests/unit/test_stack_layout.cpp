#include "north/core/tree.hpp"
#include "north/layout/stack.hpp"

#include <gtest/gtest.h>

namespace north::layout::testing {

namespace {

core::Size fixed_child_layout(core::NodeTree&, core::Handle<core::Node>,
                              const BoxConstraints& constraints, void* user_data) {
    auto* sz = static_cast<core::Size*>(user_data);
    return constraints.constrain(*sz);
}

} // namespace

class StackLayoutTestFixture : public ::testing::Test {
protected:
    core::NodeTree tree;
};

TEST_F(StackLayoutTestFixture, SizesToFitUnpositionedChildren) {
    auto container = tree.create_node();
    auto child1 = tree.create_node();
    auto child2 = tree.create_node();

    tree.set_root(container);
    tree.append_child(container, child1);
    tree.append_child(container, child2);

    core::Size s1{150.0f, 80.0f};
    core::Size s2{200.0f, 60.0f};

    tree.get(child1)->layout_fn = &fixed_child_layout;
    tree.get(child1)->user_data = &s1;

    tree.get(child2)->layout_fn = &fixed_child_layout;
    tree.get(child2)->user_data = &s2;

    BoxConstraints constraints{0.0f, 500.0f, 0.0f, 500.0f};
    StackStyle style{.default_alignment = Alignment::TopLeft};

    core::Size stack_size = stack_layout(tree, container, constraints, style);

    EXPECT_FLOAT_EQ(stack_size.width, 200.0f);
    EXPECT_FLOAT_EQ(stack_size.height, 80.0f);

    EXPECT_FLOAT_EQ(tree.get(child1)->layout.offset.x, 0.0f);
    EXPECT_FLOAT_EQ(tree.get(child1)->layout.offset.y, 0.0f);
    EXPECT_FLOAT_EQ(tree.get(child2)->layout.offset.x, 0.0f);
    EXPECT_FLOAT_EQ(tree.get(child2)->layout.offset.y, 0.0f);
}

TEST_F(StackLayoutTestFixture, AlignsUnpositionedChildToCenter) {
    auto container = tree.create_node();
    auto child = tree.create_node();

    tree.set_root(container);
    tree.append_child(container, child);

    core::Size child_sz{80.0f, 40.0f};
    tree.get(child)->layout_fn = &fixed_child_layout;
    tree.get(child)->user_data = &child_sz;

    BoxConstraints constraints = BoxConstraints::tight(core::Size{300.0f, 200.0f});
    StackStyle style{.default_alignment = Alignment::Center};

    core::Size stack_size = stack_layout(tree, container, constraints, style);

    EXPECT_FLOAT_EQ(stack_size.width, 300.0f);
    EXPECT_FLOAT_EQ(stack_size.height, 200.0f);

    // Offset = ((300 - 80) / 2, (200 - 40) / 2) = (110, 80)
    EXPECT_FLOAT_EQ(tree.get(child)->layout.offset.x, 110.0f);
    EXPECT_FLOAT_EQ(tree.get(child)->layout.offset.y, 80.0f);
}

TEST_F(StackLayoutTestFixture, PositionedPinningToAllEdges) {
    auto container = tree.create_node();
    auto background_pinned = tree.create_node();

    tree.set_root(container);
    tree.append_child(container, background_pinned);

    // Pin 10px from each edge of a 400x300 container
    tree.get(background_pinned)->positioned =
        PositionedData{.top = 10.0f, .right = 10.0f, .bottom = 10.0f, .left = 10.0f};

    BoxConstraints constraints = BoxConstraints::tight(core::Size{400.0f, 300.0f});
    StackStyle style{.default_alignment = Alignment::TopLeft};

    core::Size stack_size = stack_layout(tree, container, constraints, style);
    EXPECT_FLOAT_EQ(stack_size.width, 400.0f);
    EXPECT_FLOAT_EQ(stack_size.height, 300.0f);

    core::Node* pinned_node = tree.get(background_pinned);
    ASSERT_NE(pinned_node, nullptr);

    EXPECT_FLOAT_EQ(pinned_node->layout.offset.x, 10.0f);
    EXPECT_FLOAT_EQ(pinned_node->layout.offset.y, 10.0f);
    EXPECT_FLOAT_EQ(pinned_node->layout.size.width, 380.0f);  // 400 - 10 - 10
    EXPECT_FLOAT_EQ(pinned_node->layout.size.height, 280.0f); // 300 - 10 - 10
}

TEST_F(StackLayoutTestFixture, PositionedExplicitLeftTopAndSize) {
    auto container = tree.create_node();
    auto overlay = tree.create_node();

    tree.set_root(container);
    tree.append_child(container, overlay);

    tree.get(overlay)->positioned =
        PositionedData{.top = 25.0f, .left = 50.0f, .width = 120.0f, .height = 60.0f};

    BoxConstraints constraints = BoxConstraints::tight(core::Size{500.0f, 400.0f});
    StackStyle style{};

    stack_layout(tree, container, constraints, style);

    core::Node* node = tree.get(overlay);
    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->layout.offset.x, 50.0f);
    EXPECT_FLOAT_EQ(node->layout.offset.y, 25.0f);
    EXPECT_FLOAT_EQ(node->layout.size.width, 120.0f);
    EXPECT_FLOAT_EQ(node->layout.size.height, 60.0f);
}

} // namespace north::layout::testing
