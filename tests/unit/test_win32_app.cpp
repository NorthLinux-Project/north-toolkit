#include "north/platform/win32/win32_app.hpp"

#include <gtest/gtest.h>

namespace north::platform::win32::testing {

namespace {

int g_idle_calls = 0;

void test_idle_callback() {
    ++g_idle_calls;
}

} // namespace

TEST(Win32AppTest, SingletonLifecycle) {
    Win32App& app1 = Win32App::instance();
    Win32App& app2 = Win32App::instance();
    EXPECT_EQ(&app1, &app2);
}

TEST(Win32AppTest, RegisterAndUnregisterWindows) {
    Win32App& app = Win32App::instance();
    size_t initial_count = app.window_count();

    Win32Window window1;
    Win32Window window2;

    WindowDesc desc{.width = 300, .height = 200};
    ASSERT_TRUE(window1.create(desc));
    ASSERT_TRUE(window2.create(desc));

    app.register_window(&window1);
    EXPECT_EQ(app.window_count(), initial_count + 1);

    // Re-registration does not duplicate
    app.register_window(&window1);
    EXPECT_EQ(app.window_count(), initial_count + 1);

    app.register_window(&window2);
    EXPECT_EQ(app.window_count(), initial_count + 2);

    app.unregister_window(&window1);
    EXPECT_EQ(app.window_count(), initial_count + 1);

    app.unregister_window(&window2);
    EXPECT_EQ(app.window_count(), initial_count);

    window1.destroy();
    window2.destroy();
}

TEST(Win32AppTest, PumpEventsNonBlocking) {
    Win32App& app = Win32App::instance();

    // Pumping when empty should return immediately without blocking
    app.pump_events();

    // Post a message and pump
    PostMessageW(nullptr, WM_USER + 1, 0, 0);
    app.pump_events();
}

TEST(Win32AppTest, IdleCallbackRegistration) {
    Win32App& app = Win32App::instance();
    g_idle_calls = 0;

    app.set_idle_callback(&test_idle_callback);
    // Setting callback succeeds
    app.set_idle_callback(nullptr);
}

} // namespace north::platform::win32::testing
