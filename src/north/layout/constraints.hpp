#pragma once

#include "north/core/types.hpp"

#include <algorithm>
#include <limits>

namespace north::layout {

struct BoxConstraints {
    float min_width{0.0f};
    float max_width{std::numeric_limits<float>::infinity()};
    float min_height{0.0f};
    float max_height{std::numeric_limits<float>::infinity()};

    [[nodiscard]] static constexpr BoxConstraints tight(core::Size size) noexcept {
        return {size.width, size.width, size.height, size.height};
    }

    [[nodiscard]] static constexpr BoxConstraints loose(core::Size size) noexcept {
        return {0.0f, size.width, 0.0f, size.height};
    }

    [[nodiscard]] constexpr BoxConstraints loosen() const noexcept {
        return {0.0f, max_width, 0.0f, max_height};
    }

    [[nodiscard]] constexpr BoxConstraints tighten(core::Size size) const noexcept {
        return {std::clamp(size.width, min_width, max_width),
                std::clamp(size.width, min_width, max_width),
                std::clamp(size.height, min_height, max_height),
                std::clamp(size.height, min_height, max_height)};
    }

    [[nodiscard]] constexpr core::Size constrain(core::Size size) const noexcept {
        return {std::clamp(size.width, min_width, max_width),
                std::clamp(size.height, min_height, max_height)};
    }

    [[nodiscard]] constexpr bool is_tight() const noexcept {
        return min_width >= max_width && min_height >= max_height;
    }

    [[nodiscard]] constexpr bool has_bounded_width() const noexcept {
        return max_width < std::numeric_limits<float>::infinity();
    }

    [[nodiscard]] constexpr bool has_bounded_height() const noexcept {
        return max_height < std::numeric_limits<float>::infinity();
    }

    [[nodiscard]] constexpr BoxConstraints deflate(core::Edges edges) const noexcept {
        float h = edges.horizontal();
        float v = edges.vertical();
        return {(std::max)(0.0f, min_width - h), (std::max)(0.0f, max_width - h),
                (std::max)(0.0f, min_height - v), (std::max)(0.0f, max_height - v)};
    }

    [[nodiscard]] constexpr bool operator==(const BoxConstraints&) const noexcept = default;
};

} // namespace north::layout
