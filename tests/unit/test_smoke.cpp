#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

namespace {

// Constexpr string_view integer parser
constexpr std::optional<uint32_t> parse_uint(std::string_view text) noexcept {
    if (text.empty()) {
        return std::nullopt;
    }
    uint32_t value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        auto digit = static_cast<uint32_t>(c - '0');
        if (value > ((0xFFFFFFFFu - digit) / 10u)) {
            return std::nullopt;
        }
        value = (value * 10u) + digit;
    }
    return value;
}

// Zero-allocation monadic helpers for C++20 without requiring C++23 STL extensions
template <typename T, typename F>
constexpr auto optional_transform(const std::optional<T>& opt, F&& f)
    -> std::optional<std::remove_cvref_t<std::invoke_result_t<F, const T&>>> {
    if (opt.has_value()) {
        return std::invoke(std::forward<F>(f), *opt);
    }
    return std::nullopt;
}

template <typename T, typename F>
constexpr auto optional_and_then(const std::optional<T>& opt, F&& f)
    -> std::invoke_result_t<F, const T&> {
    if (opt.has_value()) {
        return std::invoke(std::forward<F>(f), *opt);
    }
    return std::nullopt;
}

// Forward alignment helper invariant
constexpr uintptr_t align_forward(uintptr_t ptr, size_t alignment) noexcept {
    return (ptr + (alignment - 1u)) & ~(alignment - 1u);
}

} // namespace

TEST(ToolchainEnvironment, PreprocessorFlagsAndDialect) {
    // Verify C++20 language standard conformance
#ifdef _MSVC_LANG
    EXPECT_GE(_MSVC_LANG, 202002L);
#else
    EXPECT_GE(__cplusplus, 202002L);
#endif

    // Verify exception handling is disabled in runtime and STL
#ifdef _MSC_VER
#ifdef _HAS_EXCEPTIONS
    EXPECT_EQ(_HAS_EXCEPTIONS, 0);
#else
    FAIL() << "_HAS_EXCEPTIONS definition missing under MSVC toolchain";
#endif
#ifdef _CPPUNWIND
    FAIL() << "_CPPUNWIND is defined: C++ exceptions must be disabled (/EHs-c-)";
#endif
#elif defined(__clang__) || defined(__GNUC__)
#ifdef __EXCEPTIONS
    FAIL() << "__EXCEPTIONS is defined: C++ exceptions must be disabled (-fno-exceptions)";
#endif
#endif

    // Verify RTTI is disabled
#ifdef _CPPRTTI
    FAIL() << "_CPPRTTI is defined: RTTI must be disabled (/GR-)";
#elif defined(__GXX_RTTI)
    FAIL() << "__GXX_RTTI is defined: RTTI must be disabled (-fno-rtti)";
#endif
}

TEST(SpanOperations, DynamicSpanSlicing) {
    std::array<int, 8> buffer = {10, 20, 30, 40, 50, 60, 70, 80};
    std::span<int> full_span(buffer);

    EXPECT_EQ(full_span.size(), buffer.size());
    EXPECT_EQ(full_span.size_bytes(), buffer.size() * sizeof(int));
    EXPECT_EQ(full_span.data(), buffer.data());

    std::span<int> slice = full_span.subspan(2, 4);
    EXPECT_EQ(slice.size(), 4u);
    EXPECT_EQ(slice.data(), buffer.data() + 2);
    EXPECT_EQ(slice.front(), 30);
    EXPECT_EQ(slice.back(), 60);

    std::span<int> tail = full_span.subspan(5);
    EXPECT_EQ(tail.size(), 3u);
    EXPECT_EQ(tail.front(), 60);
    EXPECT_EQ(tail.back(), 80);
}

TEST(SpanOperations, ElementAccessAndMutation) {
    std::array<int, 5> buffer = {1, 2, 3, 4, 5};
    std::span<int> span_view(buffer);

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    EXPECT_EQ(span_view[0], 1);
    EXPECT_EQ(span_view[4], 5);

    span_view[2] = 42;
    EXPECT_EQ(buffer[2], 42);
    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

    std::span<int> empty_span = span_view.subspan(buffer.size(), 0);
    EXPECT_TRUE(empty_span.empty());
    EXPECT_EQ(empty_span.size(), 0u);
}

