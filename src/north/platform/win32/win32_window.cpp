#include "north/platform/win32/win32_window.hpp"

#include "north/platform/win32/win32_app.hpp"

#include <string>

#include <imm.h>
#include <windowsx.h>

namespace north::platform::win32 {

bool Win32Window::class_registered_ = false;

namespace {

uint32_t get_modifiers() noexcept {
    uint32_t mods = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000)
        mods |= kModShift;
    if (GetKeyState(VK_CONTROL) & 0x8000)
        mods |= kModControl;
    if (GetKeyState(VK_MENU) & 0x8000)
        mods |= kModAlt;
    return mods;
}

} // namespace

Win32Window::Win32Window() = default;

Win32Window::~Win32Window() {
    destroy();
}

bool Win32Window::create(const WindowDesc& desc, gpu::D3D11Device* device) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HINSTANCE hinst = GetModuleHandleW(nullptr);

    if (!class_registered_) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &Win32Window::window_proc;
        wc.hInstance = hinst;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.hbrBackground = nullptr;
        wc.lpszClassName = kWindowClassName;

        if (!RegisterClassExW(&wc)) {
            return false;
        }
        class_registered_ = true;
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!desc.resizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }

    RECT rc{0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height)};
    AdjustWindowRectEx(&rc, style, FALSE, 0);

    std::wstring wtitle(desc.title.begin(), desc.title.end());

    hwnd_ =
        CreateWindowExW(0, kWindowClassName, wtitle.c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT,
                        rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hinst, this);

    if (!hwnd_) {
        return false;
    }

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    update_dpi();

    RECT client_rc{};
    GetClientRect(hwnd_, &client_rc);
    size_ = core::Size{static_cast<float>(client_rc.right - client_rc.left),
                       static_cast<float>(client_rc.bottom - client_rc.top)};
    min_width_ = desc.min_width;
    min_height_ = desc.min_height;

    gpu_device_ = device;
    if (gpu_device_) {
        if (!gpu_device_->init_swapchain(hwnd_, static_cast<uint32_t>(size_.width),
                                         static_cast<uint32_t>(size_.height))) {
            destroy();
            return false;
        }
    }

    return true;
}

