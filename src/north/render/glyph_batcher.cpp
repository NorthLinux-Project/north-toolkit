#include "north/render/glyph_batcher.hpp"

#include <cstdint>
using BYTE = uint8_t;

#include "north/gpu/d3d11/shaders/glyph_ps.h"
#include "north/gpu/d3d11/shaders/glyph_vs.h"

#include <algorithm>

namespace north::render {

GlyphBatcher::~GlyphBatcher() = default;

GlyphBatcher::GlyphBatcher(GlyphBatcher&& other) noexcept
    : pipeline_(other.pipeline_), instance_buffer_(other.instance_buffer_),
      current_texture_(other.current_texture_), capacity_(other.capacity_),
      instances_(std::move(other.instances_)) {
    other.pipeline_ = gpu::kInvalidPipeline;
    other.instance_buffer_ = gpu::kInvalidBuffer;
    other.current_texture_ = gpu::kInvalidTexture;
    other.capacity_ = 0;
}

GlyphBatcher& GlyphBatcher::operator=(GlyphBatcher&& other) noexcept {
    if (this != &other) {
        pipeline_ = other.pipeline_;
        instance_buffer_ = other.instance_buffer_;
        current_texture_ = other.current_texture_;
        capacity_ = other.capacity_;
        instances_ = std::move(other.instances_);

        other.pipeline_ = gpu::kInvalidPipeline;
        other.instance_buffer_ = gpu::kInvalidBuffer;
        other.current_texture_ = gpu::kInvalidTexture;
        other.capacity_ = 0;
    }
    return *this;
}

bool GlyphBatcher::init(gpu::Device& device, size_t initial_capacity) {
    std::span<const uint8_t> vs_bytes(g_glyph_vs, sizeof(g_glyph_vs));
    std::span<const uint8_t> ps_bytes(g_glyph_ps, sizeof(g_glyph_ps));
    auto pipe = device.create_pipeline(vs_bytes, ps_bytes, gpu::BlendMode::PremultipliedAlpha,
                                       gpu::VertexLayout::Glyph);
    if (!pipe) {
        return false;
    }
    pipeline_ = *pipe;

    capacity_ = initial_capacity > 0 ? initial_capacity : 1024;
    gpu::BufferDesc bdesc{.size = capacity_ * sizeof(gpu::GlyphInstance),
                          .usage = gpu::BufferUsage::Vertex,
                          .dynamic = true};
    auto buf = device.create_buffer(bdesc);
    if (!buf) {
        device.destroy_pipeline(pipeline_);
        pipeline_ = gpu::kInvalidPipeline;
        return false;
    }
    instance_buffer_ = *buf;
    instances_.reserve(capacity_);
    return true;
}

void GlyphBatcher::shutdown(gpu::Device& device) {
    if (instance_buffer_ != gpu::kInvalidBuffer) {
        device.destroy_buffer(instance_buffer_);
        instance_buffer_ = gpu::kInvalidBuffer;
    }
    if (pipeline_ != gpu::kInvalidPipeline) {
        device.destroy_pipeline(pipeline_);
        pipeline_ = gpu::kInvalidPipeline;
    }
    capacity_ = 0;
    current_texture_ = gpu::kInvalidTexture;
    instances_.clear();
}

void GlyphBatcher::begin_batch() noexcept {
    instances_.clear();
    current_texture_ = gpu::kInvalidTexture;
}

void GlyphBatcher::add_glyph(const gpu::GlyphInstance& glyph, gpu::TextureId texture,
                             gpu::Device& device) {
    if (texture == gpu::kInvalidTexture) {
        return;
    }

    if (current_texture_ != gpu::kInvalidTexture && current_texture_ != texture) {
        flush(device);
    }

    current_texture_ = texture;
    instances_.push_back(glyph);
}

void GlyphBatcher::add_run(gpu::TextureId texture, std::span<const gpu::GlyphInstance> glyphs,
                           core::Rect clip_rect, gpu::Device& device) {
    if (glyphs.empty() || texture == gpu::kInvalidTexture) {
        return;
    }

    if (current_texture_ != gpu::kInvalidTexture && current_texture_ != texture) {
        flush(device);
    }

    current_texture_ = texture;
    for (const auto& g : glyphs) {
        gpu::GlyphInstance inst = g;
        inst.clip_rect = clip_rect;
        instances_.push_back(inst);
    }
}

void GlyphBatcher::flush(gpu::Device& device) {
    if (instances_.empty() || pipeline_ == gpu::kInvalidPipeline ||
        current_texture_ == gpu::kInvalidTexture) {
        return;
    }

    size_t required_bytes = instances_.size() * sizeof(gpu::GlyphInstance);
    if (instances_.size() > capacity_) {
        if (instance_buffer_ != gpu::kInvalidBuffer) {
            device.destroy_buffer(instance_buffer_);
            instance_buffer_ = gpu::kInvalidBuffer;
        }
        size_t new_cap = capacity_;
        while (new_cap < instances_.size()) {
            new_cap *= 2;
        }
        capacity_ = new_cap;
        gpu::BufferDesc bdesc{.size = capacity_ * sizeof(gpu::GlyphInstance),
                              .usage = gpu::BufferUsage::Vertex,
                              .dynamic = true};
        auto buf = device.create_buffer(bdesc);
        if (!buf) {
            return;
        }
        instance_buffer_ = *buf;
    }

    const auto* byte_ptr = reinterpret_cast<const uint8_t*>(instances_.data());
    std::span<const uint8_t> data_span(byte_ptr, required_bytes);
    device.update_buffer(instance_buffer_, 0, data_span);

    device.set_pipeline(pipeline_);
    device.set_texture(0, current_texture_);
    device.set_instance_buffer(instance_buffer_, static_cast<uint32_t>(sizeof(gpu::GlyphInstance)));
    device.draw_instanced(4, static_cast<uint32_t>(instances_.size()));

    instances_.clear();
}

} // namespace north::render
