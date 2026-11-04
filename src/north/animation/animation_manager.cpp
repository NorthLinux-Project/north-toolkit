#include "north/animation/animation_manager.hpp"

#include <algorithm>

namespace north::animation {

AnimationManager::AnimationManager() {
    active_animators_.reserve(64);
    pending_adds_.reserve(16);
    pending_removes_.reserve(16);
}

AnimationManager& AnimationManager::instance() noexcept {
    static AnimationManager s_instance;
    return s_instance;
}

void AnimationManager::register_animator(IAnimatable* anim) {
    if (!anim)
        return;

    if (is_ticking_) {
        if (std::find(pending_adds_.begin(), pending_adds_.end(), anim) == pending_adds_.end() &&
            std::find(active_animators_.begin(), active_animators_.end(), anim) ==
                active_animators_.end()) {
            pending_adds_.push_back(anim);
        }
    } else {
        if (std::find(active_animators_.begin(), active_animators_.end(), anim) ==
            active_animators_.end()) {
            active_animators_.push_back(anim);
        }
    }
    request_frame();
}

void AnimationManager::unregister_animator(IAnimatable* anim) {
    if (!anim)
        return;

    if (is_ticking_) {
        if (std::find(pending_removes_.begin(), pending_removes_.end(), anim) ==
            pending_removes_.end()) {
            pending_removes_.push_back(anim);
        }
        auto it = std::find(pending_adds_.begin(), pending_adds_.end(), anim);
        if (it != pending_adds_.end()) {
            pending_adds_.erase(it);
        }
    } else {
        auto it = std::find(active_animators_.begin(), active_animators_.end(), anim);
        if (it != active_animators_.end()) {
            active_animators_.erase(it);
        }
    }
}

size_t AnimationManager::tick(float delta_seconds) {
    if (active_animators_.empty() && pending_adds_.empty()) {
        return 0;
    }

    is_ticking_ = true;

    for (size_t i = 0; i < active_animators_.size(); ++i) {
        auto* anim = active_animators_[i];
        if (!anim)
            continue;

        // Skip if pending removal
        if (std::find(pending_removes_.begin(), pending_removes_.end(), anim) !=
            pending_removes_.end()) {
            continue;
        }

        bool running = anim->step(delta_seconds);
        if (!running) {
            pending_removes_.push_back(anim);
        }
    }

    is_ticking_ = false;

    // Apply pending removals
    for (auto* rem : pending_removes_) {
        auto it = std::find(active_animators_.begin(), active_animators_.end(), rem);
        if (it != active_animators_.end()) {
            active_animators_.erase(it);
        }
    }
    pending_removes_.clear();

    // Apply pending additions
    for (auto* add : pending_adds_) {
        if (std::find(active_animators_.begin(), active_animators_.end(), add) ==
            active_animators_.end()) {
            active_animators_.push_back(add);
        }
    }
    pending_adds_.clear();

    if (!active_animators_.empty()) {
        request_frame();
    }

    return active_animators_.size();
}

void AnimationManager::clear() noexcept {
    active_animators_.clear();
    pending_adds_.clear();
    pending_removes_.clear();
    is_ticking_ = false;
}

} // namespace north::animation
