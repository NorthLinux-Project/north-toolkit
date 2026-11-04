#include "north/layout/constraints.hpp"

#include <gtest/gtest.h>

namespace north::layout::testing {

TEST(ConstraintsTest, TightFactoryCreatesExactBounds) {
    core::Size s{300.0f, 150.0f};
    BoxConstraints c = BoxConstraints::tight(s);

    EXPECT_FLOAT_EQ(c.min_width, 300.0f);
    EXPECT_FLOAT_EQ(c.max_width, 300.0f);
    EXPECT_FLOAT_EQ(c.min_height, 150.0f);
    EXPECT_FLOAT_EQ(c.max_height, 150.0f);
    EXPECT_TRUE(c.is_tight());
    EXPECT_TRUE(c.has_bounded_width());
    EXPECT_TRUE(c.has_bounded_height());
}

TEST(ConstraintsTest, LooseFactoryAllowsZeroMinimum) {
    core::Size s{500.0f, 400.0f};
    BoxConstraints c = BoxConstraints::loose(s);

    EXPECT_FLOAT_EQ(c.min_width, 0.0f);
    EXPECT_FLOAT_EQ(c.max_width, 500.0f);
    EXPECT_FLOAT_EQ(c.min_height, 0.0f);
    EXPECT_FLOAT_EQ(c.max_height, 400.0f);
    EXPECT_FALSE(c.is_tight());
    EXPECT_TRUE(c.has_bounded_width());
    EXPECT_TRUE(c.has_bounded_height());
}

TEST(ConstraintsTest, LoosenResetsMinimaToZero) {
    BoxConstraints c{100.0f, 400.0f, 50.0f, 200.0f};
    BoxConstraints loose_c = c.loosen();

    EXPECT_FLOAT_EQ(loose_c.min_width, 0.0f);
    EXPECT_FLOAT_EQ(loose_c.max_width, 400.0f);
    EXPECT_FLOAT_EQ(loose_c.min_height, 0.0f);
    EXPECT_FLOAT_EQ(loose_c.max_height, 200.0f);
}

TEST(ConstraintsTest, ConstrainClampsSizes) {
    BoxConstraints c{50.0f, 200.0f, 30.0f, 100.0f};

    core::Size too_small{20.0f, 10.0f};
    core::Size clamped_small = c.constrain(too_small);
    EXPECT_FLOAT_EQ(clamped_small.width, 50.0f);
    EXPECT_FLOAT_EQ(clamped_small.height, 30.0f);

    core::Size too_large{300.0f, 150.0f};
    core::Size clamped_large = c.constrain(too_large);
    EXPECT_FLOAT_EQ(clamped_large.width, 200.0f);
    EXPECT_FLOAT_EQ(clamped_large.height, 100.0f);

    core::Size in_range{100.0f, 60.0f};
    core::Size clamped_in_range = c.constrain(in_range);
    EXPECT_FLOAT_EQ(clamped_in_range.width, 100.0f);
    EXPECT_FLOAT_EQ(clamped_in_range.height, 60.0f);
}

TEST(ConstraintsTest, TightenForcesConstraintTightness) {
    BoxConstraints c{50.0f, 300.0f, 50.0f, 300.0f};
    BoxConstraints tightened = c.tighten(core::Size{120.0f, 80.0f});

    EXPECT_FLOAT_EQ(tightened.min_width, 120.0f);
    EXPECT_FLOAT_EQ(tightened.max_width, 120.0f);
    EXPECT_FLOAT_EQ(tightened.min_height, 80.0f);
    EXPECT_FLOAT_EQ(tightened.max_height, 80.0f);
    EXPECT_TRUE(tightened.is_tight());
}

TEST(ConstraintsTest, InfiniteBoundsAreUnbounded) {
    BoxConstraints unbounded{};
    EXPECT_FALSE(unbounded.has_bounded_width());
    EXPECT_FALSE(unbounded.has_bounded_height());
    EXPECT_FALSE(unbounded.is_tight());
}

} // namespace north::layout::testing
