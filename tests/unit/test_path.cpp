#include "north/render/display_list.hpp"
#include "north/render/path.hpp"
#include "north/render/path_tessellator.hpp"
#include "north/render/svg_path.hpp"
#include "north/widget/icon.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace north::render::testing {

TEST(PathTest, EmptyPathHasZeroBoundsAndNoVerbs) {
    Path p;
    EXPECT_TRUE(p.empty());
    EXPECT_EQ(p.verbs().size(), 0u);
    EXPECT_EQ(p.points().size(), 0u);
    EXPECT_FLOAT_EQ(p.bounds().width, 0.0f);
    EXPECT_FLOAT_EQ(p.bounds().height, 0.0f);
}

TEST(PathTest, BasicLineSegmentsAndBounds) {
    Path p;
    p.move_to(10.0f, 20.0f);
    p.line_to(50.0f, 20.0f);
    p.line_to(50.0f, 60.0f);
    p.close();

    EXPECT_FALSE(p.empty());
    EXPECT_EQ(p.verbs().size(), 4u);
    EXPECT_EQ(p.verbs()[0], PathVerb::MoveTo);
    EXPECT_EQ(p.verbs()[1], PathVerb::LineTo);
    EXPECT_EQ(p.verbs()[2], PathVerb::LineTo);
    EXPECT_EQ(p.verbs()[3], PathVerb::Close);

    auto b = p.bounds();
    EXPECT_FLOAT_EQ(b.x, 10.0f);
    EXPECT_FLOAT_EQ(b.y, 20.0f);
    EXPECT_FLOAT_EQ(b.width, 40.0f);
    EXPECT_FLOAT_EQ(b.height, 40.0f);
}

TEST(PathTest, TransformTranslateScale) {
    Path p;
    p.add_rect(core::Rect{0.0f, 0.0f, 10.0f, 10.0f});

    p.scale(2.0f, 3.0f);
    auto b1 = p.bounds();
    EXPECT_FLOAT_EQ(b1.width, 20.0f);
    EXPECT_FLOAT_EQ(b1.height, 30.0f);

    p.translate(core::Point{5.0f, 10.0f});
    auto b2 = p.bounds();
    EXPECT_FLOAT_EQ(b2.x, 5.0f);
    EXPECT_FLOAT_EQ(b2.y, 10.0f);
}

TEST(PathTest, CurveFlatteningProducesPolyline) {
    Path p;
    p.move_to(0.0f, 0.0f);
    p.cubic_to(0.0f, 50.0f, 100.0f, 50.0f, 100.0f, 0.0f);

    Path flat = p.flatten(0.25f);
    EXPECT_FALSE(flat.empty());

    for (auto v : flat.verbs()) {
        EXPECT_TRUE(v == PathVerb::MoveTo || v == PathVerb::LineTo || v == PathVerb::Close);
    }
    EXPECT_GT(flat.points().size(), 4u);
}

TEST(SvgPathTest, ParseSimpleLinesAndCloses) {
    Path p;
    bool ok = parse_svg_path("M 10 20 L 30 40 H 50 V 60 Z", p);
    EXPECT_TRUE(ok);
    EXPECT_EQ(p.verbs().size(), 5u);
    EXPECT_EQ(p.verbs()[0], PathVerb::MoveTo);
    EXPECT_EQ(p.verbs()[1], PathVerb::LineTo);
    EXPECT_EQ(p.verbs()[2], PathVerb::LineTo);
    EXPECT_EQ(p.verbs()[3], PathVerb::LineTo);
    EXPECT_EQ(p.verbs()[4], PathVerb::Close);
    EXPECT_FLOAT_EQ(p.points()[0].x, 10.0f);
    EXPECT_FLOAT_EQ(p.points()[0].y, 20.0f);
    EXPECT_FLOAT_EQ(p.points()[1].x, 30.0f);
    EXPECT_FLOAT_EQ(p.points()[1].y, 40.0f);
    EXPECT_FLOAT_EQ(p.points()[2].x, 50.0f);
    EXPECT_FLOAT_EQ(p.points()[2].y, 40.0f);
    EXPECT_FLOAT_EQ(p.points()[3].x, 50.0f);
    EXPECT_FLOAT_EQ(p.points()[3].y, 60.0f);
}

TEST(SvgPathTest, ParseRelativeAndCompactCoordinates) {
    Path p;
    bool ok = parse_svg_path("m10 20l-5-5h10v-10z", p);
    EXPECT_TRUE(ok);
    EXPECT_EQ(p.verbs().size(), 5u);
    EXPECT_FLOAT_EQ(p.points()[0].x, 10.0f);
    EXPECT_FLOAT_EQ(p.points()[0].y, 20.0f);
    EXPECT_FLOAT_EQ(p.points()[1].x, 5.0f);
    EXPECT_FLOAT_EQ(p.points()[1].y, 15.0f);
    EXPECT_FLOAT_EQ(p.points()[2].x, 15.0f);
    EXPECT_FLOAT_EQ(p.points()[2].y, 15.0f);
    EXPECT_FLOAT_EQ(p.points()[3].x, 15.0f);
    EXPECT_FLOAT_EQ(p.points()[3].y, 5.0f);
}

