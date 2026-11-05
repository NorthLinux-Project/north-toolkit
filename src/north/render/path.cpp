#include "north/render/path.hpp"

#include <cmath>
#include <numbers>

namespace north::render {

namespace {

constexpr float kKappa = 0.5522847498307936f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

float point_distance_sq(core::Point a, core::Point b) noexcept {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

float point_line_distance_sq(core::Point p, core::Point a, core::Point b) noexcept {
    float abx = b.x - a.x;
    float aby = b.y - a.y;
    float len_sq = abx * abx + aby * aby;
    if (len_sq < 1e-8f) {
        return point_distance_sq(p, a);
    }
    float cross = (p.x - a.x) * aby - (p.y - a.y) * abx;
    return (cross * cross) / len_sq;
}

void flatten_quad(core::Point p0, core::Point p1, core::Point p2, float tol_sq, int depth,
                  Path& out) noexcept {
    if (depth >= 10 || point_line_distance_sq(p1, p0, p2) <= tol_sq) {
        out.line_to(p2);
        return;
    }

    core::Point p01{(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f};
    core::Point p12{(p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f};
    core::Point p012{(p01.x + p12.x) * 0.5f, (p01.y + p12.y) * 0.5f};

    flatten_quad(p0, p01, p012, tol_sq, depth + 1, out);
    flatten_quad(p012, p12, p2, tol_sq, depth + 1, out);
}

void flatten_cubic(core::Point p0, core::Point p1, core::Point p2, core::Point p3, float tol_sq,
                   int depth, Path& out) noexcept {
    float d1 = point_line_distance_sq(p1, p0, p3);
    float d2 = point_line_distance_sq(p2, p0, p3);
    if (depth >= 10 || (d1 <= tol_sq && d2 <= tol_sq)) {
        out.line_to(p3);
        return;
    }

    core::Point p01{(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f};
    core::Point p12{(p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f};
    core::Point p23{(p2.x + p3.x) * 0.5f, (p2.y + p3.y) * 0.5f};
    core::Point p012{(p01.x + p12.x) * 0.5f, (p01.y + p12.y) * 0.5f};
    core::Point p123{(p12.x + p23.x) * 0.5f, (p12.y + p23.y) * 0.5f};
    core::Point p0123{(p012.x + p123.x) * 0.5f, (p012.y + p123.y) * 0.5f};

    flatten_cubic(p0, p01, p012, p0123, tol_sq, depth + 1, out);
    flatten_cubic(p0123, p123, p23, p3, tol_sq, depth + 1, out);
}

} // namespace

void Path::move_to(core::Point p) noexcept {
    verbs_.push_back(PathVerb::MoveTo);
    points_.push_back(p);
    current_point_ = p;
    subpath_start_ = p;
    has_subpath_ = true;
}

void Path::line_to(core::Point p) noexcept {
    if (!has_subpath_) {
        move_to(core::Point{0.0f, 0.0f});
    }
    verbs_.push_back(PathVerb::LineTo);
    points_.push_back(p);
    current_point_ = p;
}

void Path::quad_to(core::Point ctrl, core::Point to) noexcept {
    if (!has_subpath_) {
        move_to(core::Point{0.0f, 0.0f});
    }
    verbs_.push_back(PathVerb::QuadTo);
    points_.push_back(ctrl);
    points_.push_back(to);
    current_point_ = to;
}

void Path::cubic_to(core::Point ctrl1, core::Point ctrl2, core::Point to) noexcept {
    if (!has_subpath_) {
        move_to(core::Point{0.0f, 0.0f});
    }
    verbs_.push_back(PathVerb::CubicTo);
    points_.push_back(ctrl1);
    points_.push_back(ctrl2);
    points_.push_back(to);
    current_point_ = to;
}

void Path::arc_to_cubics(core::Point from, core::Point to, float rx, float ry, float angle_rad,
                         bool large_arc, bool sweep) noexcept {
    rx = std::abs(rx);
    ry = std::abs(ry);
    if (rx < 1e-5f || ry < 1e-5f) {
        line_to(to);
        return;
    }

    float cos_phi = std::cos(angle_rad);
    float sin_phi = std::sin(angle_rad);

    float dx = (from.x - to.x) * 0.5f;
    float dy = (from.y - to.y) * 0.5f;
    float x1p = cos_phi * dx + sin_phi * dy;
    float y1p = -sin_phi * dx + cos_phi * dy;

    float rx_sq = rx * rx;
    float ry_sq = ry * ry;
    float x1p_sq = x1p * x1p;
    float y1p_sq = y1p * y1p;

    float lambda = (x1p_sq / rx_sq) + (y1p_sq / ry_sq);
    if (lambda > 1.0f) {
        float factor = std::sqrt(lambda);
        rx *= factor;
        ry *= factor;
        rx_sq = rx * rx;
        ry_sq = ry * ry;
    }

    float sign = (large_arc == sweep) ? -1.0f : 1.0f;
    float sq = ((rx_sq * ry_sq) - (rx_sq * y1p_sq) - (ry_sq * x1p_sq)) /
               ((rx_sq * y1p_sq) + (ry_sq * x1p_sq));
    if (sq < 0.0f) {
        sq = 0.0f;
    }
    float coef = sign * std::sqrt(sq);
    float cxp = coef * ((rx * y1p) / ry);
    float cyp = coef * (-(ry * x1p) / rx);

    float cx = cos_phi * cxp - sin_phi * cyp + (from.x + to.x) * 0.5f;
    float cy = sin_phi * cxp + cos_phi * cyp + (from.y + to.y) * 0.5f;

    auto angle_between = [](float ux, float uy, float vx, float vy) noexcept {
        float dot = ux * vx + uy * vy;
        float len = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
        if (len < 1e-8f) {
            return 0.0f;
        }
        float val = dot / len;
        if (val < -1.0f) val = -1.0f;
        if (val > 1.0f) val = 1.0f;
        float ang = std::acos(val);
        if ((ux * vy - uy * vx) < 0.0f) {
            ang = -ang;
        }
        return ang;
    };

    float ux = (x1p - cxp) / rx;
    float uy = (y1p - cyp) / ry;
    float vx = (-x1p - cxp) / rx;
    float vy = (-y1p - cyp) / ry;

    float theta1 = angle_between(1.0f, 0.0f, ux, uy);
    float dtheta = angle_between(ux, uy, vx, vy);

    if (!sweep && dtheta > 0.0f) {
        dtheta -= kTwoPi;
    } else if (sweep && dtheta < 0.0f) {
        dtheta += kTwoPi;
    }

    int segments = static_cast<int>(std::ceil(std::abs(dtheta) / (kPi * 0.5f)));
    if (segments <= 0) {
        segments = 1;
    }
    float dt = dtheta / static_cast<float>(segments);

    float current_theta = theta1;
    for (int i = 0; i < segments; ++i) {
        float next_theta = current_theta + dt;
        float alpha = std::sin(dt) * (std::sqrt(4.0f + 3.0f * std::tan(dt * 0.5f) * std::tan(dt * 0.5f)) - 1.0f) / 3.0f;

        float cos_t1 = std::cos(current_theta);
        float sin_t1 = std::sin(current_theta);
        float cos_t2 = std::cos(next_theta);
        float sin_t2 = std::sin(next_theta);

        auto eval_point = [&](float c_t, float s_t) noexcept -> core::Point {
            float px = rx * c_t;
            float py = ry * s_t;
            return {
                cos_phi * px - sin_phi * py + cx,
                sin_phi * px + cos_phi * py + cy
            };
        };

        auto eval_derivative = [&](float c_t, float s_t) noexcept -> core::Point {
            float dx_dt = -rx * s_t;
            float dy_dt = ry * c_t;
            return {
                cos_phi * dx_dt - sin_phi * dy_dt,
                sin_phi * dx_dt + cos_phi * dy_dt
            };
        };

        core::Point p_start = eval_point(cos_t1, sin_t1);
        core::Point p_end = (i == segments - 1) ? to : eval_point(cos_t2, sin_t2);
        core::Point d_start = eval_derivative(cos_t1, sin_t1);
        core::Point d_end = eval_derivative(cos_t2, sin_t2);

        core::Point cp1{p_start.x + alpha * d_start.x, p_start.y + alpha * d_start.y};
        core::Point cp2{p_end.x - alpha * d_end.x, p_end.y - alpha * d_end.y};

        cubic_to(cp1, cp2, p_end);
        current_theta = next_theta;
    }
}

void Path::arc_to(core::Point to, core::Size radius, float x_axis_rotation_deg, bool large_arc,
                 bool sweep) noexcept {
    if (!has_subpath_) {
        move_to(core::Point{0.0f, 0.0f});
    }
    float rad = x_axis_rotation_deg * (kPi / 180.0f);
    arc_to_cubics(current_point_, to, radius.width, radius.height, rad, large_arc, sweep);
}

void Path::close() noexcept {
    if (has_subpath_) {
        verbs_.push_back(PathVerb::Close);
        current_point_ = subpath_start_;
        has_subpath_ = false;
    }
}

void Path::reset() noexcept {
    verbs_.clear();
    points_.clear();
    current_point_ = {0.0f, 0.0f};
    subpath_start_ = {0.0f, 0.0f};
    has_subpath_ = false;
}

void Path::add_rect(core::Rect rect) noexcept {
    move_to(rect.x, rect.y);
    line_to(rect.x + rect.width, rect.y);
    line_to(rect.x + rect.width, rect.y + rect.height);
    line_to(rect.x, rect.y + rect.height);
    close();
}

void Path::add_rounded_rect(core::Rect rect, core::CornerRadii radii) noexcept {
    float x = rect.x;
    float y = rect.y;
    float w = rect.width;
    float h = rect.height;

    float rtl = std::min({radii.top_left, w * 0.5f, h * 0.5f});
    float rtr = std::min({radii.top_right, w * 0.5f, h * 0.5f});
    float rbr = std::min({radii.bottom_right, w * 0.5f, h * 0.5f});
    float rbl = std::min({radii.bottom_left, w * 0.5f, h * 0.5f});

    move_to(x + rtl, y);
    line_to(x + w - rtr, y);
    if (rtr > 0.0f) {
        cubic_to(x + w - rtr * (1.0f - kKappa), y, x + w, y + rtr * (1.0f - kKappa), x + w, y + rtr);
    }
    line_to(x + w, y + h - rbr);
    if (rbr > 0.0f) {
        cubic_to(x + w, y + h - rbr * (1.0f - kKappa), x + w - rbr * (1.0f - kKappa), y + h, x + w - rbr, y + h);
    }
    line_to(x + rbl, y + h);
    if (rbl > 0.0f) {
        cubic_to(x + rbl * (1.0f - kKappa), y + h, x, y + h - rbl * (1.0f - kKappa), x, y + h - rbl);
    }
    line_to(x, y + rtl);
    if (rtl > 0.0f) {
        cubic_to(x, y + rtl * (1.0f - kKappa), x + rtl * (1.0f - kKappa), y, x + rtl, y);
    }
    close();
}

void Path::add_circle(core::Point center, float radius) noexcept {
    add_ellipse(center, core::Size{radius, radius});
}

void Path::add_ellipse(core::Point center, core::Size radius) noexcept {
    float rx = radius.width;
    float ry = radius.height;
    float cx = center.x;
    float cy = center.y;
    float ox = rx * kKappa;
    float oy = ry * kKappa;

    move_to(cx - rx, cy);
    cubic_to(cx - rx, cy - oy, cx - ox, cy - ry, cx, cy - ry);
    cubic_to(cx + ox, cy - ry, cx + rx, cy - oy, cx + rx, cy);
    cubic_to(cx + rx, cy + oy, cx + ox, cy + ry, cx, cy + ry);
    cubic_to(cx - ox, cy + ry, cx - rx, cy + oy, cx - rx, cy);
    close();
}

void Path::translate(core::Point offset) noexcept {
    for (auto& pt : points_) {
        pt.x += offset.x;
        pt.y += offset.y;
    }
    current_point_.x += offset.x;
    current_point_.y += offset.y;
    subpath_start_.x += offset.x;
    subpath_start_.y += offset.y;
}

void Path::scale(float sx, float sy) noexcept {
    for (auto& pt : points_) {
        pt.x *= sx;
        pt.y *= sy;
    }
    current_point_.x *= sx;
    current_point_.y *= sy;
    subpath_start_.x *= sx;
    subpath_start_.y *= sy;
}

void Path::transform(float m00, float m01, float m10, float m11, float tx, float ty) noexcept {
    auto apply = [&](core::Point p) noexcept -> core::Point {
        return {
            m00 * p.x + m01 * p.y + tx,
            m10 * p.x + m11 * p.y + ty
        };
    };
    for (auto& pt : points_) {
        pt = apply(pt);
    }
    current_point_ = apply(current_point_);
    subpath_start_ = apply(subpath_start_);
}

core::Rect Path::bounds() const noexcept {
    if (points_.empty()) {
        return core::Rect{0.0f, 0.0f, 0.0f, 0.0f};
    }
    float min_x = points_[0].x;
    float min_y = points_[0].y;
    float max_x = points_[0].x;
    float max_y = points_[0].y;

    for (const auto& pt : points_) {
        if (pt.x < min_x) min_x = pt.x;
        if (pt.x > max_x) max_x = pt.x;
        if (pt.y < min_y) min_y = pt.y;
        if (pt.y > max_y) max_y = pt.y;
    }
    return core::Rect{min_x, min_y, max_x - min_x, max_y - min_y};
}

Path Path::flatten(float tolerance) const {
    Path out;
    if (empty()) {
        return out;
    }

    float tol_sq = tolerance * tolerance;
    size_t pt_idx = 0;
    core::Point cur{0.0f, 0.0f};

    for (auto verb : verbs_) {
        switch (verb) {
        case PathVerb::MoveTo:
            cur = points_[pt_idx++];
            out.move_to(cur);
            break;
        case PathVerb::LineTo: {
            core::Point next = points_[pt_idx++];
            out.line_to(next);
            cur = next;
            break;
        }
        case PathVerb::QuadTo: {
            core::Point ctrl = points_[pt_idx++];
            core::Point to = points_[pt_idx++];
            flatten_quad(cur, ctrl, to, tol_sq, 0, out);
            cur = to;
            break;
        }
        case PathVerb::CubicTo: {
            core::Point ctrl1 = points_[pt_idx++];
            core::Point ctrl2 = points_[pt_idx++];
            core::Point to = points_[pt_idx++];
            flatten_cubic(cur, ctrl1, ctrl2, to, tol_sq, 0, out);
            cur = to;
            break;
        }
        case PathVerb::Close:
            out.close();
            cur = out.current_point();
            break;
        }
    }

    return out;
}

} // namespace north::render
