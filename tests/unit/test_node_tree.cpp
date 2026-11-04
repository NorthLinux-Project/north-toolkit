#include "north/core/tree.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace north::core::testing {

struct TestLayoutContext {
    int layout_calls{0};
    Size fixed_size{100.0f, 50.0f};

    static Size layout_callback(NodeTree&, Node::HandleType, const layout::BoxConstraints& c,
                                void* user_data) {
        auto* ctx = static_cast<TestLayoutContext*>(user_data);
        ++ctx->layout_calls;
        return c.constrain(ctx->fixed_size);
    }
};

struct TestPaintRecord {
    Node::HandleType handle;
    Point origin;
};

struct TestPaintContext {
    std::vector<TestPaintRecord> records;

    static void paint_callback(NodeTree&, Node::HandleType handle, Point origin, void*,
                               void* user_data) {
        auto* ctx = static_cast<TestPaintContext*>(user_data);
        ctx->records.push_back({handle, origin});
    }
};

class NodeTreeTestFixture : public ::testing::Test {
protected:
    NodeTree tree;
};

TEST_F(NodeTreeTestFixture, CreateAndManageHierarchy) {
    auto root = tree.create_node();
    auto child1 = tree.create_node();
    auto child2 = tree.create_node();

    tree.set_root(root);
    EXPECT_EQ(tree.root(), root);

    tree.append_child(root, child1);
    tree.append_child(root, child2);

    Node* root_node = tree.get(root);
    ASSERT_NE(root_node, nullptr);
    EXPECT_EQ(root_node->children.size(), 2u);
    EXPECT_EQ(root_node->children[0], child1);
    EXPECT_EQ(root_node->children[1], child2);

    Node* c1_node = tree.get(child1);
    ASSERT_NE(c1_node, nullptr);
    EXPECT_EQ(c1_node->parent, root);

    // Automatic reparenting
    auto new_parent = tree.create_node();
    tree.append_child(new_parent, child1);

    EXPECT_EQ(c1_node->parent, new_parent);
    EXPECT_EQ(root_node->children.size(), 1u);
    EXPECT_EQ(root_node->children[0], child2);
}

TEST_F(NodeTreeTestFixture, DestroyNodeCleansDescendantsRecursively) {
    auto root = tree.create_node();
    auto child = tree.create_node();
    auto grandchild = tree.create_node();

    tree.append_child(root, child);
    tree.append_child(child, grandchild);

    EXPECT_EQ(tree.node_count(), 3u);

    // Destroy child should recursively destroy grandchild and unhook from root
    tree.destroy_node(child);

    EXPECT_EQ(tree.node_count(), 1u);
    EXPECT_EQ(tree.get(child), nullptr);
    EXPECT_EQ(tree.get(grandchild), nullptr);

    Node* root_node = tree.get(root);
    ASSERT_NE(root_node, nullptr);
    EXPECT_TRUE(root_node->children.empty());
}

TEST_F(NodeTreeTestFixture, UpwardBitmaskBubbling) {
    auto root = tree.create_node();
    auto child = tree.create_node();
    auto leaf = tree.create_node();

    tree.set_root(root);
    tree.append_child(root, child);
    tree.append_child(child, leaf);

    // Clear all dirty flags across the tree to test clean state
    tree.get(root)->dirty = kNone;
    tree.get(child)->dirty = kNone;
    tree.get(leaf)->dirty = kNone;

    EXPECT_FALSE(tree.get(root)->is_dirty(kChildren));
    EXPECT_FALSE(tree.get(child)->is_dirty(kChildren));
    EXPECT_FALSE(tree.get(leaf)->is_dirty(kLayout));

    // Mark leaf dirty for layout
    tree.mark_dirty(leaf, kLayout);

    EXPECT_TRUE(tree.get(leaf)->is_dirty(kLayout));
    EXPECT_TRUE(tree.get(child)->is_dirty(kChildren));
    EXPECT_TRUE(tree.get(root)->is_dirty(kChildren));
}

TEST_F(NodeTreeTestFixture, LayoutTraversesOnlyDirtySubtrees) {
    auto root = tree.create_node();
    auto branch1 = tree.create_node();
    auto branch2 = tree.create_node();

    tree.set_root(root);
    tree.append_child(root, branch1);
    tree.append_child(root, branch2);

    TestLayoutContext ctx1;
    TestLayoutContext ctx2;

    tree.get(branch1)->layout_fn = &TestLayoutContext::layout_callback;
    tree.get(branch1)->user_data = &ctx1;

    tree.get(branch2)->layout_fn = &TestLayoutContext::layout_callback;
    tree.get(branch2)->user_data = &ctx2;

    // First layout pass lays out all dirty nodes
    layout::BoxConstraints constraints = layout::BoxConstraints::loose(Size{800.0f, 600.0f});
    tree.layout(constraints);

    EXPECT_EQ(ctx1.layout_calls, 1);
    EXPECT_EQ(ctx2.layout_calls, 1);
    EXPECT_FALSE(tree.get(branch1)->is_dirty(kLayout | kChildren));
    EXPECT_FALSE(tree.get(branch2)->is_dirty(kLayout | kChildren));

    // Second layout pass without dirtying anything skips both branches!
    tree.layout(constraints);
    EXPECT_EQ(ctx1.layout_calls, 1);
    EXPECT_EQ(ctx2.layout_calls, 1);

    // Invalidate branch 1 only
    tree.mark_dirty(branch1, kLayout);
    tree.layout(constraints);

    EXPECT_EQ(ctx1.layout_calls, 2);
    EXPECT_EQ(ctx2.layout_calls, 1); // branch2 skipped!
}

TEST_F(NodeTreeTestFixture, PaintTraversesAndAccumulatesOffsets) {
    auto root = tree.create_node();
    auto child = tree.create_node();

    tree.set_root(root);
    tree.append_child(root, child);

    tree.get(root)->layout.offset = Point{10.0f, 20.0f};
    tree.get(child)->layout.offset = Point{5.0f, 8.0f};

    TestPaintContext p_ctx;
    tree.get(root)->paint_fn = &TestPaintContext::paint_callback;
    tree.get(root)->user_data = &p_ctx;

    tree.get(child)->paint_fn = &TestPaintContext::paint_callback;
    tree.get(child)->user_data = &p_ctx;

    tree.paint(nullptr);

    ASSERT_EQ(p_ctx.records.size(), 2u);
    EXPECT_EQ(p_ctx.records[0].handle, root);
    EXPECT_FLOAT_EQ(p_ctx.records[0].origin.x, 10.0f);
    EXPECT_FLOAT_EQ(p_ctx.records[0].origin.y, 20.0f);

    EXPECT_EQ(p_ctx.records[1].handle, child);
    EXPECT_FLOAT_EQ(p_ctx.records[1].origin.x, 15.0f); // 10 + 5
    EXPECT_FLOAT_EQ(p_ctx.records[1].origin.y, 28.0f); // 20 + 8
}

} // namespace north::core::testing
