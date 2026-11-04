#include "north/core/arena.hpp"
#include "north/event/input_dispatcher.hpp"
#include "north/gpu/d3d11/d3d11_device.hpp"
#include "north/render/display_list.hpp"
#include "north/render/renderer.hpp"
#include "north/text/font_manager.hpp"
#include "north/widget/box.hpp"
#include "north/widget/button.hpp"
#include "north/widget/text.hpp"
#include "north/widget/text_input.hpp"

#include <gtest/gtest.h>

namespace north::widget::testing {

class WidgetTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        device = std::make_unique<gpu::D3D11Device>();
        initialized = device->init_offscreen(800, 600);
        arena = std::make_unique<core::Arena>(1024 * 64);
    }

    void TearDown() override {
        arena.reset();
        device.reset();
    }

    std::unique_ptr<gpu::D3D11Device> device;
    std::unique_ptr<core::Arena> arena;
    bool initialized{false};
};

TEST_F(WidgetTestFixture, BoxBuilderHierarchyAndPadding) {
    core::NodeTree tree;

    auto child1 = BoxBuilder(tree)
                      .width(100.0f)
                      .height(50.0f)
                      .background(core::Color{1.0f, 0.0f, 0.0f, 1.0f})
                      .build();

    auto root = BoxBuilder(tree)
                    .padding(core::Edges{10.0f, 20.0f, 10.0f, 20.0f})
                    .background(core::Color{0.1f, 0.1f, 0.1f, 1.0f})
                    .border(core::Color{1.0f, 1.0f, 1.0f, 1.0f}, core::Edges::uniform(2.0f))
                    .corner_radii(core::CornerRadii::uniform(8.0f))
                    .shadow(core::Color{0.0f, 0.0f, 0.0f, 0.5f}, 12.0f)
                    .child(child1)
                    .build();

    tree.set_root(root);
    tree.layout(layout::BoxConstraints::loose({800.0f, 600.0f}));

    auto* r_node = tree.get(root);
    ASSERT_NE(r_node, nullptr);
    EXPECT_FLOAT_EQ(r_node->layout.size.width, 140.0f); // 100 + 20 + 20
    EXPECT_FLOAT_EQ(r_node->layout.size.height, 70.0f); // 50 + 10 + 10

    auto* c1_node = tree.get(child1);
    ASSERT_NE(c1_node, nullptr);
    EXPECT_FLOAT_EQ(c1_node->layout.offset.x, 20.0f);
    EXPECT_FLOAT_EQ(c1_node->layout.offset.y, 10.0f);

    render::DisplayList dl(*arena, core::Rect{0.0f, 0.0f, 800.0f, 600.0f});
    tree.paint(&dl);

    EXPECT_GT(dl.size(), 0u);
}

TEST_F(WidgetTestFixture, BoxBuilderFlexRowLayout) {
    core::NodeTree tree;

    auto c1 = BoxBuilder(tree).width(60.0f).height(40.0f).build();

    auto c2 = BoxBuilder(tree).width(80.0f).height(40.0f).build();

    auto root = BoxBuilder(tree)
                    .flex(layout::FlexDirection::Row, layout::MainAxisAlignment::Start,
                          layout::CrossAxisAlignment::Start, 10.0f)
                    .padding(core::Edges::uniform(15.0f))
                    .child(c1)
                    .child(c2)
                    .build();

    tree.set_root(root);
    tree.layout(layout::BoxConstraints::loose({800.0f, 600.0f}));

    auto* root_node = tree.get(root);
    ASSERT_NE(root_node, nullptr);
    // Width = padding(30) + c1(60) + gap(10) + c2(80) = 180
    EXPECT_FLOAT_EQ(root_node->layout.size.width, 180.0f);
    EXPECT_FLOAT_EQ(root_node->layout.size.height, 70.0f); // 40 + 30
}

TEST_F(WidgetTestFixture, TextBuilderLayoutAndPaint) {
    ASSERT_TRUE(initialized);

    core::NodeTree tree;
    auto text_node = TextBuilder(tree, "North Declarative Widget Text")
                         .font_size(18.0f)
                         .color(core::Color{1.0f, 1.0f, 1.0f, 1.0f})
                         .build();

    tree.set_root(text_node);
    tree.layout(layout::BoxConstraints::loose({800.0f, 600.0f}));

    auto* node = tree.get(text_node);
    ASSERT_NE(node, nullptr);
    EXPECT_GT(node->layout.size.width, 50.0f);
    EXPECT_GT(node->layout.size.height, 10.0f);

    text::GlyphAtlas atlas(*device);
    render::DisplayList dl(*arena, core::Rect{0.0f, 0.0f, 800.0f, 600.0f});
    dl.set_glyph_atlas(&atlas);

    tree.paint(&dl);

    ASSERT_EQ(dl.size(), 1u);
    EXPECT_EQ(dl.commands()[0].type, render::CommandType::GlyphRun);
}

