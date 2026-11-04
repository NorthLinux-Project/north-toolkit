#include "north/render/quad_batcher.hpp"

#include <cstdint>
using BYTE = uint8_t;

#include "north/gpu/d3d11/shaders/quad_ps.h"
#include "north/gpu/d3d11/shaders/quad_vs.h"

#include <algorithm>

namespace north::render {

QuadBatcher::~QuadBatcher() = default;

QuadBatcher::QuadBatcher(QuadBatcher&& other) noexcept
    : pipeline_(other.pipeline_), instance_buffer_(other.instance_buffer_),
      capacity_(other.capacity_), instances_(std::move(other.instances_)) {
    other.pipeline_ = gpu::kInvalidPipeline;
    other.instance_buffer_ = gpu::kInvalidBuffer;
    other.capacity_ = 0;
}

QuadBatcher& QuadBatcher::operator=(QuadBatcher&& other) noexcept {
    if (this != &other) {
        pipeline_ = other.pipeline_;
        instance_buffer_ = other.instance_buffer_;
        capacity_ = other.capacity_;
        instances_ = std::move(other.instances_);

        other.pipeline_ = gpu::kInvalidPipeline;
        other.instance_buffer_ = gpu::kInvalidBuffer;
        other.capacity_ = 0;
    }
    return *this;
}

bool QuadBatcher::init(gpu::Device& device, size_t initial_capacity) {
    std::span<const uint8_t> vs_bytes(g_quad_vs, sizeof(g_quad_vs));
    std::span<const uint8_t> ps_bytes(g_quad_ps, sizeof(g_quad_ps));
    auto pipe = device.create_pipeline(vs_bytes, ps_bytes, gpu::BlendMode::PremultipliedAlpha);
    if (!pipe) {
        return false;
    }
    pipeline_ = *pipe;

    capacity_ = initial_capacity > 0 ? initial_capacity : 1024;
    gpu::BufferDesc bdesc{.size = capacity_ * sizeof(gpu::QuadInstance),
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

void QuadBatcher::shutdown(gpu::Device& device) {
    if (instance_buffer_ != gpu::kInvalidBuffer) {
        device.destroy_buffer(instance_buffer_);
        instance_buffer_ = gpu::kInvalidBuffer;
    }
    if (pipeline_ != gpu::kInvalidPipeline) {
        device.destroy_pipeline(pipeline_);
        pipeline_ = gpu::kInvalidPipeline;
    }
    capacity_ = 0;
    instances_.clear();
}

void QuadBatcher::begin_batch() noexcept {
    instances_.clear();
}

void QuadBatcher::add_quad(const DrawQuadCmd& cmd, core::Rect clip_rect) {
    gpu::QuadInstance inst{};
    inst.rect = cmd.bounds;
    inst.radii = cmd.radii;
    inst.fill_color = cmd.fill_color.premultiply();
    inst.border_color = cmd.border_color.premultiply();
    inst.border_width = cmd.border_width;
    inst.clip_rect = clip_rect;
    inst.shadow_color = core::Color{0.0f, 0.0f, 0.0f, 0.0f};
    inst.shadow_radius = 0.0f;
    inst.shadow_spread = 0.0f;
    inst.shadow_offset_x = 0.0f;
    inst.shadow_offset_y = 0.0f;
    instances_.push_back(inst);
}

void QuadBatcher::add_shadow(const DrawShadowCmd& cmd, core::Rect clip_rect) {
    gpu::QuadInstance inst{};
    inst.rect = cmd.bounds;
    inst.radii = cmd.radii;
    inst.fill_color = core::Color{0.0f, 0.0f, 0.0f, 0.0f};
    inst.border_color = core::Color{0.0f, 0.0f, 0.0f, 0.0f};
    inst.border_width = core::Edges{0.0f, 0.0f, 0.0f, 0.0f};
    inst.clip_rect = clip_rect;
    inst.shadow_color = cmd.color.premultiply();
    inst.shadow_radius = cmd.blur_radius;
    inst.shadow_spread = cmd.spread;
    inst.shadow_offset_x = cmd.offset.x;
    inst.shadow_offset_y = cmd.offset.y;
    instances_.push_back(inst);
}

void QuadBatcher::batch(std::span<const DrawCommand> commands) {
    for (const auto& cmd : commands) {
        switch (cmd.type) {
        case CommandType::Quad:
            add_quad(cmd.quad, cmd.clip_rect);
            break;
        case CommandType::Shadow:
            add_shadow(cmd.shadow, cmd.clip_rect);
            break;
        case CommandType::GlyphRun:
            break;
        }
    }
}

void QuadBatcher::flush(gpu::Device& device) {
    if (instances_.empty() || pipeline_ == gpu::kInvalidPipeline) {
        return;
    }

    size_t required_bytes = instances_.size() * sizeof(gpu::QuadInstance);
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
        gpu::BufferDesc bdesc{.size = capacity_ * sizeof(gpu::QuadInstance),
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
    device.set_instance_buffer(instance_buffer_, static_cast<uint32_t>(sizeof(gpu::QuadInstance)));
    device.draw_instanced(4, static_cast<uint32_t>(instances_.size()));

    instances_.clear();
}

} // namespace north::render
