#include "north/core/types.hpp"

#include <gtest/gtest.h>

namespace north::core::testing {

TEST(TypesTest, PointArithmetic) {
    Point a{10.0f, 20.0f};
    Point b{5.0f, 8.0f};

    Point sum = a + b;
    EXPECT_FLOAT_EQ(sum.x, 15.0f);
    EXPECT_FLOAT_EQ(sum.y, 28.0f);

    Point diff = a - b;
    EXPECT_FLOAT_EQ(diff.x, 5.0f);
    EXPECT_FLOAT_EQ(diff.y, 12.0f);

    EXPECT_EQ(a, (Point{10.0f, 20.0f}));
    EXPECT_NE(a, b);
}

TEST(TypesTest, SizeEmptyCheck) {
    Size normal{100.0f, 50.0f};
    EXPECT_FALSE(normal.is_empty());

    Size zero_w{0.0f, 50.0f};
    EXPECT_TRUE(zero_w.is_empty());

    Size neg_h{100.0f, -5.0f};
    EXPECT_TRUE(neg_h.is_empty());
}

TEST(TypesTest, RectContainsPoint) {
    Rect rect{10.0f, 20.0f, 100.0f, 50.0f};

    EXPECT_TRUE(rect.contains(Point{10.0f, 20.0f}));
    EXPECT_TRUE(rect.contains(Point{50.0f, 40.0f}));
    EXPECT_TRUE(rect.contains(Point{109.9f, 69.9f}));

    EXPECT_FALSE(rect.contains(Point{9.9f, 20.0f}));
    EXPECT_FALSE(rect.contains(Point{110.0f, 50.0f}));
    EXPECT_FALSE(rect.contains(Point{50.0f, 70.0f}));
}

TEST(TypesTest, RectIntersectOverlapping) {
    Rect a{0.0f, 0.0f, 100.0f, 100.0f};
    Rect b{50.0f, 50.0f, 100.0f, 100.0f};

    Rect inter = a.intersect(b);
    EXPECT_FLOAT_EQ(inter.x, 50.0f);
    EXPECT_FLOAT_EQ(inter.y, 50.0f);
    EXPECT_FLOAT_EQ(inter.width, 50.0f);
    EXPECT_FLOAT_EQ(inter.height, 50.0f);
    EXPECT_FALSE(inter.is_empty());
}

TEST(TypesTest, RectIntersectDisjointReturnsEmpty) {
    Rect a{0.0f, 0.0f, 10.0f, 10.0f};
    Rect b{20.0f, 20.0f, 10.0f, 10.0f};

    Rect inter = a.intersect(b);
    EXPECT_TRUE(inter.is_empty());
}

TEST(TypesTest, RectUniteBoundingBox) {
    Rect a{10.0f, 10.0f, 20.0f, 20.0f};
    Rect b{40.0f, 50.0f, 10.0f, 10.0f};

    Rect united = a.unite(b);
    EXPECT_FLOAT_EQ(united.x, 10.0f);
    EXPECT_FLOAT_EQ(united.y, 10.0f);
    EXPECT_FLOAT_EQ(united.width, 40.0f);
    EXPECT_FLOAT_EQ(united.height, 50.0f);
}

TEST(TypesTest, ColorConversionsAndPremultiply) {
    Color red = Color::from_rgba8(255, 0, 0, 255);
    EXPECT_FLOAT_EQ(red.r, 1.0f);
    EXPECT_FLOAT_EQ(red.g, 0.0f);
    EXPECT_FLOAT_EQ(red.b, 0.0f);
    EXPECT_FLOAT_EQ(red.a, 1.0f);

    Color hex_color = Color::from_hex(0x12345678);
    EXPECT_FLOAT_EQ(hex_color.r, 0x12 / 255.0f);
    EXPECT_FLOAT_EQ(hex_color.g, 0x34 / 255.0f);
    EXPECT_FLOAT_EQ(hex_color.b, 0x56 / 255.0f);
    EXPECT_FLOAT_EQ(hex_color.a, 0x78 / 255.0f);

    Color semi_white{1.0f, 1.0f, 1.0f, 0.5f};
    Color premult = semi_white.premultiply();
    EXPECT_FLOAT_EQ(premult.r, 0.5f);
    EXPECT_FLOAT_EQ(premult.g, 0.5f);
    EXPECT_FLOAT_EQ(premult.b, 0.5f);
    EXPECT_FLOAT_EQ(premult.a, 0.5f);

    uint32_t packed = red.to_rgba8();
    EXPECT_EQ(packed, 0xFF0000FF);
}

TEST(TypesTest, EdgesAndCornerRadiiFactories) {
    Edges u = Edges::uniform(8.0f);
    EXPECT_FLOAT_EQ(u.top, 8.0f);
    EXPECT_FLOAT_EQ(u.right, 8.0f);
    EXPECT_FLOAT_EQ(u.bottom, 8.0f);
    EXPECT_FLOAT_EQ(u.left, 8.0f);
    EXPECT_FLOAT_EQ(u.horizontal(), 16.0f);
    EXPECT_FLOAT_EQ(u.vertical(), 16.0f);

    Edges s = Edges::symmetric(10.0f, 20.0f);
    EXPECT_FLOAT_EQ(s.top, 10.0f);
    EXPECT_FLOAT_EQ(s.bottom, 10.0f);
    EXPECT_FLOAT_EQ(s.left, 20.0f);
    EXPECT_FLOAT_EQ(s.right, 20.0f);

    CornerRadii r = CornerRadii::uniform(4.0f);
    EXPECT_FLOAT_EQ(r.top_left, 4.0f);
    EXPECT_FLOAT_EQ(r.bottom_right, 4.0f);
}

} // namespace north::core::testing
