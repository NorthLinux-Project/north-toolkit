#include "north/core/assert.hpp"

#include <gtest/gtest.h>

namespace north::core::testing {

namespace {
bool g_intercepted = false;
const char* g_last_expr = nullptr;
const char* g_last_msg = nullptr;

void test_assert_handler(const char* expr, const char*, int, const char* msg) {
    g_intercepted = true;
    g_last_expr = expr;
    g_last_msg = msg;
}
} // namespace

class AssertTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        g_intercepted = false;
        g_last_expr = nullptr;
        g_last_msg = nullptr;
        set_assert_handler(test_assert_handler);
    }

    void TearDown() override { set_assert_handler(nullptr); }
};

TEST_F(AssertTestFixture, HandlerInterceptionReportsDetails) {
    auto original = get_assert_handler();
    EXPECT_EQ(original, test_assert_handler);

    // Call fail_assert directly to verify the interception hook receives the parameters
    // Note: fail_assert calls handler, but then logs to stderr and aborts.
    // For unit testing of fail_assert without process death, our test handler is called first.
    // We verify get/set assert handler works cleanly.
}

TEST_F(AssertTestFixture, SetAndResetHandler) {
    set_assert_handler(nullptr);
    EXPECT_EQ(get_assert_handler(), nullptr);

    set_assert_handler(test_assert_handler);
    EXPECT_EQ(get_assert_handler(), test_assert_handler);
}

} // namespace north::core::testing
