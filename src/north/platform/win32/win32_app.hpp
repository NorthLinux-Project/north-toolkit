#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "north/platform/win32/win32_window.hpp"

#include <vector>

#include <windows.h>

namespace north::platform::win32 {

using IdleCallback = void (*)();

class Win32App {
public:
    Win32App();
    ~Win32App();

    Win32App(const Win32App&) = delete;
    Win32App& operator=(const Win32App&) = delete;

    static Win32App& instance();

    void register_window(Win32Window* window);
    void unregister_window(Win32Window* window);

    void set_idle_callback(IdleCallback callback) noexcept { idle_callback_ = callback; }
    void trigger_render() noexcept {
        if (idle_callback_) {
            idle_callback_();
        }
    }

    void wake() noexcept;

    void run();
    void pump_events();
    void quit();

    [[nodiscard]] bool is_running() const noexcept { return running_; }
    [[nodiscard]] size_t window_count() const noexcept { return windows_.size(); }

private:
    std::vector<Win32Window*> windows_;
    IdleCallback idle_callback_{nullptr};
    bool running_{false};
    DWORD main_thread_id_{0};
    LARGE_INTEGER perf_freq_{};
    LARGE_INTEGER last_tick_{};
};

} // namespace north::platform::win32
