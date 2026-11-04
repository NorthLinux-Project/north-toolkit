#pragma once

#include <algorithm>
#include <cmath>

namespace north::animation {

struct SpringDesc {
    float stiffness{180.0f};
    float damping{12.0f};
    float mass{1.0f};

    [[nodiscard]] static constexpr SpringDesc snappy() noexcept { return {300.0f, 20.0f, 1.0f}; }

    [[nodiscard]] static constexpr SpringDesc smooth() noexcept { return {150.0f, 16.0f, 1.0f}; }

    [[nodiscard]] static constexpr SpringDesc gentle() noexcept { return {100.0f, 14.0f, 1.0f}; }
};

class CubicBezier {
public:
    constexpr CubicBezier(float x1, float y1, float x2, float y2) noexcept
        : x1_(std::clamp(x1, 0.0f, 1.0f)), y1_(y1), x2_(std::clamp(x2, 0.0f, 1.0f)), y2_(y2) {}

    [[nodiscard]] float solve(float x) const noexcept;

    [[nodiscard]] static constexpr CubicBezier linear() noexcept {
        return {0.0f, 0.0f, 1.0f, 1.0f};
    }

    [[nodiscard]] static constexpr CubicBezier ease_in() noexcept {
        return {0.42f, 0.0f, 1.0f, 1.0f};
    }

    [[nodiscard]] static constexpr CubicBezier ease_out() noexcept {
        return {0.0f, 0.0f, 0.58f, 1.0f};
    }

    [[nodiscard]] static constexpr CubicBezier ease_in_out() noexcept {
        return {0.42f, 0.0f, 0.58f, 1.0f};
    }

private:
    float x1_;
    float y1_;
    float x2_;
    float y2_;

    [[nodiscard]] float sample_curve_x(float t) const noexcept;
    [[nodiscard]] float sample_curve_y(float t) const noexcept;
    [[nodiscard]] float sample_curve_derivative_x(float t) const noexcept;
    [[nodiscard]] float solve_curve_x(float x) const noexcept;
};

void step_spring(float& position, float& velocity, float target, float dt,
                 const SpringDesc& desc) noexcept;

} // namespace north::animation
