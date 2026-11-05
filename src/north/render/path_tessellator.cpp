#include "north/render/path_tessellator.hpp"

#include <algorithm>
#include <cmath>

namespace north::render {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kAAWidth = 1.0f;

struct Vec2 {
    float x{0.0f};
    float y{0.0f};

    [[nodiscard]] float length_sq() const noexcept { return x * x + y * y; }
    [[nodiscard]] float length() const noexcept { return std::sqrt(length_sq()); }

    [[nodiscard]] Vec2 normalized() const noexcept {
        float len = length();
        if (len < 1e-6f) return {0.0f, 0.0f};
        return {x / len, y / len};
    }
};

Vec2 operator+(Vec2 a, Vec2 b) noexcept { return {a.x + b.x, a.y + b.y}; }
Vec2 operator-(Vec2 a, Vec2 b) noexcept { return {a.x - b.x, a.y - b.y}; }
Vec2 operator*(Vec2 a, float s) noexcept { return {a.x * s, a.y * s}; }

float cross_product(Vec2 a, Vec2 b) noexcept { return a.x * b.y - a.y * b.x; }
float dot_product(Vec2 a, Vec2 b) noexcept { return a.x * b.x + a.y * b.y; }

bool point_in_triangle(Vec2 p, Vec2 a, Vec2 b, Vec2 c) noexcept {
    float c1 = cross_product(b - a, p - a);
    float c2 = cross_product(c - b, p - b);
    float c3 = cross_product(a - c, p - c);
    return (c1 >= -1e-5f && c2 >= -1e-5f && c3 >= -1e-5f) ||
           (c1 <= 1e-5f && c2 <= 1e-5f && c3 <= 1e-5f);
}

struct SubpathContour {
    std::vector<core::Point> points;
    bool closed{false};
};

std::vector<SubpathContour> extract_subpaths(const Path& path) {
    std::vector<SubpathContour> subpaths;
    auto verbs = path.verbs();
    auto pts = path.points();

    size_t pt_idx = 0;
    SubpathContour cur;

    for (auto verb : verbs) {
        switch (verb) {
        case PathVerb::MoveTo:
            if (!cur.points.empty()) {
                subpaths.push_back(std::move(cur));
                cur = SubpathContour{};
            }
            cur.points.push_back(pts[pt_idx++]);
            break;
        case PathVerb::LineTo:
            cur.points.push_back(pts[pt_idx++]);
            break;
        case PathVerb::Close:
            cur.closed = true;
            if (!cur.points.empty()) {
                subpaths.push_back(std::move(cur));
                cur = SubpathContour{};
            }
            break;
        default:
            break;
        }
    }

    if (!cur.points.empty()) {
        subpaths.push_back(std::move(cur));
    }

    return subpaths;
}

uint16_t add_vertex(PathMesh& mesh, core::Point pos, float coverage, core::Color color,
                    core::Rect clip) noexcept {
    uint16_t idx = static_cast<uint16_t>(mesh.vertices.size());
    mesh.vertices.push_back(gpu::PathVertex{
        .position = pos,
        .aa_coverage = coverage,
        .pad = 0.0f,
        .color = color,
        .clip_rect = clip,
    });
    return idx;
}

void add_triangle(PathMesh& mesh, uint16_t i0, uint16_t i1, uint16_t i2) noexcept {
    mesh.indices.push_back(i0);
    mesh.indices.push_back(i1);
    mesh.indices.push_back(i2);
}

} // namespace

