#include "north/platform/win32/win32_window.hpp"

#include <gtest/gtest.h>

#include <vector>

#include <windowsx.h>

namespace north::platform::win32::testing {

class TestWindowListener final : public IWindowListener {
public:
    void on_resize(core::Size new_size) override {
        ++resize_count;
        last_size = new_size;
    }

    void on_dpi_changed(float dpi_scale, core::Rect suggested_rect) override {
        ++dpi_count;
        last_dpi = dpi_scale;
        last_suggested_rect = suggested_rect;
    }

    void on_pointer(const PointerEvent& event) override { pointer_events.push_back(event); }

    void on_key(const KeyEvent& event) override { key_events.push_back(event); }

    void on_close() override { ++close_count; }

    size_t resize_count{0};
    core::Size last_size{0.0f, 0.0f};

    size_t dpi_count{0};
    float last_dpi{1.0f};
    core::Rect last_suggested_rect{};

    std::vector<PointerEvent> pointer_events;
    std::vector<KeyEvent> key_events;
    size_t close_count{0};
};

TEST(Win32WindowTest, CreationAndDestruction) {
    Win32Window window;
    EXPECT_FALSE(window.is_open());
    EXPECT_EQ(window.hwnd(), nullptr);

    WindowDesc desc{.title = "Test North Window", .width = 640, .height = 480, .resizable = true};

    bool created = window.create(desc, nullptr);
    ASSERT_TRUE(created);
    EXPECT_TRUE(window.is_open());
    EXPECT_NE(window.hwnd(), nullptr);
    EXPECT_GT(window.dpi_scale(), 0.0f);
    EXPECT_GT(window.size().width, 0.0f);
    EXPECT_GT(window.size().height, 0.0f);

    window.destroy();
    EXPECT_FALSE(window.is_open());
    EXPECT_EQ(window.hwnd(), nullptr);
}

TEST(Win32WindowTest, ResizeEventRouting) {
    Win32Window window;
    WindowDesc desc{.width = 400, .height = 300};
    ASSERT_TRUE(window.create(desc));

    TestWindowListener listener;
    window.set_listener(&listener);

    SendMessageW(window.hwnd(), WM_SIZE, 0, MAKELPARAM(800, 600));

    EXPECT_EQ(listener.resize_count, 1u);
    EXPECT_FLOAT_EQ(listener.last_size.width, 800.0f);
    EXPECT_FLOAT_EQ(listener.last_size.height, 600.0f);
    EXPECT_FLOAT_EQ(window.size().width, 800.0f);
    EXPECT_FLOAT_EQ(window.size().height, 600.0f);

    window.destroy();
}

TEST(Win32WindowTest, PointerEventRouting) {
    Win32Window window;
    WindowDesc desc{.width = 400, .height = 300};
    ASSERT_TRUE(window.create(desc));

    TestWindowListener listener;
    window.set_listener(&listener);

    SendMessageW(window.hwnd(), WM_MOUSEMOVE, 0, MAKELPARAM(120, 240));
    SendMessageW(window.hwnd(), WM_LBUTTONDOWN, 0, MAKELPARAM(120, 240));
    SendMessageW(window.hwnd(), WM_LBUTTONUP, 0, MAKELPARAM(120, 240));

    ASSERT_EQ(listener.pointer_events.size(), 3u);

    // Move
    EXPECT_EQ(listener.pointer_events[0].action, PointerAction::Move);
    EXPECT_EQ(listener.pointer_events[0].button, PointerButton::None);
    EXPECT_FLOAT_EQ(listener.pointer_events[0].position.x, 120.0f);
    EXPECT_FLOAT_EQ(listener.pointer_events[0].position.y, 240.0f);

    // Down
    EXPECT_EQ(listener.pointer_events[1].action, PointerAction::Down);
    EXPECT_EQ(listener.pointer_events[1].button, PointerButton::Left);
    EXPECT_FLOAT_EQ(listener.pointer_events[1].position.x, 120.0f);
    EXPECT_FLOAT_EQ(listener.pointer_events[1].position.y, 240.0f);

    // Up
    EXPECT_EQ(listener.pointer_events[2].action, PointerAction::Up);
    EXPECT_EQ(listener.pointer_events[2].button, PointerButton::Left);
    EXPECT_FLOAT_EQ(listener.pointer_events[2].position.x, 120.0f);
    EXPECT_FLOAT_EQ(listener.pointer_events[2].position.y, 240.0f);

    window.destroy();
}

TEST(Win32WindowTest, KeyEventRouting) {
    Win32Window window;
    WindowDesc desc{.width = 400, .height = 300};
    ASSERT_TRUE(window.create(desc));

    TestWindowListener listener;
    window.set_listener(&listener);

    SendMessageW(window.hwnd(), WM_KEYDOWN, VK_ESCAPE, 0);
    SendMessageW(window.hwnd(), WM_KEYUP, VK_ESCAPE, 0);
    SendMessageW(window.hwnd(), WM_CHAR, L'Z', 0);

    ASSERT_EQ(listener.key_events.size(), 3u);

    EXPECT_EQ(listener.key_events[0].key_code, static_cast<uint32_t>(VK_ESCAPE));
    EXPECT_TRUE(listener.key_events[0].is_down);

    EXPECT_EQ(listener.key_events[1].key_code, static_cast<uint32_t>(VK_ESCAPE));
    EXPECT_FALSE(listener.key_events[1].is_down);

    EXPECT_EQ(listener.key_events[2].character, static_cast<char32_t>(L'Z'));
    EXPECT_TRUE(listener.key_events[2].is_down);

    window.destroy();
}

TEST(Win32WindowTest, CloseEventRouting) {
    Win32Window window;
    WindowDesc desc{.width = 400, .height = 300};
    ASSERT_TRUE(window.create(desc));

    TestWindowListener listener;
    window.set_listener(&listener);

    SendMessageW(window.hwnd(), WM_CLOSE, 0, 0);

    EXPECT_EQ(listener.close_count, 1u);
    EXPECT_FALSE(window.is_open());
    EXPECT_EQ(window.hwnd(), nullptr);
}

TEST(Win32WindowTest, MinMaxInfoTrackSize) {
    Win32Window window;
    WindowDesc desc{.width = 500, .height = 400, .min_width = 320, .min_height = 240};
    ASSERT_TRUE(window.create(desc));

    MINMAXINFO mmi{};
    SendMessageW(window.hwnd(), WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&mmi));

    EXPECT_GE(mmi.ptMinTrackSize.x, 320);
    EXPECT_GE(mmi.ptMinTrackSize.y, 240);

    window.destroy();
}

TEST(Win32WindowTest, EraseBackgroundSuppressed) {
    Win32Window window;
    WindowDesc desc{.width = 400, .height = 300};
    ASSERT_TRUE(window.create(desc));

    LRESULT res = SendMessageW(window.hwnd(), WM_ERASEBKGND, 0, 0);
    EXPECT_EQ(res, 1);

    window.destroy();
}

} // namespace north::platform::win32::testing