TEST_F(WidgetTestFixture, ButtonClickAndStateTransitions) {
    core::NodeTree tree;

    static int click_count = 0;
    click_count = 0;

    auto btn_node = ButtonBuilder(tree, "Click Me")
                        .padding(core::Edges{8.0f, 16.0f, 8.0f, 16.0f})
                        .on_click([](void*) { ++click_count; })
                        .build();

    tree.set_root(btn_node);
    tree.layout(layout::BoxConstraints::loose({800.0f, 600.0f}));

    auto* node = tree.get(btn_node);
    ASSERT_NE(node, nullptr);
    EXPECT_GT(node->layout.size.width, 20.0f);
    EXPECT_GT(node->layout.size.height, 10.0f);

    auto* btn_data = reinterpret_cast<ButtonData*>(node->user_data);
    ASSERT_NE(btn_data, nullptr);
    EXPECT_EQ(btn_data->state.get(), ButtonState::Normal);

    event::InputDispatcher dispatcher(tree);

    // 1. Move pointer over button center
    core::Point center{node->layout.size.width * 0.5f, node->layout.size.height * 0.5f};
    platform::PointerEvent move_ev{.action = platform::PointerAction::Move, .position = center};
    dispatcher.on_pointer(move_ev);
    EXPECT_EQ(btn_data->state.get(), ButtonState::Hovered);

    // 2. Press left mouse down
    platform::PointerEvent down_ev{.action = platform::PointerAction::Down,
                                   .button = platform::PointerButton::Left,
                                   .position = center};
    dispatcher.on_pointer(down_ev);
    EXPECT_EQ(btn_data->state.get(), ButtonState::Pressed);

    // 3. Release left mouse up
    platform::PointerEvent up_ev{.action = platform::PointerAction::Up,
                                 .button = platform::PointerButton::Left,
                                 .position = center};
    dispatcher.on_pointer(up_ev);
    EXPECT_EQ(btn_data->state.get(), ButtonState::Hovered);
    EXPECT_EQ(click_count, 1);

    // 4. Move pointer outside button bounds
    platform::PointerEvent leave_ev{.action = platform::PointerAction::Move,
                                    .position = core::Point{999.0f, 999.0f}};
    dispatcher.on_pointer(leave_ev);
    EXPECT_EQ(btn_data->state.get(), ButtonState::Normal);
}