void PathTessellator::tessellate_fill(const Path& path, core::Color color, core::Rect clip_rect,
                                     FillRule rule, PathMesh& out_mesh) {
    (void)rule;
    Path flat = path.flatten(0.25f);
    auto subpaths = extract_subpaths(flat);

    for (const auto& sp : subpaths) {
        const auto& pts = sp.points;
        if (pts.size() < 3) continue;

        // Deduplicate closing point if it matches start
        size_t n = pts.size();
        if (n > 3 && std::abs(pts[0].x - pts[n - 1].x) < 1e-4f &&
            std::abs(pts[0].y - pts[n - 1].y) < 1e-4f) {
            --n;
        }
        if (n < 3) continue;

        // Determine signed area (orientation)
        float signed_area = 0.0f;
        for (size_t i = 0; i < n; ++i) {
            size_t next = (i + 1) % n;
            signed_area += (pts[i].x * pts[next].y - pts[next].x * pts[i].y);
        }

        bool is_ccw = (signed_area > 0.0f);

        // Map polygon points to vertices in mesh
        std::vector<uint16_t> poly_indices;
        poly_indices.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            poly_indices.push_back(add_vertex(out_mesh, pts[i], 1.0f, color, clip_rect));
        }

        // Ear Clipping Triangulation
        std::vector<size_t> active;
        active.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            active.push_back(i);
        }

        size_t attempts = 0;
        size_t max_attempts = active.size() * active.size();

        while (active.size() > 2 && attempts < max_attempts) {
            bool ear_found = false;
            size_t count = active.size();

            for (size_t i = 0; i < count; ++i) {
                size_t prev_idx = active[(i + count - 1) % count];
                size_t curr_idx = active[i];
                size_t next_idx = active[(i + 1) % count];

                Vec2 a{pts[prev_idx].x, pts[prev_idx].y};
                Vec2 b{pts[curr_idx].x, pts[curr_idx].y};
                Vec2 c{pts[next_idx].x, pts[next_idx].y};

                float cross = cross_product(b - a, c - a);
                bool convex = is_ccw ? (cross > 1e-5f) : (cross < -1e-5f);
                if (!convex) {
                    continue;
                }

                bool has_point_inside = false;
                for (size_t j = 0; j < count; ++j) {
                    size_t test_idx = active[j];
                    if (test_idx == prev_idx || test_idx == curr_idx || test_idx == next_idx) {
                        continue;
                    }
                    Vec2 p{pts[test_idx].x, pts[test_idx].y};
                    if (point_in_triangle(p, a, b, c)) {
                        has_point_inside = true;
                        break;
                    }
                }

                if (!has_point_inside) {
                    // Clip ear
                    add_triangle(out_mesh, poly_indices[prev_idx], poly_indices[curr_idx],
                                 poly_indices[next_idx]);
                    active.erase(active.begin() + static_cast<ptrdiff_t>(i));
                    ear_found = true;
                    break;
                }
            }

            if (!ear_found) {
                // Degenerate fallback: clip the first vertex
                size_t prev_idx = active[count - 1];
                size_t curr_idx = active[0];
                size_t next_idx = active[1];
                add_triangle(out_mesh, poly_indices[prev_idx], poly_indices[curr_idx],
                             poly_indices[next_idx]);
                active.erase(active.begin());
            }

            ++attempts;
        }

        // Generate AA Fringe around polygon boundary
        std::vector<uint16_t> outer_fringe_indices;
        outer_fringe_indices.reserve(n);

        for (size_t i = 0; i < n; ++i) {
            size_t prev = (i + n - 1) % n;
            size_t next = (i + 1) % n;

            Vec2 p_prev{pts[prev].x, pts[prev].y};
            Vec2 p_curr{pts[i].x, pts[i].y};
            Vec2 p_next{pts[next].x, pts[next].y};

            Vec2 e0 = (p_curr - p_prev).normalized();
            Vec2 e1 = (p_next - p_curr).normalized();

            // Outward normal: for CCW (dy, -dx), for CW (-dy, dx)
            Vec2 n0 = is_ccw ? Vec2{e0.y, -e0.x} : Vec2{-e0.y, e0.x};
            Vec2 n1 = is_ccw ? Vec2{e1.y, -e1.x} : Vec2{-e1.y, e1.x};

            Vec2 n_avg = (n0 + n1).normalized();
            float dot = dot_product(n_avg, n0);
            float miter = (dot > 0.2f) ? std::min(1.0f / dot, 2.0f) : 2.0f;

            Vec2 outer_pt = p_curr + n_avg * (kAAWidth * miter);
            outer_fringe_indices.push_back(
                add_vertex(out_mesh, core::Point{outer_pt.x, outer_pt.y}, 0.0f, color, clip_rect));
        }

        // Stitch AA fringe quad ribbon around outer boundary
        for (size_t i = 0; i < n; ++i) {
            size_t next = (i + 1) % n;
            uint16_t i_inner = poly_indices[i];
            uint16_t i_outer = outer_fringe_indices[i];
            uint16_t next_inner = poly_indices[next];
            uint16_t next_outer = outer_fringe_indices[next];

            add_triangle(out_mesh, i_inner, i_outer, next_outer);
            add_triangle(out_mesh, i_inner, next_outer, next_inner);
        }
    }
}

