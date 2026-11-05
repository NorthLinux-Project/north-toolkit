#pragma once

#include "north/core/types.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace north::render {

enum class PathVerb : uint8_t {
    MoveTo,
    LineTo,
    QuadTo,
    CubicTo,
    Close,
};

enum class FillRule : uint8_t {
    EvenOdd,
    NonZero,
};

enum class LineCap : uint8_t {
    Butt,
    Round,
    Square,
};

enum class LineJoin : uint8_t {
    Miter,
    Bevel,
    Round,
};

struct StrokeStyle {
    float width{1.0f};
    LineCap cap{LineCap::Butt};
    LineJoin join{LineJoin::Miter};
    float miter_limit{4.0f};
};

class Path {
public:
    Path() = default;
    ~Path() = default;

    Path(const Path&) = default;
    Path& operator=(const Path&) = default;
    Path(Path&&) noexcept = default;
    Path& operator=(Path&&) noexcept = default;

    void move_to(core::Point p) noexcept;
    void move_to(float x, float y) noexcept { move_to(core::Point{x, y}); }

    void line_to(core::Point p) noexcept;
    void line_to(float x, float y) noexcept { line_to(core::Point{x, y}); }

    void quad_to(core::Point ctrl, core::Point to) noexcept;
    void quad_to(float cx, float cy, float x, float y) noexcept {
        quad_to(core::Point{cx, cy}, core::Point{x, y});
    }

    void cubic_to(core::Point ctrl1, core::Point ctrl2, core::Point to) noexcept;
    void cubic_to(float c1x, float c1y, float c2x, float c2y, float x, float y) noexcept {
        cubic_to(core::Point{c1x, c1y}, core::Point{c2x, c2y}, core::Point{x, y});
    }

    void arc_to(core::Point to, core::Size radius, float x_axis_rotation_deg, bool large_arc,
                bool sweep) noexcept;

    void close() noexcept;
    void reset() noexcept;

    void add_rect(core::Rect rect) noexcept;
    void add_rounded_rect(core::Rect rect, core::CornerRadii radii) noexcept;
    void add_circle(core::Point center, float radius) noexcept;
    void add_ellipse(core::Point center, core::Size radius) noexcept;

    void translate(core::Point offset) noexcept;
    void scale(float sx, float sy) noexcept;
    void transform(float m00, float m01, float m10, float m11, float tx, float ty) noexcept;

    [[nodiscard]] bool empty() const noexcept { return verbs_.empty(); }
    [[nodiscard]] std::span<const PathVerb> verbs() const noexcept { return verbs_; }
    [[nodiscard]] std::span<const core::Point> points() const noexcept { return points_; }
    [[nodiscard]] core::Point current_point() const noexcept { return current_point_; }

    [[nodiscard]] core::Rect bounds() const noexcept;
    [[nodiscard]] Path flatten(float tolerance = 0.25f) const;

private:
    void arc_to_cubics(core::Point from, core::Point to, float rx, float ry,
                       float angle_rad, bool large_arc, bool sweep) noexcept;

    std::vector<PathVerb> verbs_;
    std::vector<core::Point> points_;
    core::Point current_point_{0.0f, 0.0f};
    core::Point subpath_start_{0.0f, 0.0f};
    bool has_subpath_{false};
};

} // namespace north::render