TEST(SvgPathTest, ParseCubicAndQuadraticCurves) {
    Path p;
    bool ok = parse_svg_path("M 0 0 C 10 20 30 40 50 50 S 70 80 90 90 Q 95 95 100 100 T 110 110 Z", p);
    EXPECT_TRUE(ok);
    EXPECT_EQ(p.verbs().size(), 6u);
    EXPECT_EQ(p.verbs()[0], PathVerb::MoveTo);
    EXPECT_EQ(p.verbs()[1], PathVerb::CubicTo);
    EXPECT_EQ(p.verbs()[2], PathVerb::CubicTo);
    EXPECT_EQ(p.verbs()[3], PathVerb::QuadTo);
    EXPECT_EQ(p.verbs()[4], PathVerb::QuadTo);
    EXPECT_EQ(p.verbs()[5], PathVerb::Close);
}

TEST(SvgPathTest, ParseEllipticalArc) {
    Path p;
    bool ok = parse_svg_path("M 10 10 A 25 25 0 0 1 60 10 Z", p);
    EXPECT_TRUE(ok);
    EXPECT_GT(p.verbs().size(), 2u);
}

TEST(SvgPathTest, ParseInvalidSyntaxFailsGracefully) {
    Path p;
    bool ok = parse_svg_path("M 10 not_a_number L 30 40", p);
    EXPECT_FALSE(ok);
}

TEST(PathTessellatorTest, FillTriangleGeneratesInteriorAndAAFringe) {
    Path p;
    p.move_to(0.0f, 0.0f);
    p.line_to(50.0f, 0.0f);
    p.line_to(25.0f, 50.0f);
    p.close();

    PathMesh mesh;
    core::Color col{1.0f, 0.0f, 0.0f, 1.0f};
    core::Rect clip{0.0f, 0.0f, 100.0f, 100.0f};
    PathTessellator::tessellate_fill(p, col, clip, FillRule::NonZero, mesh);

    EXPECT_FALSE(mesh.empty());
    // 3 interior vertices + 3 outer fringe vertices = 6 vertices
    EXPECT_EQ(mesh.vertex_count(), 6u);
    // 1 interior triangle (3 indices) + 3 fringe quads (3 * 6 = 18 indices) = 21 indices
    EXPECT_EQ(mesh.index_count(), 21u);

    // Verify AA coverage values
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_FLOAT_EQ(mesh.vertices[i].aa_coverage, 1.0f);
    }
    for (size_t i = 3; i < 6; ++i) {
        EXPECT_FLOAT_EQ(mesh.vertices[i].aa_coverage, 0.0f);
    }
}

TEST(PathTessellatorTest, StrokeTessellationGeneratesRibbonAndAAFringe) {
    Path p;
    p.move_to(10.0f, 10.0f);
    p.line_to(90.0f, 10.0f);

    PathMesh mesh;
    core::Color col{0.0f, 1.0f, 0.0f, 1.0f};
    core::Rect clip{0.0f, 0.0f, 100.0f, 100.0f};
    StrokeStyle style{.width = 4.0f, .cap = LineCap::Butt};

    PathTessellator::tessellate_stroke(p, col, clip, style, mesh);
    EXPECT_FALSE(mesh.empty());
    // Center ribbon (2 triangles = 6 indices) + Left AA (6 indices) + Right AA (6 indices) = 18 indices
    EXPECT_EQ(mesh.index_count(), 18u);
}

TEST(DisplayListTest, PushPathFillAndStrokeCommands) {
    core::Arena arena{64 * 1024};
    DisplayList dl(arena);

    Path p;
    p.add_circle(core::Point{50.0f, 50.0f}, 25.0f);

    dl.push_path_fill(p, core::Color{1.0f, 1.0f, 1.0f, 1.0f});
    dl.push_path_stroke(p, core::Color{1.0f, 0.0f, 0.0f, 1.0f}, StrokeStyle{.width = 2.0f});

    EXPECT_EQ(dl.size(), 2u);
    EXPECT_EQ(dl.commands()[0].type, CommandType::PathMesh);
    EXPECT_EQ(dl.commands()[1].type, CommandType::PathMesh);
    EXPECT_FALSE(dl.commands()[0].path_mesh.vertices.empty());
    EXPECT_FALSE(dl.commands()[0].path_mesh.indices.empty());
}

TEST(IconWidgetTest, BuildsAndLaysOutAccordingToSize) {
    core::NodeTree tree;
    widget::IconBuilder builder(tree, "M 10 10 L 90 10 L 90 90 Z");
    builder.size(32.0f, 32.0f);
    builder.color(core::Color{0.0f, 0.5f, 1.0f, 1.0f});

    auto handle = builder.build();
    EXPECT_TRUE(handle.is_valid());

    auto* node = tree.get(handle);
    ASSERT_NE(node, nullptr);
    EXPECT_NE(node->user_data, nullptr);

    layout::BoxConstraints constraints{
        .min_width = 0.0f, .max_width = 100.0f, .min_height = 0.0f, .max_height = 100.0f};
    core::Size layout_sz = node->layout_fn(tree, handle, constraints, node->user_data);
    EXPECT_FLOAT_EQ(layout_sz.width, 32.0f);
    EXPECT_FLOAT_EQ(layout_sz.height, 32.0f);
}

} // namespace north::render::testing
