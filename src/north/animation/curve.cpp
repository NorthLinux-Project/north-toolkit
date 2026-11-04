#include "north/animation/curve.hpp"

#include <cmath>

namespace north::animation {

float CubicBezier::sample_curve_x(float t) const noexcept {
    float inv_t = 1.0f - t;
    return 3.0f * inv_t * inv_t * t * x1_ + 3.0f * inv_t * t * t * x2_ + t * t * t;
}

float CubicBezier::sample_curve_y(float t) const noexcept {
    float inv_t = 1.0f - t;
    return 3.0f * inv_t * inv_t * t * y1_ + 3.0f * inv_t * t * t * y2_ + t * t * t;
}

float CubicBezier::sample_curve_derivative_x(float t) const noexcept {
    float inv_t = 1.0f - t;
    return 3.0f * inv_t * inv_t * x1_ + 6.0f * inv_t * t * (x2_ - x1_) +
           3.0f * t * t * (1.0f - x2_);
}

float CubicBezier::solve_curve_x(float x) const noexcept {
    float t = x;
    for (int i = 0; i < 8; ++i) {
        float x_curr = sample_curve_x(t) - x;
        if (std::abs(x_curr) < 1e-5f) {
            return t;
        }
        float d = sample_curve_derivative_x(t);
        if (std::abs(d) < 1e-6f) {
            break;
        }
        t -= x_curr / d;
    }

    float t_min = 0.0f;
    float t_max = 1.0f;
    t = x;

    for (int iter = 0; iter < 16; ++iter) {
        float x_curr = sample_curve_x(t);
        if (std::abs(x_curr - x) < 1e-5f) {
            return t;
        }
        if (x > x_curr) {
            t_min = t;
        } else {
            t_max = t;
        }
        t = (t_max + t_min) * 0.5f;
    }

    return t;
}

float CubicBezier::solve(float x) const noexcept {
    if (x <= 0.0f)
        return 0.0f;
    if (x >= 1.0f)
        return 1.0f;
    return sample_curve_y(solve_curve_x(x));
}

void step_spring(float& position, float& velocity, float target, float dt,
                 const SpringDesc& desc) noexcept {
    if (dt <= 0.0f)
        return;

    float m = (std::max)(0.001f, desc.mass);
    float k = (std::max)(0.0f, desc.stiffness);
    float c = (std::max)(0.0f, desc.damping);

    float omega_0 = std::sqrt(k / m);
    float zeta = c / (2.0f * std::sqrt(k * m));

    float x0 = position - target;
    float v0 = velocity;

    float x_dt = 0.0f;
    float v_dt = 0.0f;

    if (std::abs(zeta - 1.0f) < 1e-4f) {
        // Critically damped
        float decay = std::exp(-omega_0 * dt);
        float term = v0 + omega_0 * x0;
        x_dt = (x0 + term * dt) * decay;
        v_dt = (v0 - term * omega_0 * dt) * decay;
    } else if (zeta < 1.0f) {
        // Under-damped (harmonic with decay envelope)
        float omega_d = omega_0 * std::sqrt(1.0f - zeta * zeta);
        float alpha = zeta * omega_0;
        float decay = std::exp(-alpha * dt);
        float sin_t = std::sin(omega_d * dt);
        float cos_t = std::cos(omega_d * dt);

        float b = (v0 + alpha * x0) / omega_d;
        x_dt = decay * (x0 * cos_t + b * sin_t);
        v_dt = decay * (v0 * cos_t - (alpha * b + omega_d * x0) * sin_t);
    } else {
        // Over-damped
        float omega_d = omega_0 * std::sqrt(zeta * zeta - 1.0f);
        float r1 = -zeta * omega_0 + omega_d;
        float r2 = -zeta * omega_0 - omega_d;

        float c2 = (v0 - r1 * x0) / (r2 - r1);
        float c1 = x0 - c2;

        float e1 = std::exp(r1 * dt);
        float e2 = std::exp(r2 * dt);

        x_dt = c1 * e1 + c2 * e2;
        v_dt = c1 * r1 * e1 + c2 * r2 * e2;
    }

    position = target + x_dt;
    velocity = v_dt;
}

} // namespace north::animation
