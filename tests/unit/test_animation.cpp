#include "north/animation/animated_value.hpp"
#include "north/animation/animation_manager.hpp"
#include "north/animation/curve.hpp"
#include "north/animation/interpolator.hpp"
#include "north/core/tree.hpp"
#include "north/event/input_dispatcher.hpp"
#include "north/widget/box.hpp"
#include "north/widget/button.hpp"
#include "north/widget/scroll_area.hpp"
#include "north/widget/virtual_list.hpp"

#include <gtest/gtest.h>

#include <cmath>

using namespace north;

TEST(AnimationTest, CubicBezierPresetsAndBoundaries) {
    auto linear = animation::CubicBezier::linear();
    EXPECT_FLOAT_EQ(linear.solve(0.0f), 0.0f);
    EXPECT_NEAR(linear.solve(0.5f), 0.5f, 1e-4f);
    EXPECT_FLOAT_EQ(linear.solve(1.0f), 1.0f);

    auto ease_in = animation::CubicBezier::ease_in();
    EXPECT_FLOAT_EQ(ease_in.solve(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(ease_in.solve(1.0f), 1.0f);
    EXPECT_LT(ease_in.solve(0.5f), 0.5f);

    auto ease_out = animation::CubicBezier::ease_out();
    EXPECT_FLOAT_EQ(ease_out.solve(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(ease_out.solve(1.0f), 1.0f);
    EXPECT_GT(ease_out.solve(0.5f), 0.5f);

    auto ease_in_out = animation::CubicBezier::ease_in_out();
    EXPECT_FLOAT_EQ(ease_in_out.solve(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(ease_in_out.solve(1.0f), 1.0f);
    EXPECT_NEAR(ease_in_out.solve(0.5f), 0.5f, 1e-3f);

    // Clamping out-of-range inputs
    EXPECT_FLOAT_EQ(ease_in.solve(-0.5f), 0.0f);
    EXPECT_FLOAT_EQ(ease_in.solve(1.5f), 1.0f);
}

TEST(AnimationTest, SpringCriticallyDampedConvergence) {
    // Zeta = c / (2 * sqrt(k * m))
    // For k = 100, m = 1: c = 20 gives zeta = 1.0 (critically damped)
    animation::SpringDesc desc{.stiffness = 100.0f, .damping = 20.0f, .mass = 1.0f};

    float pos = 0.0f;
    float vel = 0.0f;
    float target = 100.0f;

    // Step in small time steps towards target
    float dt = 0.016f;
    for (int i = 0; i < 60; ++i) {
        animation::step_spring(pos, vel, target, dt, desc);
        // Critically damped spring should monotonically approach target without overshoot
        EXPECT_LE(pos, target + 0.01f);
    }

    // After ~1 second, should have settled near target
    EXPECT_NEAR(pos, target, 0.5f);
    EXPECT_NEAR(vel, 0.0f, 1.0f);
}

TEST(AnimationTest, SpringUnderDampedOscillationAndDecay) {
    // Zeta < 1: under-damped spring exhibits harmonic oscillation with exponential decay
    animation::SpringDesc desc{.stiffness = 200.0f, .damping = 5.0f, .mass = 1.0f};

    float pos = 0.0f;
    float vel = 0.0f;
    float target = 50.0f;

    float dt = 0.016f;
    bool overshot = false;

    for (int i = 0; i < 180; ++i) {
        animation::step_spring(pos, vel, target, dt, desc);
        if (pos > target) {
            overshot = true;
        }
    }

    // Under-damped spring must have overshot the target at least once
    EXPECT_TRUE(overshot);

    // And after 3 seconds, the decay envelope settles close to target
    EXPECT_NEAR(pos, target, 0.5f);
}

TEST(AnimationTest, InterpolatorSpecializations) {
    // Float
    EXPECT_FLOAT_EQ(animation::Interpolator<float>::lerp(0.0f, 10.0f, 0.5f), 5.0f);
    EXPECT_FLOAT_EQ(animation::Interpolator<float>::distance(2.0f, 5.0f), 3.0f);

    // Point
    core::Point p1{0.0f, 0.0f};
    core::Point p2{10.0f, 20.0f};
    core::Point p_mid = animation::Interpolator<core::Point>::lerp(p1, p2, 0.5f);
    EXPECT_FLOAT_EQ(p_mid.x, 5.0f);
    EXPECT_FLOAT_EQ(p_mid.y, 10.0f);

    // Size
    core::Size s1{100.0f, 200.0f};
    core::Size s2{200.0f, 400.0f};
    core::Size s_mid = animation::Interpolator<core::Size>::lerp(s1, s2, 0.5f);
    EXPECT_FLOAT_EQ(s_mid.width, 150.0f);
    EXPECT_FLOAT_EQ(s_mid.height, 300.0f);

    // Color (premultiplied linear blend)
    core::Color c1{1.0f, 0.0f, 0.0f, 1.0f};
    core::Color c2{0.0f, 1.0f, 0.0f, 1.0f};
    core::Color c_mid = animation::Interpolator<core::Color>::lerp(c1, c2, 0.5f);
    EXPECT_NEAR(c_mid.r, 0.5f, 1e-4f);
    EXPECT_NEAR(c_mid.g, 0.5f, 1e-4f);
    EXPECT_NEAR(c_mid.b, 0.0f, 1e-4f);
    EXPECT_NEAR(c_mid.a, 1.0f, 1e-4f);
}

TEST(AnimationTest, AnimatedValueSpringConvergence) {
    auto& mgr = animation::AnimationManager::instance();
    mgr.clear();

    animation::AnimatedValue<float> anim(0.0f);
    EXPECT_FALSE(anim.is_animating());
    EXPECT_FLOAT_EQ(anim.get(), 0.0f);
    EXPECT_EQ(mgr.active_count(), 0u);

    anim.spring_to(100.0f, animation::SpringDesc::snappy());
    EXPECT_TRUE(anim.is_animating());
    EXPECT_EQ(mgr.active_count(), 1u);

    // Tick the manager until settled
    float dt = 0.016f;
    size_t ticks = 0;
    while (anim.is_animating() && ticks < 200) {
        mgr.tick(dt);
        ++ticks;
    }

    EXPECT_FALSE(anim.is_animating());
    EXPECT_EQ(mgr.active_count(), 0u);
    EXPECT_FLOAT_EQ(anim.get(), 100.0f);
}

TEST(AnimationTest, AnimatedValueTweenAndDirtyObserver) {
    auto& mgr = animation::AnimationManager::instance();
    mgr.clear();

    animation::AnimatedValue<core::Color> color_anim(core::Color{1.0f, 0.0f, 0.0f, 1.0f});

    int dirty_notifications = 0;
    color_anim.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* cnt = static_cast<int*>(ctx);
            (*cnt)++;
            EXPECT_EQ(flags, core::kPaint);
        },
        &dirty_notifications, core::kPaint);

    core::Color target{0.0f, 1.0f, 0.0f, 1.0f};
    color_anim.tween_to(target, 0.1f, animation::CubicBezier::linear());

    EXPECT_TRUE(color_anim.is_animating());
    EXPECT_EQ(mgr.active_count(), 1u);

    // Tick half duration
    mgr.tick(0.05f);
    EXPECT_TRUE(color_anim.is_animating());
    EXPECT_GT(dirty_notifications, 0);
    EXPECT_NEAR(color_anim.get().r, 0.5f, 0.1f);
    EXPECT_NEAR(color_anim.get().g, 0.5f, 0.1f);

    // Complete tween
    mgr.tick(0.06f);
    EXPECT_FALSE(color_anim.is_animating());
    EXPECT_EQ(mgr.active_count(), 0u);
    EXPECT_NEAR(color_anim.get().r, 0.0f, 1e-4f);
    EXPECT_NEAR(color_anim.get().g, 1.0f, 1e-4f);
}

TEST(AnimationTest, ButtonSmoothColorTransitions) {
    core::NodeTree tree;

    auto btn = widget::ButtonBuilder(tree, "Animated Button").build();

    tree.set_root(btn);
    tree.layout(layout::BoxConstraints::tight({120.0f, 40.0f}));

    auto* node = tree.get(btn);
    ASSERT_NE(node, nullptr);
    auto* data = reinterpret_cast<widget::ButtonData*>(node->user_data);
    ASSERT_NE(data, nullptr);

    // Initial color matches normal background
    EXPECT_EQ(data->animated_bg.get(), data->normal_style.background);
    EXPECT_FALSE(data->animated_bg.is_animating());

    // Trigger hover
    data->state.set(widget::ButtonState::Hovered);
    EXPECT_TRUE(data->animated_bg.is_animating());

    // Advance 60ms (~half of 120ms transition)
    animation::AnimationManager::instance().tick(0.06f);
    EXPECT_TRUE(data->animated_bg.is_animating());
    // Color should be between normal and hover
    EXPECT_NE(data->animated_bg.get(), data->normal_style.background);
    EXPECT_NE(data->animated_bg.get(), data->hover_style.background);

    // Complete transition
    animation::AnimationManager::instance().tick(0.10f);
    EXPECT_FALSE(data->animated_bg.is_animating());
    EXPECT_NEAR(data->animated_bg.get().r, data->hover_style.background.r, 1e-3f);
    EXPECT_NEAR(data->animated_bg.get().g, data->hover_style.background.g, 1e-3f);
}

TEST(AnimationTest, ScrollAreaSmoothScrollPhysics) {
    core::NodeTree tree;

    auto content = widget::BoxBuilder(tree).width(200.0f).height(1000.0f).build();

    auto scroll_area = widget::ScrollAreaBuilder(tree)
                           .width(200.0f)
                           .height(200.0f)
                           .smooth_scroll(true)
                           .child(content)
                           .build();

    tree.set_root(scroll_area);
    tree.layout(layout::BoxConstraints::tight({200.0f, 200.0f}));

    auto* data = reinterpret_cast<widget::ScrollAreaData*>(tree.get(scroll_area)->user_data);
    ASSERT_NE(data, nullptr);
    EXPECT_TRUE(data->smooth_scroll);
    EXPECT_FLOAT_EQ(data->scroll_offset.y, 0.0f);

    event::InputDispatcher dispatcher(tree);

    // Scroll down 100px
    dispatcher.on_scroll(0.0f, -100.0f, {50.0f, 50.0f});
    EXPECT_FLOAT_EQ(data->target_scroll.y, 100.0f);
    EXPECT_TRUE(data->animated_scroll.is_animating());

    // Initially position has barely moved
    EXPECT_LT(data->scroll_offset.y, 100.0f);

    // Tick spring animation
    animation::AnimationManager::instance().tick(0.05f);
    EXPECT_GT(data->scroll_offset.y, 0.0f);
    EXPECT_LT(data->scroll_offset.y, 100.0f);

    // Settle spring
    for (int i = 0; i < 150 && data->animated_scroll.is_animating(); ++i) {
        animation::AnimationManager::instance().tick(0.016f);
    }

    EXPECT_FALSE(data->animated_scroll.is_animating());
    EXPECT_NEAR(data->scroll_offset.y, 100.0f, 0.1f);
}

TEST(AnimationTest, VirtualListSmoothScrollPhysics) {
    core::NodeTree tree;

    auto vl_node = widget::VirtualListBuilder(tree)
                       .item_count(100)
                       .item_extent(50.0f)
                       .width(300.0f)
                       .height(300.0f)
                       .smooth_scroll(true)
                       .item_builder([](core::NodeTree& t, size_t, void*) {
                           return widget::BoxBuilder(t).height(50.0f).build();
                       })
                       .build();

    tree.set_root(vl_node);
    tree.layout(layout::BoxConstraints::tight({300.0f, 300.0f}));

    auto* data = reinterpret_cast<widget::VirtualListData*>(tree.get(vl_node)->user_data);
    ASSERT_NE(data, nullptr);
    EXPECT_TRUE(data->smooth_scroll);
    EXPECT_FLOAT_EQ(data->scroll_offset.y, 0.0f);

    event::InputDispatcher dispatcher(tree);

    // Scroll down 150px
    dispatcher.on_scroll(0.0f, -150.0f, {50.0f, 50.0f});
    EXPECT_FLOAT_EQ(data->target_scroll_y, 150.0f);
    EXPECT_TRUE(data->animated_scroll_y.is_animating());

    // Settle spring
    for (int i = 0; i < 150 && data->animated_scroll_y.is_animating(); ++i) {
        animation::AnimationManager::instance().tick(0.016f);
    }

    EXPECT_FALSE(data->animated_scroll_y.is_animating());
    EXPECT_NEAR(data->scroll_offset.y, 150.0f, 0.1f);
}