void Win32Window::destroy() {
    if (hwnd_) {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    gpu_device_ = nullptr;
    listener_ = nullptr;
}

void Win32Window::show(bool visible) {
    if (hwnd_) {
        ShowWindow(hwnd_, visible ? SW_SHOW : SW_HIDE);
        if (visible) {
            UpdateWindow(hwnd_);
        }
    }
}

void Win32Window::set_ime_position(core::Point screen_pos) {
    if (!hwnd_)
        return;
    HIMC himc = ImmGetContext(hwnd_);
    if (!himc)
        return;

    COMPOSITIONFORM form{};
    form.dwStyle = CFS_POINT;
    form.ptCurrentPos.x = static_cast<LONG>(screen_pos.x * dpi_scale_);
    form.ptCurrentPos.y = static_cast<LONG>(screen_pos.y * dpi_scale_);
    ImmSetCompositionWindow(himc, &form);

    CANDIDATEFORM cform{};
    cform.dwIndex = 0;
    cform.dwStyle = CFS_CANDIDATEPOS;
    cform.ptCurrentPos.x = static_cast<LONG>(screen_pos.x * dpi_scale_);
    cform.ptCurrentPos.y = static_cast<LONG>(screen_pos.y * dpi_scale_);
    ImmSetCandidateWindow(himc, &cform);

    ImmReleaseContext(hwnd_, himc);
}

void Win32Window::update_dpi() {
    if (hwnd_) {
        UINT dpi = GetDpiForWindow(hwnd_);
        dpi_scale_ = static_cast<float>(dpi > 0 ? dpi : 96) / 96.0f;
    } else {
        dpi_scale_ = 1.0f;
    }
}

LRESULT CALLBACK Win32Window::window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    auto* window = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!window && msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
        window = reinterpret_cast<Win32Window*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    }

    if (window) {
        return window->handle_message(msg, wparam, lparam);
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT Win32Window::handle_message(UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd_, &ps);
        Win32App::instance().trigger_render();
        EndPaint(hwnd_, &ps);
        return 0;
    }

    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lparam);
        if (min_width_ > 0) {
            mmi->ptMinTrackSize.x = static_cast<LONG>(static_cast<float>(min_width_) * dpi_scale_);
        }
        if (min_height_ > 0) {
            mmi->ptMinTrackSize.y = static_cast<LONG>(static_cast<float>(min_height_) * dpi_scale_);
        }
        return 0;
    }

    case WM_EXITSIZEMOVE: {
        Win32App::instance().trigger_render();
        return 0;
    }

    case WM_SIZE: {
        uint32_t w = LOWORD(lparam);
        uint32_t h = HIWORD(lparam);
        size_ = core::Size{static_cast<float>(w), static_cast<float>(h)};

        if (gpu_device_ && w > 0 && h > 0) {
            gpu_device_->resize_swapchain(w, h);
        }
        if (listener_ && w > 0 && h > 0) {
            listener_->on_resize(size_);
        }
        if (w > 0 && h > 0) {
            Win32App::instance().trigger_render();
        }
        return 0;
    }

    case WM_DPICHANGED: {
        UINT dpi = HIWORD(wparam);
        dpi_scale_ = static_cast<float>(dpi) / 96.0f;
        auto* prc = reinterpret_cast<RECT*>(lparam);
        SetWindowPos(hwnd_, nullptr, prc->left, prc->top, prc->right - prc->left,
                     prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
        if (listener_) {
            core::Rect sug{static_cast<float>(prc->left), static_cast<float>(prc->top),
                           static_cast<float>(prc->right - prc->left),
                           static_cast<float>(prc->bottom - prc->top)};
            listener_->on_dpi_changed(dpi_scale_, sug);
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (listener_) {
            PointerEvent pe{.action = PointerAction::Move,
                            .button = PointerButton::None,
                            .position = core::Point{static_cast<float>(GET_X_LPARAM(lparam)),
                                                    static_cast<float>(GET_Y_LPARAM(lparam))},
                            .modifiers = get_modifiers(),
                            .pressure = 1.0f,
                            .type = PointerType::Mouse};
            listener_->on_pointer(pe);
        }
        return 0;
    }

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN: {
        SetCapture(hwnd_);
        if (listener_) {
            PointerButton btn = (msg == WM_LBUTTONDOWN)   ? PointerButton::Left
                                : (msg == WM_RBUTTONDOWN) ? PointerButton::Right
                                                          : PointerButton::Middle;
            PointerEvent pe{.action = PointerAction::Down,
                            .button = btn,
                            .position = core::Point{static_cast<float>(GET_X_LPARAM(lparam)),
                                                    static_cast<float>(GET_Y_LPARAM(lparam))},
                            .modifiers = get_modifiers(),
                            .pressure = 1.0f,
                            .type = PointerType::Mouse};
            listener_->on_pointer(pe);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
        ReleaseCapture();
        if (listener_) {
            PointerButton btn = (msg == WM_LBUTTONUP)   ? PointerButton::Left
                                : (msg == WM_RBUTTONUP) ? PointerButton::Right
                                                        : PointerButton::Middle;
            PointerEvent pe{.action = PointerAction::Up,
                            .button = btn,
                            .position = core::Point{static_cast<float>(GET_X_LPARAM(lparam)),
                                                    static_cast<float>(GET_Y_LPARAM(lparam))},
                            .modifiers = get_modifiers(),
                            .pressure = 1.0f,
                            .type = PointerType::Mouse};
            listener_->on_pointer(pe);
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        short delta = GET_WHEEL_DELTA_WPARAM(wparam);
        POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(hwnd_, &pt);
        float scroll_delta = (static_cast<float>(delta) / static_cast<float>(WHEEL_DELTA)) * 48.0f;
        if (listener_) {
            listener_->on_scroll(0.0f, scroll_delta,
                                 core::Point{static_cast<float>(pt.x), static_cast<float>(pt.y)});
        }
        return 0;
    }

    case WM_MOUSEHWHEEL: {
        short delta = GET_WHEEL_DELTA_WPARAM(wparam);
        POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(hwnd_, &pt);
        float scroll_delta = (static_cast<float>(delta) / static_cast<float>(WHEEL_DELTA)) * 48.0f;
        if (listener_) {
            listener_->on_scroll(scroll_delta, 0.0f,
                                 core::Point{static_cast<float>(pt.x), static_cast<float>(pt.y)});
        }
        return 0;
    }

    case WM_KEYDOWN:
    case WM_KEYUP: {
        if (listener_) {
            KeyEvent ke{.key_code = static_cast<uint32_t>(wparam),
                        .character = 0,
                        .is_down = (msg == WM_KEYDOWN),
                        .modifiers = get_modifiers()};
            listener_->on_key(ke);
        }
        return 0;
    }

    case WM_CHAR: {
        if (listener_) {
            char32_t ch = static_cast<char32_t>(wparam);
            if (ch >= 32 || ch == U'\t' || ch == U'\r' || ch == U'\n') {
                KeyEvent ke{
                    .key_code = 0, .character = ch, .is_down = true, .modifiers = get_modifiers()};
                listener_->on_key(ke);
            }
        }
        return 0;
    }

    case WM_IME_STARTCOMPOSITION: {
        return DefWindowProcW(hwnd_, msg, wparam, lparam);
    }

    case WM_IME_COMPOSITION: {
        HIMC himc = ImmGetContext(hwnd_);
        if (himc) {
            if (lparam & GCS_RESULTSTR) {
                LONG bytes = ImmGetCompositionStringW(himc, GCS_RESULTSTR, nullptr, 0);
                if (bytes > 0) {
                    std::wstring wbuf(static_cast<size_t>(bytes) / sizeof(wchar_t), L'\0');
                    ImmGetCompositionStringW(himc, GCS_RESULTSTR, wbuf.data(), bytes);
                    int u8_len =
                        WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), static_cast<int>(wbuf.size()),
                                            nullptr, 0, nullptr, nullptr);
                    if (u8_len > 0) {
                        std::string u8(static_cast<size_t>(u8_len), '\0');
                        WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), static_cast<int>(wbuf.size()),
                                            u8.data(), u8_len, nullptr, nullptr);
                        if (listener_) {
                            listener_->on_ime_commit(u8);
                        }
                    }
                }
            }
            if (lparam & GCS_COMPSTR) {
                LONG bytes = ImmGetCompositionStringW(himc, GCS_COMPSTR, nullptr, 0);
                std::string u8;
                if (bytes > 0) {
                    std::wstring wbuf(static_cast<size_t>(bytes) / sizeof(wchar_t), L'\0');
                    ImmGetCompositionStringW(himc, GCS_COMPSTR, wbuf.data(), bytes);
                    int u8_len =
                        WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), static_cast<int>(wbuf.size()),
                                            nullptr, 0, nullptr, nullptr);
                    if (u8_len > 0) {
                        u8.resize(static_cast<size_t>(u8_len));
                        WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), static_cast<int>(wbuf.size()),
                                            u8.data(), u8_len, nullptr, nullptr);
                    }
                }
                LONG cur_pos = ImmGetCompositionStringW(himc, GCS_CURSORPOS, nullptr, 0);
                if (listener_) {
                    listener_->on_ime_composition(u8, static_cast<int>(cur_pos));
                }
            }
            ImmReleaseContext(hwnd_, himc);
            return 0;
        }
        break;
    }

    case WM_IME_ENDCOMPOSITION: {
        if (listener_) {
            listener_->on_ime_composition({}, 0);
        }
        return DefWindowProcW(hwnd_, msg, wparam, lparam);
    }

    case WM_CLOSE: {
        if (listener_) {
            listener_->on_close();
        }
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return 0;
    }

    case WM_DESTROY: {
        hwnd_ = nullptr;
        PostQuitMessage(0);
        return 0;
    }

    default:
        break;
    }

    return DefWindowProcW(hwnd_, msg, wparam, lparam);
}

} // namespace north::platform::win32