void PathTessellator::tessellate_stroke(const Path& path, core::Color color, core::Rect clip_rect,
                                       const StrokeStyle& style, PathMesh& out_mesh) {
    if (style.width <= 0.0f) return;

    Path flat = path.flatten(0.25f);
    auto subpaths = extract_subpaths(flat);
    float hw = style.width * 0.5f;

    for (const auto& sp : subpaths) {
        const auto& pts = sp.points;
        if (pts.size() < 2) continue;

        size_t n = pts.size();
        bool is_closed = sp.closed;
        if (!is_closed && n > 2 && std::abs(pts[0].x - pts[n - 1].x) < 1e-4f &&
            std::abs(pts[0].y - pts[n - 1].y) < 1e-4f) {
            is_closed = true;
        }

        std::vector<uint16_t> left_inner(n);
        std::vector<uint16_t> right_inner(n);
        std::vector<uint16_t> left_outer(n);
        std::vector<uint16_t> right_outer(n);

        for (size_t i = 0; i < n; ++i) {
            Vec2 p_curr{pts[i].x, pts[i].y};
            Vec2 normal{0.0f, 0.0f};

            if (is_closed) {
                size_t prev = (i + n - 1) % n;
                size_t next = (i + 1) % n;
                Vec2 e0 = (p_curr - Vec2{pts[prev].x, pts[prev].y}).normalized();
                Vec2 e1 = (Vec2{pts[next].x, pts[next].y} - p_curr).normalized();
                Vec2 n0{-e0.y, e0.x};
                Vec2 n1{-e1.y, e1.x};
                normal = (n0 + n1).normalized();
            } else {
                if (i == 0) {
                    Vec2 dir = (Vec2{pts[1].x, pts[1].y} - p_curr).normalized();
                    normal = {-dir.y, dir.x};
                } else if (i == n - 1) {
                    Vec2 dir = (p_curr - Vec2{pts[i - 1].x, pts[i - 1].y}).normalized();
                    normal = {-dir.y, dir.x};
                } else {
                    Vec2 e0 = (p_curr - Vec2{pts[i - 1].x, pts[i - 1].y}).normalized();
                    Vec2 e1 = (Vec2{pts[i + 1].x, pts[i + 1].y} - p_curr).normalized();
                    Vec2 n0{-e0.y, e0.x};
                    Vec2 n1{-e1.y, e1.x};
                    normal = (n0 + n1).normalized();
                }
            }

            Vec2 p_left_inner = p_curr + normal * hw;
            Vec2 p_right_inner = p_curr - normal * hw;
            Vec2 p_left_outer = p_curr + normal * (hw + kAAWidth);
            Vec2 p_right_outer = p_curr - normal * (hw + kAAWidth);

            left_inner[i] = add_vertex(out_mesh, core::Point{p_left_inner.x, p_left_inner.y}, 1.0f,
                                       color, clip_rect);
            right_inner[i] = add_vertex(out_mesh, core::Point{p_right_inner.x, p_right_inner.y},
                                        1.0f, color, clip_rect);
            left_outer[i] = add_vertex(out_mesh, core::Point{p_left_outer.x, p_left_outer.y}, 0.0f,
                                       color, clip_rect);
            right_outer[i] = add_vertex(out_mesh, core::Point{p_right_outer.x, p_right_outer.y},
                                        0.0f, color, clip_rect);
        }

        size_t seg_count = is_closed ? n : (n - 1);
        for (size_t i = 0; i < seg_count; ++i) {
            size_t next = (i + 1) % n;

            // Center stroke ribbon
            add_triangle(out_mesh, left_inner[i], right_inner[i], right_inner[next]);
            add_triangle(out_mesh, left_inner[i], right_inner[next], left_inner[next]);

            // Left AA fringe ribbon
            add_triangle(out_mesh, left_outer[i], left_inner[i], left_inner[next]);
            add_triangle(out_mesh, left_outer[i], left_inner[next], left_outer[next]);

            // Right AA fringe ribbon
            add_triangle(out_mesh, right_inner[i], right_outer[i], right_outer[next]);
            add_triangle(out_mesh, right_inner[i], right_outer[next], right_inner[next]);
        }

        // Handle open path caps (Square / Round)
        if (!is_closed) {
            // Start cap
            Vec2 p0{pts[0].x, pts[0].y};
            Vec2 p1{pts[1].x, pts[1].y};
            Vec2 dir0 = (p0 - p1).normalized(); // pointing outward from start

            if (style.cap == LineCap::Square) {
                Vec2 ext_l = p0 + dir0 * hw + Vec2{dir0.y, -dir0.x} * hw;
                Vec2 ext_r = p0 + dir0 * hw - Vec2{dir0.y, -dir0.x} * hw;
                uint16_t vl = add_vertex(out_mesh, core::Point{ext_l.x, ext_l.y}, 1.0f, color,
                                         clip_rect);
                uint16_t vr = add_vertex(out_mesh, core::Point{ext_r.x, ext_r.y}, 1.0f, color,
                                         clip_rect);
                add_triangle(out_mesh, left_inner[0], right_inner[0], vr);
                add_triangle(out_mesh, left_inner[0], vr, vl);
            } else if (style.cap == LineCap::Round) {
                // Semicircle fan
                constexpr int kFanSegments = 8;
                uint16_t center_idx =
                    add_vertex(out_mesh, pts[0], 1.0f, color, clip_rect);
                Vec2 normal = (Vec2{pts[0].x, pts[0].y} - Vec2{pts[1].x, pts[1].y}).normalized();
                float base_angle = std::atan2(normal.y, normal.x);

                uint16_t prev_fan = left_inner[0];
                for (int s = 1; s <= kFanSegments; ++s) {
                    float theta = base_angle - (kPi * 0.5f) + (static_cast<float>(s) / static_cast<float>(kFanSegments)) * kPi;
                    Vec2 pt = p0 + Vec2{std::cos(theta), std::sin(theta)} * hw;
                    uint16_t curr_fan = (s == kFanSegments)
                                            ? right_inner[0]
                                            : add_vertex(out_mesh, core::Point{pt.x, pt.y}, 1.0f,
                                                         color, clip_rect);
                    add_triangle(out_mesh, center_idx, prev_fan, curr_fan);
                    prev_fan = curr_fan;
                }
            }

            // End cap
            Vec2 pn{pts[n - 1].x, pts[n - 1].y};
            Vec2 pn_prev{pts[n - 2].x, pts[n - 2].y};
            Vec2 dirn = (pn - pn_prev).normalized(); // pointing outward from end

            if (style.cap == LineCap::Square) {
                Vec2 ext_l = pn + dirn * hw + Vec2{dirn.y, -dirn.x} * hw;
                Vec2 ext_r = pn + dirn * hw - Vec2{dirn.y, -dirn.x} * hw;
                uint16_t vl = add_vertex(out_mesh, core::Point{ext_l.x, ext_l.y}, 1.0f, color,
                                         clip_rect);
                uint16_t vr = add_vertex(out_mesh, core::Point{ext_r.x, ext_r.y}, 1.0f, color,
                                         clip_rect);
                add_triangle(out_mesh, left_inner[n - 1], right_inner[n - 1], vr);
                add_triangle(out_mesh, left_inner[n - 1], vr, vl);
            } else if (style.cap == LineCap::Round) {
                constexpr int kFanSegments = 8;
                uint16_t center_idx =
                    add_vertex(out_mesh, pts[n - 1], 1.0f, color, clip_rect);
                float base_angle = std::atan2(dirn.y, dirn.x);

                uint16_t prev_fan = left_inner[n - 1];
                for (int s = 1; s <= kFanSegments; ++s) {
                    float theta = base_angle - (kPi * 0.5f) + (static_cast<float>(s) / static_cast<float>(kFanSegments)) * kPi;
                    Vec2 pt = pn + Vec2{std::cos(theta), std::sin(theta)} * hw;
                    uint16_t curr_fan = (s == kFanSegments)
                                            ? right_inner[n - 1]
                                            : add_vertex(out_mesh, core::Point{pt.x, pt.y}, 1.0f,
                                                         color, clip_rect);
                    add_triangle(out_mesh, center_idx, prev_fan, curr_fan);
                    prev_fan = curr_fan;
                }
            }
        }
    }
}

} // namespace north::render
