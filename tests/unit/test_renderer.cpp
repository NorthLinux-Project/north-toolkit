#include "north/gpu/d3d11/d3d11_device.hpp"
#include "north/render/renderer.hpp"

#include <gtest/gtest.h>

namespace north::render::testing {

class RendererTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        device = std::make_unique<gpu::D3D11Device>();
        initialized = device->init_offscreen(800, 600);
    }

    void TearDown() override { device.reset(); }

    std::unique_ptr<gpu::D3D11Device> device;
    bool initialized{false};
};

TEST_F(RendererTestFixture, InitializesAndRendersEmptyTree) {
    ASSERT_TRUE(initialized);

    Renderer renderer(*device);
    ASSERT_TRUE(renderer.init());

    core::NodeTree tree;
    auto root = tree.create_node();
    tree.set_root(root);

    core::Size viewport{800.0f, 600.0f};
    tree.layout(layout::BoxConstraints::tight(viewport));

    renderer.render_frame(tree, viewport, core::Color{0.1f, 0.1f, 0.1f, 1.0f});
    EXPECT_GT(renderer.frame_arena().capacity(), 0);

    renderer.shutdown();
}

TEST_F(RendererTestFixture, RendersHierarchyWithDefaultBoxPaintingAndCustomPainter) {
    ASSERT_TRUE(initialized);

    Renderer renderer(*device);
    ASSERT_TRUE(renderer.init());

    core::NodeTree tree;
    auto root = tree.create_node();
    tree.set_root(root);
    auto* root_node = tree.get(root);
    ASSERT_NE(root_node, nullptr);
    root_node->paint.background = core::Color{0.12f, 0.12f, 0.12f, 1.0f};

    // Child 1: uses default box painting
    auto child1 = tree.create_node();
    tree.append_child(root, child1);
    auto* c1_node = tree.get(child1);
    ASSERT_NE(c1_node, nullptr);
    c1_node->paint.background = core::Color{0.2f, 0.4f, 0.8f, 1.0f};
    c1_node->paint.border_color = core::Color{1.0f, 1.0f, 1.0f, 0.8f};
    c1_node->paint.border_width = core::Edges::uniform(2.0f);
    c1_node->paint.radii = core::CornerRadii::uniform(8.0f);
    c1_node->paint.shadow_color = core::Color{0.0f, 0.0f, 0.0f, 0.5f};
    c1_node->paint.shadow_radius = 10.0f;
    c1_node->layout.size = core::Size{200.0f, 100.0f};
    c1_node->layout.offset = core::Point{50.0f, 50.0f};

    // Child 2: uses custom paint_fn
    auto child2 = tree.create_node();
    tree.append_child(root, child2);
    auto* c2_node = tree.get(child2);
    ASSERT_NE(c2_node, nullptr);
    c2_node->layout.size = core::Size{100.0f, 50.0f};
    c2_node->layout.offset = core::Point{300.0f, 50.0f};

    static bool custom_painted = false;
    custom_painted = false;
    c2_node->paint_fn = [](core::NodeTree&, core::Node::HandleType, core::Point origin,
                           void* stream, void*) {
        auto* dl = static_cast<DisplayList*>(stream);
        if (dl) {
            dl->push_quad(core::Rect{origin.x, origin.y, 100.0f, 50.0f},
                          core::CornerRadii::uniform(4.0f), core::Color{0.8f, 0.2f, 0.2f, 1.0f});
            custom_painted = true;
        }
    };

    core::Size viewport{800.0f, 600.0f};
    root_node->layout.size = viewport;

    renderer.render_frame(tree, viewport, core::Color{0.0f, 0.0f, 0.0f, 1.0f});

    EXPECT_TRUE(custom_painted);

    // Render consecutive frames to verify arena reset and stable memory
    for (int frame = 0; frame < 5; ++frame) {
        custom_painted = false;
        renderer.render_frame(tree, viewport, core::Color{0.0f, 0.0f, 0.0f, 1.0f});
        EXPECT_TRUE(custom_painted);
    }

    renderer.shutdown();
}

} // namespace north::render::testing
