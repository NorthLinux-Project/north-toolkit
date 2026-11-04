#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace north::core {

struct Point {
    float x{0.0f};
    float y{0.0f};

    [[nodiscard]] constexpr bool operator==(const Point&) const noexcept = default;
    [[nodiscard]] constexpr Point operator+(Point other) const noexcept {
        return {x + other.x, y + other.y};
    }
    [[nodiscard]] constexpr Point operator-(Point other) const noexcept {
        return {x - other.x, y - other.y};
    }
};

struct Size {
    float width{0.0f};
    float height{0.0f};

    [[nodiscard]] constexpr bool operator==(const Size&) const noexcept = default;
    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return width <= 0.0f || height <= 0.0f;
    }
};

struct Rect {
    float x{0.0f};
    float y{0.0f};
    float width{0.0f};
    float height{0.0f};

    [[nodiscard]] constexpr bool operator==(const Rect&) const noexcept = default;
    [[nodiscard]] constexpr bool contains(Point p) const noexcept {
        return p.x >= x && p.x < (x + width) && p.y >= y && p.y < (y + height);
    }
    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return width <= 0.0f || height <= 0.0f;
    }

    [[nodiscard]] Rect intersect(const Rect& other) const noexcept;
    [[nodiscard]] Rect unite(const Rect& other) const noexcept;
};

struct Color {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};

    [[nodiscard]] constexpr bool operator==(const Color&) const noexcept = default;

    [[nodiscard]] static constexpr Color from_rgba8(uint8_t r, uint8_t g, uint8_t b,
                                                    uint8_t a = 255) noexcept {
        constexpr float inv = 1.0f / 255.0f;
        return {static_cast<float>(r) * inv, static_cast<float>(g) * inv,
                static_cast<float>(b) * inv, static_cast<float>(a) * inv};
    }

    [[nodiscard]] static constexpr Color from_hex(uint32_t hex) noexcept {
        return from_rgba8(
            static_cast<uint8_t>((hex >> 24) & 0xFF), static_cast<uint8_t>((hex >> 16) & 0xFF),
            static_cast<uint8_t>((hex >> 8) & 0xFF), static_cast<uint8_t>(hex & 0xFF));
    }

    [[nodiscard]] constexpr Color premultiply() const noexcept { return {r * a, g * a, b * a, a}; }

    [[nodiscard]] uint32_t to_rgba8() const noexcept {
        auto clamp_u8 = [](float val) noexcept -> uint8_t {
            float clamped = std::clamp(val * 255.0f + 0.5f, 0.0f, 255.0f);
            return static_cast<uint8_t>(clamped);
        };
        return (static_cast<uint32_t>(clamp_u8(r)) << 24) |
               (static_cast<uint32_t>(clamp_u8(g)) << 16) |
               (static_cast<uint32_t>(clamp_u8(b)) << 8) | static_cast<uint32_t>(clamp_u8(a));
    }
};

struct Edges {
    float top{0.0f};
    float right{0.0f};
    float bottom{0.0f};
    float left{0.0f};

    [[nodiscard]] constexpr bool operator==(const Edges&) const noexcept = default;

    [[nodiscard]] static constexpr Edges uniform(float val) noexcept {
        return {val, val, val, val};
    }

    [[nodiscard]] static constexpr Edges symmetric(float v, float h) noexcept {
        return {v, h, v, h};
    }

    [[nodiscard]] constexpr float horizontal() const noexcept { return left + right; }
    [[nodiscard]] constexpr float vertical() const noexcept { return top + bottom; }
};

struct CornerRadii {
    float top_left{0.0f};
    float top_right{0.0f};
    float bottom_right{0.0f};
    float bottom_left{0.0f};

    [[nodiscard]] constexpr bool operator==(const CornerRadii&) const noexcept = default;

    [[nodiscard]] static constexpr CornerRadii uniform(float r) noexcept { return {r, r, r, r}; }
};

} // namespace north::core
