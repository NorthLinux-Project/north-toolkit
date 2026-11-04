#pragma once

#include <cstdlib>

namespace north::core {

using AssertHandler = void (*)(const char* expr, const char* file, int line, const char* msg);

void set_assert_handler(AssertHandler handler) noexcept;
[[nodiscard]] AssertHandler get_assert_handler() noexcept;

[[noreturn]] void fail_assert(const char* expr, const char* file, int line,
                              const char* msg) noexcept;

} // namespace north::core

#if defined(_MSC_VER) || defined(__clang__)
#define NORTH_DEBUG_BREAK() __debugbreak()
#else
#define NORTH_DEBUG_BREAK() __builtin_trap()
#endif

#if !defined(NDEBUG)
#define NORTH_ASSERT(expr, msg)                                                                    \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            ::north::core::fail_assert(#expr, __FILE__, __LINE__, msg);                            \
        }                                                                                          \
    } while (false)
#else
#define NORTH_ASSERT(expr, msg)                                                                    \
    do {                                                                                           \
        (void)sizeof(expr);                                                                        \
    } while (false)
#endif

#if defined(_MSC_VER)
#define NORTH_UNREACHABLE() __assume(0)
#elif defined(__clang__) || defined(__GNUC__)
#define NORTH_UNREACHABLE() __builtin_unreachable()
#else
#define NORTH_UNREACHABLE() ::std::abort()
#endif
