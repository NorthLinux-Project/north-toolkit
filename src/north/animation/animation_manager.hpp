#pragma once

#include <cstddef>
#include <vector>

namespace north::animation {

class IAnimatable {
public:
    virtual ~IAnimatable() = default;
    virtual bool step(float delta_seconds) = 0;
};

class AnimationManager {
public:
    static AnimationManager& instance() noexcept;

    void register_animator(IAnimatable* anim);
    void unregister_animator(IAnimatable* anim);

    size_t tick(float delta_seconds);

    [[nodiscard]] bool has_active_animations() const noexcept { return !active_animators_.empty(); }

    [[nodiscard]] size_t active_count() const noexcept { return active_animators_.size(); }

    void set_frame_callback(void (*fn)(void*), void* user_data = nullptr) noexcept {
        frame_cb_ = fn;
        frame_cb_ud_ = user_data;
    }

    void request_frame() noexcept {
        if (frame_cb_) {
            frame_cb_(frame_cb_ud_);
        }
    }

    void clear() noexcept;

private:
    AnimationManager();
    ~AnimationManager() = default;
    AnimationManager(const AnimationManager&) = delete;
    AnimationManager& operator=(const AnimationManager&) = delete;

    std::vector<IAnimatable*> active_animators_;
    std::vector<IAnimatable*> pending_adds_;
    std::vector<IAnimatable*> pending_removes_;
    bool is_ticking_{false};

    void (*frame_cb_)(void*){nullptr};
    void* frame_cb_ud_{nullptr};
};

} // namespace north::animation
