#include "north/render/display_list.hpp"
#include "north/render/path_tessellator.hpp"

#include <algorithm>
#include <cstring>
#include <cstring>

namespace north::render {

DisplayList::DisplayList(core::Arena& arena, core::Rect initial_clip) noexcept
    : arena_(arena), count_(0), capacity_(64), clip_stack_depth_(1) {
    clip_stack_[0] = initial_clip;
    auto initial_span = arena_.alloc_array<DrawCommand>(capacity_);
    commands_ = initial_span.data();
}

void DisplayList::ensure_capacity() noexcept {
    if (count_ < capacity_) {
        return;
    }

    size_t new_cap = capacity_ == 0 ? 64 : capacity_ * 2;
    auto new_span = arena_.alloc_array<DrawCommand>(new_cap);
    if (!new_span.empty()) {
        if (commands_ && count_ > 0) {
            std::memcpy(new_span.data(), commands_, count_ * sizeof(DrawCommand));
        }
        commands_ = new_span.data();
        capacity_ = new_cap;
    }
}

void DisplayList::push_quad(core::Rect bounds, core::CornerRadii radii, core::Color fill,
                            core::Color border, core::Edges border_width) noexcept {
    ensure_capacity();
    if (!commands_ || count_ >= capacity_) {
        return;
    }

    DrawCommand& cmd = commands_[count_++];
    cmd.type = CommandType::Quad;
    cmd.clip_rect = current_clip();
    cmd.quad.bounds = bounds;
    cmd.quad.radii = radii;
    cmd.quad.fill_color = fill;
    cmd.quad.border_color = border;
    cmd.quad.border_width = border_width;
}

void DisplayList::push_shadow(core::Rect bounds, core::CornerRadii radii, core::Color color,
                              float blur_radius, float spread, core::Point offset) noexcept {
    ensure_capacity();
    if (!commands_ || count_ >= capacity_) {
        return;
    }

    DrawCommand& cmd = commands_[count_++];
    cmd.type = CommandType::Shadow;
    cmd.clip_rect = current_clip();
    cmd.shadow.bounds = bounds;
    cmd.shadow.radii = radii;
    cmd.shadow.color = color;
    cmd.shadow.blur_radius = blur_radius;
    cmd.shadow.spread = spread;
    cmd.shadow.offset = offset;
}

void DisplayList::push_image(gpu::TextureId texture, core::Rect bounds, core::CornerRadii radii,
                             core::Rect uv_rect, core::Color tint, core::Color border_color,
                             core::Edges border_width) noexcept {
    if (texture == gpu::kInvalidTexture) {
        return;
    }
    ensure_capacity();
    if (!commands_ || count_ >= capacity_) {
        return;
    }

    DrawCommand& cmd = commands_[count_++];
    cmd.type = CommandType::Image;
    cmd.clip_rect = current_clip();
    cmd.image.texture = texture;
    cmd.image.bounds = bounds;
    cmd.image.radii = radii;
    cmd.image.uv_rect = uv_rect;
    cmd.image.tint = tint;
    cmd.image.border_color = border_color;
    cmd.image.border_width = border_width;
}

void DisplayList::push_glyph_run(gpu::TextureId atlas_texture,
                                 std::span<const gpu::GlyphInstance> glyphs) noexcept {
    ensure_capacity();
    if (!commands_ || count_ >= capacity_) {
        return;
    }

    DrawCommand& cmd = commands_[count_++];
    cmd.type = CommandType::GlyphRun;
    cmd.clip_rect = current_clip();
    cmd.glyph_run.atlas_texture = atlas_texture;
    cmd.glyph_run.glyphs = glyphs;
}

void DisplayList::push_path_mesh(std::span<const gpu::PathVertex> vertices,
                                std::span<const uint16_t> indices) noexcept {
    if (vertices.empty() || indices.empty()) {
        return;
    }
    ensure_capacity();
    if (!commands_ || count_ >= capacity_) {
        return;
    }

    DrawCommand& cmd = commands_[count_++];
    cmd.type = CommandType::PathMesh;
    cmd.clip_rect = current_clip();
    cmd.path_mesh.vertices = vertices;
    cmd.path_mesh.indices = indices;
}

void DisplayList::push_path_fill(const Path& path, core::Color color,
                                FillRule rule) noexcept {
    PathMesh mesh;
    PathTessellator::tessellate_fill(path, color, current_clip(), rule, mesh);
    if (mesh.empty()) {
        return;
    }

    auto v_span = arena_.alloc_array<gpu::PathVertex>(mesh.vertices.size());
    auto i_span = arena_.alloc_array<uint16_t>(mesh.indices.size());
    if (v_span.empty() || i_span.empty()) {
        return;
    }

    std::memcpy(v_span.data(), mesh.vertices.data(), mesh.vertices.size() * sizeof(gpu::PathVertex));
    std::memcpy(i_span.data(), mesh.indices.data(), mesh.indices.size() * sizeof(uint16_t));

    push_path_mesh(v_span, i_span);
}

void DisplayList::push_path_stroke(const Path& path, core::Color color,
                                  const StrokeStyle& style) noexcept {
    PathMesh mesh;
    PathTessellator::tessellate_stroke(path, color, current_clip(), style, mesh);
    if (mesh.empty()) {
        return;
    }

    auto v_span = arena_.alloc_array<gpu::PathVertex>(mesh.vertices.size());
    auto i_span = arena_.alloc_array<uint16_t>(mesh.indices.size());
    if (v_span.empty() || i_span.empty()) {
        return;
    }

    std::memcpy(v_span.data(), mesh.vertices.data(), mesh.vertices.size() * sizeof(gpu::PathVertex));
    std::memcpy(i_span.data(), mesh.indices.data(), mesh.indices.size() * sizeof(uint16_t));

    push_path_mesh(v_span, i_span);
}

void DisplayList::push_clip(core::Rect clip) noexcept {
    if (clip_stack_depth_ >= kMaxClipDepth) {
        return;
    }
    core::Rect accumulated = current_clip().intersect(clip);
    clip_stack_[clip_stack_depth_++] = accumulated;
}

void DisplayList::pop_clip() noexcept {
    if (clip_stack_depth_ > 1) {
        --clip_stack_depth_;
    }
}

core::Rect DisplayList::current_clip() const noexcept {
    if (clip_stack_depth_ == 0) {
        return core::Rect{0.0f, 0.0f, 1e7f, 1e7f};
    }
    return clip_stack_[clip_stack_depth_ - 1];
}

std::span<const DrawCommand> DisplayList::commands() const noexcept {
    if (!commands_ || count_ == 0) {
        return {};
    }
    return std::span<const DrawCommand>(commands_, count_);
}

} // namespace north::render
