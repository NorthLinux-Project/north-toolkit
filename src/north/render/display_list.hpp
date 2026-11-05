#pragma once

#include "north/core/arena.hpp"
#include "north/core/types.hpp"
#include "north/gpu/types.hpp"
#include "north/render/path.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace north::text {
class GlyphAtlas;
}

namespace north::render {

enum class CommandType : uint8_t { Quad, Shadow, GlyphRun, PathMesh, Image };

struct DrawQuadCmd {
    core::Rect bounds;
    core::CornerRadii radii;
    core::Color fill_color;
    core::Color border_color;
    core::Edges border_width;
};

struct DrawShadowCmd {
    core::Rect bounds;
    core::CornerRadii radii;
    core::Color color;
    float blur_radius{0.0f};
    float spread{0.0f};
    core::Point offset{0.0f, 0.0f};
};

struct DrawGlyphRunCmd {
    gpu::TextureId atlas_texture{gpu::kInvalidTexture};
    std::span<const gpu::GlyphInstance> glyphs{};
};

struct DrawPathMeshCmd {
    std::span<const gpu::PathVertex> vertices;
    std::span<const uint16_t> indices;
};

struct DrawImageCmd {
    gpu::TextureId texture{gpu::kInvalidTexture};
    core::Rect bounds{};
    core::Rect uv_rect{0.0f, 0.0f, 1.0f, 1.0f};
    core::CornerRadii radii{};
    core::Color tint{1.0f, 1.0f, 1.0f, 1.0f};
    core::Color border_color{0.0f, 0.0f, 0.0f, 0.0f};
    core::Edges border_width{};
};

struct DrawCommand {
    CommandType type{CommandType::Quad};
    core::Rect clip_rect{};
    union {
        DrawQuadCmd quad;
        DrawShadowCmd shadow;
        DrawGlyphRunCmd glyph_run;
        DrawPathMeshCmd path_mesh;
        DrawImageCmd image;
    };

    DrawCommand() noexcept : type(CommandType::Quad), clip_rect{}, quad{} {}
    ~DrawCommand() noexcept = default;
};

class DisplayList {
public:
    explicit DisplayList(core::Arena& arena,
                         core::Rect initial_clip = core::Rect{0.0f, 0.0f, 1e7f, 1e7f}) noexcept;

    void push_quad(core::Rect bounds, core::CornerRadii radii, core::Color fill,
                   core::Color border = {}, core::Edges border_width = {}) noexcept;

    void push_shadow(core::Rect bounds, core::CornerRadii radii, core::Color color,
                     float blur_radius, float spread = 0.0f,
                     core::Point offset = {0.0f, 0.0f}) noexcept;

    void push_image(gpu::TextureId texture, core::Rect bounds,
                    core::CornerRadii radii = {},
                    core::Rect uv_rect = {0.0f, 0.0f, 1.0f, 1.0f},
                    core::Color tint = {1.0f, 1.0f, 1.0f, 1.0f},
                    core::Color border_color = {},
                    core::Edges border_width = {}) noexcept;

    void push_glyph_run(gpu::TextureId atlas_texture,
                        std::span<const gpu::GlyphInstance> glyphs) noexcept;

    void push_path_mesh(std::span<const gpu::PathVertex> vertices,
                        std::span<const uint16_t> indices) noexcept;

    void push_path_fill(const Path& path, core::Color color,
                        FillRule rule = FillRule::NonZero) noexcept;

    void push_path_stroke(const Path& path, core::Color color,
                          const StrokeStyle& style) noexcept;

    [[nodiscard]] std::span<gpu::GlyphInstance> alloc_glyphs(size_t count) noexcept {
        return arena_.alloc_array<gpu::GlyphInstance>(count);
    }

    void set_glyph_atlas(text::GlyphAtlas* atlas) noexcept { atlas_ = atlas; }
    [[nodiscard]] text::GlyphAtlas* glyph_atlas() const noexcept { return atlas_; }

    void push_clip(core::Rect clip) noexcept;
    void pop_clip() noexcept;

    [[nodiscard]] core::Rect current_clip() const noexcept;
    [[nodiscard]] std::span<const DrawCommand> commands() const noexcept;
    [[nodiscard]] size_t size() const noexcept { return count_; }
    [[nodiscard]] bool empty() const noexcept { return count_ == 0; }

private:
    void ensure_capacity() noexcept;

    core::Arena& arena_;
    text::GlyphAtlas* atlas_{nullptr};
    DrawCommand* commands_{nullptr};
    size_t count_{0};
    size_t capacity_{0};

    static constexpr size_t kMaxClipDepth = 32;
    std::array<core::Rect, kMaxClipDepth> clip_stack_{};
    size_t clip_stack_depth_{0};
};

} // namespace north::render
