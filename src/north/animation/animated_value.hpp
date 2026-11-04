#pragma once

#include "north/animation/animation_manager.hpp"
#include "north/animation/curve.hpp"
#include "north/animation/interpolator.hpp"
#include "north/core/signal.hpp"

namespace north::animation {

namespace detail {

inline void step_val_spring(float& current, float& vel, float target, float dt,
                            const SpringDesc& desc) noexcept {
    step_spring(current, vel, target, dt, desc);
}

inline void step_val_spring(core::Point& current, core::Point& vel, const core::Point& target,
                            float dt, const SpringDesc& desc) noexcept {
    step_spring(current.x, vel.x, target.x, dt, desc);
    step_spring(current.y, vel.y, target.y, dt, desc);
}

inline void step_val_spring(core::Size& current, core::Size& vel, const core::Size& target,
                            float dt, const SpringDesc& desc) noexcept {
    step_spring(current.width, vel.width, target.width, dt, desc);
    step_spring(current.height, vel.height, target.height, dt, desc);
}

inline void step_val_spring(core::Rect& current, core::Rect& vel, const core::Rect& target,
                            float dt, const SpringDesc& desc) noexcept {
    step_spring(current.x, vel.x, target.x, dt, desc);
    step_spring(current.y, vel.y, target.y, dt, desc);
    step_spring(current.width, vel.width, target.width, dt, desc);
    step_spring(current.height, vel.height, target.height, dt, desc);
}

inline void step_val_spring(core::Color& current, core::Color& vel, const core::Color& target,
                            float dt, const SpringDesc& desc) noexcept {
    step_spring(current.r, vel.r, target.r, dt, desc);
    step_spring(current.g, vel.g, target.g, dt, desc);
    step_spring(current.b, vel.b, target.b, dt, desc);
    step_spring(current.a, vel.a, target.a, dt, desc);
}

inline bool is_vel_settled(float v, float eps) noexcept {
    return std::abs(v) < eps;
}

inline bool is_vel_settled(const core::Point& v, float eps) noexcept {
    return std::abs(v.x) < eps && std::abs(v.y) < eps;
}

inline bool is_vel_settled(const core::Size& v, float eps) noexcept {
    return std::abs(v.width) < eps && std::abs(v.height) < eps;
}

inline bool is_vel_settled(const core::Rect& v, float eps) noexcept {
    return std::abs(v.x) < eps && std::abs(v.y) < eps && std::abs(v.width) < eps &&
           std::abs(v.height) < eps;
}

inline bool is_vel_settled(const core::Color& v, float eps) noexcept {
    return std::abs(v.r) < eps && std::abs(v.g) < eps && std::abs(v.b) < eps && std::abs(v.a) < eps;
}

} // namespace detail

enum class AnimMode : uint8_t { None, Spring, Tween };

template <typename T>
class AnimatedValue : public IAnimatable {
public:
    explicit AnimatedValue(const T& initial_value = T{})
        : signal_(initial_value), current_(initial_value), target_(initial_value),
          start_value_(initial_value), velocity_{} {}

    ~AnimatedValue() override { AnimationManager::instance().unregister_animator(this); }

    AnimatedValue(const AnimatedValue&) = delete;
    AnimatedValue& operator=(const AnimatedValue&) = delete;

    AnimatedValue(AnimatedValue&& other) noexcept
        : signal_(std::move(other.signal_)), current_(std::move(other.current_)),
          target_(std::move(other.target_)), start_value_(std::move(other.start_value_)),
          velocity_(std::move(other.velocity_)), mode_(other.mode_),
          spring_desc_(other.spring_desc_), tween_curve_(other.tween_curve_),
          duration_(other.duration_), elapsed_(other.elapsed_), epsilon_(other.epsilon_),
          vel_epsilon_(other.vel_epsilon_) {
        AnimationManager::instance().unregister_animator(&other);
        other.mode_ = AnimMode::None;
        if (mode_ != AnimMode::None) {
            AnimationManager::instance().register_animator(this);
        }
    }

