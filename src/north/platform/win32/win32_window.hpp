#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "north/core/types.hpp"
#include "north/gpu/d3d11/d3d11_device.hpp"
#include "north/platform/window_listener.hpp"

#include <string_view>

#include <windows.h>

namespace north::platform::win32 {

struct WindowDesc {
    std::string_view title{"North Toolkit Window"};
    uint32_t width{1024};
    uint32_t height{768};
    bool resizable{true};
    uint32_t min_width{320};
    uint32_t min_height{240};
};

class Win32Window {
public:
    Win32Window();
    ~Win32Window();

    Win32Window(const Win32Window&) = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    [[nodiscard]] bool create(const WindowDesc& desc, gpu::D3D11Device* device = nullptr);
    void destroy();

    void show(bool visible = true);
    void set_listener(IWindowListener* listener) noexcept { listener_ = listener; }
    void set_ime_position(core::Point screen_pos);

    [[nodiscard]] HWND hwnd() const noexcept { return hwnd_; }
    [[nodiscard]] core::Size size() const noexcept { return size_; }
    [[nodiscard]] float dpi_scale() const noexcept { return dpi_scale_; }
    [[nodiscard]] uint32_t min_width() const noexcept { return min_width_; }
    [[nodiscard]] uint32_t min_height() const noexcept { return min_height_; }
    [[nodiscard]] bool is_open() const noexcept { return hwnd_ != nullptr; }

    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
    LRESULT handle_message(UINT msg, WPARAM wparam, LPARAM lparam);
    void update_dpi();

    HWND hwnd_{nullptr};
    IWindowListener* listener_{nullptr};
    gpu::D3D11Device* gpu_device_{nullptr};
    core::Size size_{0.0f, 0.0f};
    float dpi_scale_{1.0f};
    uint32_t min_width_{320};
    uint32_t min_height_{240};

    static constexpr const wchar_t* kWindowClassName = L"NorthToolkitWindowClass";
    static bool class_registered_;
};

} // namespace north::platform::win32
