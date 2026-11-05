#include "north/platform/win32/win32_app.hpp"

#include "north/animation/animation_manager.hpp"

#include <algorithm>

namespace north::platform::win32 {

Win32App& Win32App::instance() {
    static Win32App app;
    return app;
}

Win32App::Win32App() {
    main_thread_id_ = GetCurrentThreadId();
    QueryPerformanceFrequency(&perf_freq_);
    QueryPerformanceCounter(&last_tick_);
    animation::AnimationManager::instance().set_frame_callback(
        [](void* ud) {
            if (ud) {
                static_cast<Win32App*>(ud)->wake();
            }
        },
        this);
}

Win32App::~Win32App() {
    animation::AnimationManager::instance().set_frame_callback(nullptr, nullptr);
    windows_.clear();
}

void Win32App::wake() noexcept {
    if (main_thread_id_ != 0) {
        PostThreadMessageW(main_thread_id_, WM_NULL, 0, 0);
    }
}

void Win32App::register_window(Win32Window* window) {
    if (window && std::find(windows_.begin(), windows_.end(), window) == windows_.end()) {
        windows_.push_back(window);
    }
}

void Win32App::unregister_window(Win32Window* window) {
    auto it = std::find(windows_.begin(), windows_.end(), window);
    if (it != windows_.end()) {
        windows_.erase(it);
    }
    if (windows_.empty() && running_) {
        quit();
    }
}

void Win32App::pump_events() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            running_ = false;
            break;
        }
        if (msg.message == WM_MOUSEMOVE) {
            MSG next_msg;
            if (PeekMessageW(&next_msg, msg.hwnd, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_NOREMOVE)) {
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void Win32App::run() {
    running_ = true;
    main_thread_id_ = GetCurrentThreadId();
    QueryPerformanceFrequency(&perf_freq_);
    QueryPerformanceCounter(&last_tick_);

    while (running_) {
        pump_events();

        if (!running_) {
            break;
        }

        bool has_anims = animation::AnimationManager::instance().has_active_animations();
        if (has_anims) {
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            float dt = 0.0f;
            if (perf_freq_.QuadPart > 0) {
                dt = static_cast<float>(now.QuadPart - last_tick_.QuadPart) /
                     static_cast<float>(perf_freq_.QuadPart);
            }
            last_tick_ = now;
            dt = (std::min)(dt, 0.1f);
            animation::AnimationManager::instance().tick(dt);
            has_anims = animation::AnimationManager::instance().has_active_animations();
        } else {
            QueryPerformanceCounter(&last_tick_);
        }

        if (idle_callback_) {
            idle_callback_();
        }

        DWORD timeout = has_anims ? 1 : INFINITE;
        MsgWaitForMultipleObjectsEx(0, nullptr, timeout, QS_ALLINPUT, MWMO_ALERTABLE);
    }
}

void Win32App::quit() {
    running_ = false;
    PostQuitMessage(0);
}

} // namespace north::platform::win32