TEST_F(WidgetTestFixture, ButtonClickRebuildsFullDemoTree) {
    ASSERT_TRUE(initialized);

    core::NodeTree tree;
    core::Signal<int> counter{0};
    core::Signal<bool> is_dark{true};
    std::string input_text{"North UI Toolkit"};

    auto rebuild = [&]() {
        if (tree.root().is_valid()) {
            tree.destroy_node(tree.root());
        }

        const auto& theme = is_dark.get() ? style::Theme::dark() : style::Theme::light();
        const auto& c = theme.colors();

        std::string count_str = "Count: " + std::to_string(counter.get());
        auto count_text = TextBuilder(tree, count_str).font_size(28.0f).color(c.on_surface).build();

        auto dec_btn = ButtonBuilder(tree, "-")
                           .theme(theme)
                           .padding(core::Edges{10.0f, 20.0f, 10.0f, 20.0f})
                           .on_click(
                               [](void* ud) {
                                   auto* s = static_cast<core::Signal<int>*>(ud);
                                   s->set(s->get() - 1);
                               },
                               &counter)
                           .build();

        auto inc_btn = ButtonBuilder(tree, "+")
                           .theme(theme)
                           .padding(core::Edges{10.0f, 20.0f, 10.0f, 20.0f})
                           .on_click(
                               [](void* ud) {
                                   auto* s = static_cast<core::Signal<int>*>(ud);
                                   s->set(s->get() + 1);
                               },
                               &counter)
                           .build();

        auto theme_btn = ButtonBuilder(tree, is_dark.get() ? "Light Mode" : "Dark Mode")
                             .theme(theme)
                             .padding(core::Edges{10.0f, 16.0f, 10.0f, 16.0f})
                             .build();

        auto button_row = BoxBuilder(tree)
                              .flex(layout::FlexDirection::Row, layout::MainAxisAlignment::Center,
                                    layout::CrossAxisAlignment::Center, 12.0f)
                              .child(dec_btn)
                              .child(inc_btn)
                              .child(theme_btn)
                              .build();

        auto text_input = TextInputBuilder(tree)
                              .theme(theme)
                              .text(input_text)
                              .placeholder("Type here...")
                              .width(200.0f)
                              .height(32.0f)
                              .build();

        auto card =
            BoxBuilder(tree)
                .flex(layout::FlexDirection::Column, layout::MainAxisAlignment::Center,
                      layout::CrossAxisAlignment::Center, 18.0f)
                .padding(core::Edges::uniform(32.0f))
                .background(c.surface)
                .border(c.outline_variant, core::Edges::uniform(1.0f))
                .corner_radii(core::CornerRadii::uniform(12.0f))
                .shadow(core::Color{0.0f, 0.0f, 0.0f, 0.2f}, 12.0f, 0.0f, core::Point{0.0f, 4.0f})
                .child(count_text)
                .child(text_input)
                .child(button_row)
                .build();

        auto root = BoxBuilder(tree)
                        .flex(layout::FlexDirection::Column, layout::MainAxisAlignment::Center,
                              layout::CrossAxisAlignment::Center)
                        .background(c.background)
                        .child(card)
                        .build();

        tree.set_root(root);
    };

    struct RebuildCtx {
        decltype(rebuild)* fn;
    } rebuild_ctx{&rebuild};

    counter.bind_dirty([](void* ud, uint32_t) { (*static_cast<RebuildCtx*>(ud)->fn)(); },
                       &rebuild_ctx);

    rebuild();

    event::InputDispatcher dispatcher(tree);

    core::Size sz{640.0f, 480.0f};
    tree.layout(layout::BoxConstraints::tight(sz));

    render::Renderer renderer(*device);
    ASSERT_TRUE(renderer.init());
    renderer.render_frame(tree, sz, core::Color{0.1f, 0.1f, 0.1f, 1.0f});

    // Find the position of dec_btn to click it
    // Search tree for dec_btn
    core::Point dec_btn_pos{0.0f, 0.0f};
    auto find_dec = [&](auto& self, core::Handle<core::Node> h, core::Point orig) -> bool {
        auto* n = tree.get(h);
        if (!n)
            return false;
        core::Point cur_orig = orig + n->layout.offset;
        if (n->user_data) {
            auto* k = reinterpret_cast<WidgetKind*>(n->user_data);
            if (*k == WidgetKind::Button) {
                auto* b = reinterpret_cast<ButtonData*>(n->user_data);
                if (b->label == "-") {
                    dec_btn_pos = cur_orig + core::Point{n->layout.size.width * 0.5f,
                                                         n->layout.size.height * 0.5f};
                    return true;
                }
            }
        }
        for (auto c : n->children) {
            if (self(self, c, cur_orig))
                return true;
        }
        return false;
    };
    ASSERT_TRUE(find_dec(find_dec, tree.root(), {0.0f, 0.0f}));

    // Click dec_btn
    platform::PointerEvent down_ev{.action = platform::PointerAction::Down,
                                   .button = platform::PointerButton::Left,
                                   .position = dec_btn_pos};
    dispatcher.on_pointer(down_ev);

    platform::PointerEvent up_ev{.action = platform::PointerAction::Up,
                                 .button = platform::PointerButton::Left,
                                 .position = dec_btn_pos};
    dispatcher.on_pointer(up_ev);

    EXPECT_EQ(counter.get(), -1);

    // Frame after click
    tree.layout(layout::BoxConstraints::tight(sz));
    renderer.render_frame(tree, sz, core::Color{0.1f, 0.1f, 0.1f, 1.0f});
}

TEST_F(WidgetTestFixture, RelayoutOnResizeEvent) {
    core::NodeTree tree;
    auto box = BoxBuilder(tree)
                   .flex(layout::FlexDirection::Column, layout::MainAxisAlignment::Center,
                         layout::CrossAxisAlignment::Center)
                   .build();

    tree.set_root(box);
    tree.layout(layout::BoxConstraints::tight({640.0f, 480.0f}));

    auto* node = tree.get(box);
    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->layout.size.width, 640.0f);
    EXPECT_FLOAT_EQ(node->layout.size.height, 480.0f);

    event::InputDispatcher dispatcher(tree);

    // Trigger on_resize to 1024x768
    dispatcher.on_resize(core::Size{1024.0f, 768.0f});

    EXPECT_FLOAT_EQ(node->layout.size.width, 1024.0f);
    EXPECT_FLOAT_EQ(node->layout.size.height, 768.0f);
}

} // namespace north::widget::testing
