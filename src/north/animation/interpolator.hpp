#pragma once

#include "north/core/types.hpp"

#include <algorithm>
#include <cmath>

namespace north::animation {

template <typename T>
struct Interpolator {
    [[nodiscard]] static T lerp(const T& a, const T& b, float t) noexcept {
        return a + (b - a) * t;
    }

    [[nodiscard]] static float distance(const T& a, const T& b) noexcept { return std::abs(b - a); }
};

template <>
struct Interpolator<core::Point> {
    [[nodiscard]] static core::Point lerp(const core::Point& a, const core::Point& b,
                                          float t) noexcept {
        return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
    }

    [[nodiscard]] static float distance(const core::Point& a, const core::Point& b) noexcept {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    }
};

template <>
struct Interpolator<core::Size> {
    [[nodiscard]] static core::Size lerp(const core::Size& a, const core::Size& b,
                                         float t) noexcept {
        return {a.width + (b.width - a.width) * t, a.height + (b.height - a.height) * t};
    }

    [[nodiscard]] static float distance(const core::Size& a, const core::Size& b) noexcept {
        float dw = b.width - a.width;
        float dh = b.height - a.height;
        return (std::max)(std::abs(dw), std::abs(dh));
    }
};

template <>
struct Interpolator<core::Rect> {
    [[nodiscard]] static core::Rect lerp(const core::Rect& a, const core::Rect& b,
                                         float t) noexcept {
        return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.width + (b.width - a.width) * t,
                a.height + (b.height - a.height) * t};
    }

    [[nodiscard]] static float distance(const core::Rect& a, const core::Rect& b) noexcept {
        float dx = std::abs(b.x - a.x);
        float dy = std::abs(b.y - a.y);
        float dw = std::abs(b.width - a.width);
        float dh = std::abs(b.height - a.height);
        return (std::max)({dx, dy, dw, dh});
    }
};

template <>
struct Interpolator<core::Color> {
    [[nodiscard]] static core::Color lerp(const core::Color& a, const core::Color& b,
                                          float t) noexcept {
        return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
                a.a + (b.a - a.a) * t};
    }

    [[nodiscard]] static float distance(const core::Color& a, const core::Color& b) noexcept {
        float dr = std::abs(b.r - a.r);
        float dg = std::abs(b.g - a.g);
        float db = std::abs(b.b - a.b);
        float da = std::abs(b.a - a.a);
        return (std::max)({dr, dg, db, da});
    }
};

} // namespace north::animation
