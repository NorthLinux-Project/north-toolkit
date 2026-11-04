#pragma once

#include "north/core/arena.hpp"
#include "north/core/tree.hpp"
#include "north/gpu/device.hpp"
#include "north/render/display_list.hpp"
#include "north/render/glyph_batcher.hpp"
#include "north/render/quad_batcher.hpp"
#include "north/text/glyph_atlas.hpp"

namespace north::render {

void default_box_paint(core::NodeTree& tree, core::Node::HandleType handle, core::Point origin,
                       void* stream, void* user_data);

class Renderer {
public:
    explicit Renderer(gpu::Device& device);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) noexcept = default;
    Renderer& operator=(Renderer&&) noexcept = default;

    [[nodiscard]] bool init();
    void shutdown();

    void render_frame(core::NodeTree& tree, core::Size viewport_size, core::Color clear_color);

    [[nodiscard]] core::Arena& frame_arena() noexcept { return frame_arena_; }
    [[nodiscard]] QuadBatcher& quad_batcher() noexcept { return quad_batcher_; }
    [[nodiscard]] GlyphBatcher& glyph_batcher() noexcept { return glyph_batcher_; }
    [[nodiscard]] text::GlyphAtlas& glyph_atlas() noexcept { return glyph_atlas_; }

private:
    gpu::Device& device_;
    core::Arena frame_arena_{1024 * 1024};
    QuadBatcher quad_batcher_;
    GlyphBatcher glyph_batcher_;
    text::GlyphAtlas glyph_atlas_;
    bool initialized_{false};
};

} // namespace north::render