    AnimatedValue& operator=(AnimatedValue&& other) noexcept {
        if (this != &other) {
            AnimationManager::instance().unregister_animator(this);
            AnimationManager::instance().unregister_animator(&other);
            other.mode_ = AnimMode::None;

            signal_ = std::move(other.signal_);
            current_ = std::move(other.current_);
            target_ = std::move(other.target_);
            start_value_ = std::move(other.start_value_);
            velocity_ = std::move(other.velocity_);
            mode_ = other.mode_;
            spring_desc_ = other.spring_desc_;
            tween_curve_ = other.tween_curve_;
            duration_ = other.duration_;
            elapsed_ = other.elapsed_;
            epsilon_ = other.epsilon_;
            vel_epsilon_ = other.vel_epsilon_;

            if (mode_ != AnimMode::None) {
                AnimationManager::instance().register_animator(this);
            }
        }
        return *this;
    }

    [[nodiscard]] const T& get() const noexcept { return signal_.get(); }
    [[nodiscard]] const T& target() const noexcept { return target_; }
    [[nodiscard]] bool is_animating() const noexcept { return mode_ != AnimMode::None; }

    [[nodiscard]] core::Signal<T>& signal() noexcept { return signal_; }
    [[nodiscard]] const core::Signal<T>& signal() const noexcept { return signal_; }

    void bind_dirty(void (*fn)(void*, uint32_t), void* ctx = nullptr,
                    uint32_t flags = core::kPaint) {
        signal_.bind_dirty(fn, ctx, flags);
    }

    void set_immediate(const T& value) {
        if (mode_ != AnimMode::None) {
            AnimationManager::instance().unregister_animator(this);
            mode_ = AnimMode::None;
        }
        current_ = value;
        target_ = value;
        velocity_ = T{};
        signal_.set(value);
    }

    void spring_to(const T& target, const SpringDesc& desc = SpringDesc{}) {
        target_ = target;
        spring_desc_ = desc;
        mode_ = AnimMode::Spring;
        AnimationManager::instance().register_animator(this);
    }

    void tween_to(const T& target, float duration_seconds,
                  const CubicBezier& curve = CubicBezier::ease_in_out()) {
        start_value_ = current_;
        target_ = target;
        duration_ = (std::max)(0.001f, duration_seconds);
        elapsed_ = 0.0f;
        tween_curve_ = curve;
        mode_ = AnimMode::Tween;
        AnimationManager::instance().register_animator(this);
    }

    void set_tolerances(float pos_eps, float vel_eps) noexcept {
        epsilon_ = pos_eps;
        vel_epsilon_ = vel_eps;
    }

    bool step(float delta_seconds) override {
        if (mode_ == AnimMode::None) {
            return false;
        }

        if (mode_ == AnimMode::Spring) {
            detail::step_val_spring(current_, velocity_, target_, delta_seconds, spring_desc_);
            signal_.set(current_);

            float dist = Interpolator<T>::distance(current_, target_);
            if (dist < epsilon_ && detail::is_vel_settled(velocity_, vel_epsilon_)) {
                current_ = target_;
                velocity_ = T{};
                signal_.set(current_);
                mode_ = AnimMode::None;
                return false;
            }
            return true;
        }

        if (mode_ == AnimMode::Tween) {
            elapsed_ += delta_seconds;
            float t = std::clamp(elapsed_ / duration_, 0.0f, 1.0f);
            float factor = tween_curve_.solve(t);
            current_ = Interpolator<T>::lerp(start_value_, target_, factor);
            signal_.set(current_);

            if (t >= 1.0f) {
                current_ = target_;
                signal_.set(current_);
                mode_ = AnimMode::None;
                return false;
            }
            return true;
        }

        return false;
    }

private:
    core::Signal<T> signal_;
    T current_;
    T target_;
    T start_value_;
    T velocity_{};
    AnimMode mode_{AnimMode::None};
    SpringDesc spring_desc_{};
    CubicBezier tween_curve_{CubicBezier::linear()};
    float duration_{0.0f};
    float elapsed_{0.0f};
    float epsilon_{0.001f};
    float vel_epsilon_{0.01f};
};

} // namespace north::animation
