#pragma once

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace north::core {

enum DirtyFlags : uint32_t {
    kNone = 0,
    kLayout = 1 << 0,
    kPaint = 1 << 1,
    kTransform = 1 << 2,
    kChildren = 1 << 3,
    kAll = 0xFFFFFFFF
};

using DirtyCallback = void (*)(void* user_data, uint32_t flags);

template <typename T>
class Signal {
public:
    explicit Signal(T initial_value = {}) : value_(std::move(initial_value)) {}

    [[nodiscard]] const T& get() const noexcept { return value_; }

    void set(T new_value) {
        if (!(value_ == new_value)) {
            value_ = std::move(new_value);
            notify_observers();
        }
    }

    void bind_dirty(DirtyCallback cb, void* user_data, uint32_t flags = kAll) {
        if (!cb)
            return;
        Observer obs{.cb = cb, .user_data = user_data, .flags = flags};
        if (inline_count_ < 2) {
            inline_observers_[inline_count_++] = obs;
        } else {
            overflow_observers_.push_back(obs);
        }
    }

    void unbind_dirty(void* user_data) noexcept {
        if (!user_data)
            return;
        for (uint8_t i = 0; i < inline_count_;) {
            if (inline_observers_[i].user_data == user_data) {
                for (uint8_t j = i; j + 1 < inline_count_; ++j) {
                    inline_observers_[j] = inline_observers_[j + 1];
                }
                --inline_count_;
                inline_observers_[inline_count_] = Observer{};
            } else {
                ++i;
            }
        }
        for (auto it = overflow_observers_.begin(); it != overflow_observers_.end();) {
            if (it->user_data == user_data) {
                it = overflow_observers_.erase(it);
            } else {
                ++it;
            }
        }
    }

    [[nodiscard]] size_t observer_count() const noexcept {
        return inline_count_ + overflow_observers_.size();
    }

    [[nodiscard]] bool is_bound(void* user_data, DirtyCallback cb) const noexcept {
        for (uint8_t i = 0; i < inline_count_; ++i) {
            if (inline_observers_[i].user_data == user_data && inline_observers_[i].cb == cb) {
                return true;
            }
        }
        for (const auto& obs : overflow_observers_) {
            if (obs.user_data == user_data && obs.cb == cb) {
                return true;
            }
        }
        return false;
    }

private:
    struct Observer {
        DirtyCallback cb{nullptr};
        void* user_data{nullptr};
        uint32_t flags{kAll};
    };

    T value_{};
    std::array<Observer, 2> inline_observers_{};
    uint8_t inline_count_{0};
    std::vector<Observer> overflow_observers_;

    void notify_observers() const {
        if (inline_count_ == 0 && overflow_observers_.empty()) {
            return;
        }

        Observer local_buf[8];
        size_t total = inline_count_ + overflow_observers_.size();
        std::vector<Observer> heap_buf;
        Observer* list = local_buf;
        if (total > 8) {
            heap_buf.resize(total);
            list = heap_buf.data();
        }
        for (uint8_t i = 0; i < inline_count_; ++i) {
            list[i] = inline_observers_[i];
        }
        for (size_t i = 0; i < overflow_observers_.size(); ++i) {
            list[inline_count_ + i] = overflow_observers_[i];
        }

        for (size_t i = 0; i < total; ++i) {
            if (list[i].cb && is_bound(list[i].user_data, list[i].cb)) {
                list[i].cb(list[i].user_data, list[i].flags);
            }
        }
    }
};

template <typename T, typename ComputeFn>
class Computed {
public:
    explicit Computed(ComputeFn fn) : compute_fn_(std::move(fn)), dirty_(true) {}

    [[nodiscard]] const T& get() {
        if (dirty_) {
            cached_value_ = compute_fn_();
            dirty_ = false;
        }
        return cached_value_;
    }

    void mark_dirty() noexcept { dirty_ = true; }

    [[nodiscard]] bool is_dirty() const noexcept { return dirty_; }

    static void on_dependency_dirty(void* user_data, uint32_t) noexcept {
        if (user_data) {
            static_cast<Computed*>(user_data)->mark_dirty();
        }
    }

private:
    ComputeFn compute_fn_;
    T cached_value_{};
    bool dirty_{true};
};

template <typename EffectFn>
class Effect {
public:
    explicit Effect(EffectFn fn) : effect_fn_(std::move(fn)) {}

    void run() { effect_fn_(); }

    static void on_dependency_dirty(void* user_data, uint32_t) {
        if (user_data) {
            static_cast<Effect*>(user_data)->run();
        }
    }

private:
    EffectFn effect_fn_;
};

} // namespace north::core
