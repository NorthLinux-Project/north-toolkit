#pragma once

#include <compare>
#include <cstdint>
#include <functional>

namespace north::core {

template <typename T>
struct Handle {
    uint32_t index{0};
    uint32_t generation{0};

    [[nodiscard]] constexpr bool is_valid() const noexcept { return generation != 0; }

    [[nodiscard]] constexpr bool operator==(const Handle&) const noexcept = default;
    [[nodiscard]] constexpr auto operator<=>(const Handle&) const noexcept = default;
};

} // namespace north::core

template <typename T>
struct std::hash<north::core::Handle<T>> {
    [[nodiscard]] size_t operator()(const north::core::Handle<T>& h) const noexcept {
        return (static_cast<size_t>(h.index) << 32) | static_cast<size_t>(h.generation);
    }
};
