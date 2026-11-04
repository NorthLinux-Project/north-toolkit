#pragma once

#include "north/core/types.hpp"

#include <cstdint>

namespace north::gpu {

using TextureId = uint32_t;
using BufferId = uint32_t;
using PipelineId = uint32_t;

constexpr TextureId kInvalidTexture = 0;
constexpr BufferId kInvalidBuffer = 0;
constexpr PipelineId kInvalidPipeline = 0;

enum class TextureFormat : uint8_t { RGBA8, R8, BGRA8 };

enum class BufferUsage : uint8_t { Vertex, Index, Constant };

enum class BlendMode : uint8_t { Opaque, PremultipliedAlpha, Alpha };

enum class VertexLayout : uint8_t { Quad, Glyph };

struct TextureDesc {
    uint32_t width{0};
    uint32_t height{0};
    TextureFormat format{TextureFormat::RGBA8};
    bool render_target{false};
};

struct BufferDesc {
    size_t size{0};
    BufferUsage usage{BufferUsage::Vertex};
    bool dynamic{true};
};

struct alignas(16) QuadInstance {
    core::Rect rect;             // 16 bytes: x, y, width, height
    core::CornerRadii radii;     // 16 bytes: tl, tr, br, bl
    core::Color fill_color;      // 16 bytes: r, g, b, a
    core::Color border_color;    // 16 bytes: r, g, b, a
    core::Edges border_width;    // 16 bytes: top, right, bottom, left
    core::Rect clip_rect;        // 16 bytes: x, y, width, height
    core::Color shadow_color;    // 16 bytes: r, g, b, a
    float shadow_radius{0.0f};   // 4 bytes
    float shadow_spread{0.0f};   // 4 bytes
    float shadow_offset_x{0.0f}; // 4 bytes
    float shadow_offset_y{0.0f}; // 4 bytes
};

struct alignas(16) GlyphInstance {
    core::Rect dst_rect;
    core::Rect uv_rect;
    core::Color color;
    core::Rect clip_rect;
};

struct alignas(16) ViewConstants {
    float viewport_width{0.0f};
    float viewport_height{0.0f};
    float pad[2]{0.0f, 0.0f};
};

} // namespace north::gpu
