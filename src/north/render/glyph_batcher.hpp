#pragma once

#include "north/gpu/device.hpp"
#include "north/gpu/types.hpp"

#include <vector>

namespace north::render {

class GlyphBatcher {
public:
    GlyphBatcher() = default;
    ~GlyphBatcher();

    GlyphBatcher(const GlyphBatcher&) = delete;
    GlyphBatcher& operator=(const GlyphBatcher&) = delete;
    GlyphBatcher(GlyphBatcher&& other) noexcept;
    GlyphBatcher& operator=(GlyphBatcher&& other) noexcept;

    [[nodiscard]] bool init(gpu::Device& device, size_t initial_capacity = 1024);
    void shutdown(gpu::Device& device);

    void begin_batch() noexcept;
    void add_glyph(const gpu::GlyphInstance& glyph, gpu::TextureId texture, gpu::Device& device);
    void add_run(gpu::TextureId texture, std::span<const gpu::GlyphInstance> glyphs,
                 core::Rect clip_rect, gpu::Device& device);
    void flush(gpu::Device& device);

    [[nodiscard]] bool has_pending() const noexcept { return !instances_.empty(); }
    [[nodiscard]] size_t instance_count() const noexcept { return instances_.size(); }
    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }

private:
    gpu::PipelineId pipeline_{gpu::kInvalidPipeline};
    gpu::BufferId instance_buffer_{gpu::kInvalidBuffer};
    gpu::TextureId current_texture_{gpu::kInvalidTexture};
    size_t capacity_{0};
    std::vector<gpu::GlyphInstance> instances_;
};

} // namespace north::render