TEST(StringViewOperations, PrefixAndSuffixMatching) {
    constexpr std::string_view identifier = "north::toolkit::v1";

    EXPECT_TRUE(identifier.starts_with("north::"));
    EXPECT_TRUE(identifier.ends_with("::v1"));
    EXPECT_FALSE(identifier.starts_with("south::"));
    EXPECT_FALSE(identifier.ends_with("::v2"));
}

TEST(StringViewOperations, SubstringAndTrimming) {
    std::string_view text = "north::toolkit::v1";

    std::string_view core = text.substr(7, 7);
    EXPECT_EQ(core, "toolkit");

    text.remove_prefix(7);
    EXPECT_TRUE(text.starts_with("toolkit"));

    text.remove_suffix(4);
    EXPECT_EQ(text, "toolkit");
}

TEST(StringViewOperations, ConstexprIntegerParsing) {
    constexpr auto compile_val = parse_uint("1024");
    static_assert(compile_val.has_value());
    static_assert(*compile_val == 1024u);

    constexpr auto compile_invalid = parse_uint("10a24");
    static_assert(!compile_invalid.has_value());

    auto runtime_val = parse_uint("65535");
    ASSERT_TRUE(runtime_val.has_value());
    EXPECT_EQ(*runtime_val, 65535u);

    auto runtime_empty = parse_uint("");
    EXPECT_FALSE(runtime_empty.has_value());
}

TEST(OptionalOperations, ValueExtractionAndLifecycle) {
    std::optional<int> empty_opt;
    EXPECT_FALSE(empty_opt.has_value());
    EXPECT_EQ(empty_opt.value_or(100), 100);

    std::optional<int> populated_opt(42);
    ASSERT_TRUE(populated_opt.has_value());
    EXPECT_EQ(*populated_opt, 42);
    EXPECT_EQ(populated_opt.value_or(100), 42);

    populated_opt.reset();
    EXPECT_FALSE(populated_opt.has_value());

    populated_opt.emplace(99);
    ASSERT_TRUE(populated_opt.has_value());
    EXPECT_EQ(*populated_opt, 99);
}

TEST(OptionalOperations, MonadicTransformationPipeline) {
    std::optional<int> initial(42);

    auto doubled = optional_transform(initial, [](int x) noexcept { return x * 2; });
    ASSERT_TRUE(doubled.has_value());
    EXPECT_EQ(*doubled, 84);

    auto checked_half = optional_and_then(doubled, [](int x) noexcept -> std::optional<int> {
        if ((x % 2) == 0) {
            return x / 2;
        }
        return std::nullopt;
    });
    ASSERT_TRUE(checked_half.has_value());
    EXPECT_EQ(*checked_half, 42);

    std::optional<int> empty_opt;
    auto empty_transformed = optional_transform(empty_opt, [](int x) noexcept { return x * 2; });
    EXPECT_FALSE(empty_transformed.has_value());
}

TEST(MemoryAlignment, AlignmentInvariants) {
    constexpr size_t max_align = alignof(std::max_align_t);
    EXPECT_GE(max_align, sizeof(void*));
    EXPECT_EQ((max_align & (max_align - 1u)), 0u);

    EXPECT_EQ(alignof(uint8_t), 1u);
    EXPECT_EQ(alignof(uint16_t), 2u);
    EXPECT_EQ(alignof(uint32_t), 4u);
    EXPECT_EQ(alignof(uint64_t), 8u);
    EXPECT_EQ(alignof(void*), sizeof(void*));
}

TEST(MemoryAlignment, ForwardAlignmentCalculations) {
    EXPECT_EQ(align_forward(0, 16), 0u);
    EXPECT_EQ(align_forward(1, 16), 16u);
    EXPECT_EQ(align_forward(15, 16), 16u);
    EXPECT_EQ(align_forward(16, 16), 16u);
    EXPECT_EQ(align_forward(17, 16), 32u);

    constexpr uintptr_t test_addr = 0x1001;
    EXPECT_EQ(align_forward(test_addr, 64), 0x1040u);
}
