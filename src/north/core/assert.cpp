#include "north/core/assert.hpp"

#include <cstdio>
#include <cstdlib>

namespace north::core {

namespace {
AssertHandler g_assert_handler = nullptr;
} // namespace

void set_assert_handler(AssertHandler handler) noexcept {
    g_assert_handler = handler;
}

AssertHandler get_assert_handler() noexcept {
    return g_assert_handler;
}

[[noreturn]] void fail_assert(const char* expr, const char* file, int line,
                              const char* msg) noexcept {
    if (g_assert_handler) {
        g_assert_handler(expr, file, line, msg);
    }

    std::fprintf(stderr, "[NORTH ASSERTION FAILED] %s\n  Expression: %s\n  Location:   %s:%d\n",
                 msg ? msg : "", expr ? expr : "", file ? file : "", line);
    std::fflush(stderr);

    NORTH_DEBUG_BREAK();
    std::abort();
}

} // namespace north::core
