#include "north/render/image_batcher.hpp"

#include <cstdint>
using BYTE = uint8_t;

#include "north/gpu/d3d11/shaders/image_ps.h"
#include "north/gpu/d3d11/shaders/image_vs.h"

#include <algorithm>
#include <cstring>

namespace north::render {

ImageBatcher::~ImageBatcher() = default;

ImageBatcher::ImageBatcher(ImageBatcher&& other) noexcept
    : pipeline_(other.pipeline_), instance_buffer_(other.instance_buffer_),
      current_texture_(other.current_texture_), capacity_(other.capacity_),
      instances_(std::move(other.instances_)) {
    other.pipeline_ = gpu::kInvalidPipeline;
    other.instance_buffer_ = gpu::kInvalidBuffer;
    other.current_texture_ = gpu::kInvalidTexture;
    other.capacity_ = 0;
}

ImageBatcher& ImageBatcher::operator=(ImageBatcher&& other) noexcept {
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

bool ImageBatcher::init(gpu::Device& device, size_t initial_capacity) {
    std::span<const uint8_t> vs_bytes(g_image_vs, sizeof(g_image_vs));
    std::span<const uint8_t> ps_bytes(g_image_ps, sizeof(g_image_ps));
    auto pipe = device.create_pipeline(vs_bytes, ps_bytes, gpu::BlendMode::PremultipliedAlpha,
                                       gpu::VertexLayout::Image);
    if (!pipe) {
        return false;
    }
    pipeline_ = *pipe;

    capacity_ = initial_capacity > 0 ? initial_capacity : 256;
    gpu::BufferDesc bdesc{.size = capacity_ * sizeof(gpu::ImageInstance),
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

void ImageBatcher::shutdown(gpu::Device& device) {
    if (instance_buffer_ != gpu::kInvalidBuffer) {
        device.destroy_buffer(instance_buffer_);
        instance_buffer_ = gpu::kInvalidBuffer;
    }
    if (pipeline_ != gpu::kInvalidPipeline) {
        device.destroy_pipeline(pipeline_);
        pipeline_ = gpu::kInvalidPipeline;
    }
    current_texture_ = gpu::kInvalidTexture;
    capacity_ = 0;
    instances_.clear();
}

void ImageBatcher::begin_batch() noexcept {
    instances_.clear();
    current_texture_ = gpu::kInvalidTexture;
}

void ImageBatcher::add_image(const DrawImageCmd& cmd, core::Rect clip_rect, gpu::Device& device) {
    if (cmd.texture == gpu::kInvalidTexture) {
        return;
    }

    if (current_texture_ != gpu::kInvalidTexture && current_texture_ != cmd.texture &&
        !instances_.empty()) {
        flush(device);
    }
    current_texture_ = cmd.texture;

    gpu::ImageInstance inst{};
    inst.rect = cmd.bounds;
    inst.uv_rect = cmd.uv_rect;
    inst.radii = cmd.radii;
    inst.tint_color = cmd.tint.premultiply();
    inst.border_color = cmd.border_color.premultiply();
    inst.border_width = cmd.border_width;
    inst.clip_rect = clip_rect;

    instances_.push_back(inst);
}

void ImageBatcher::flush(gpu::Device& device) {
    if (instances_.empty() || pipeline_ == gpu::kInvalidPipeline ||
        current_texture_ == gpu::kInvalidTexture) {
        instances_.clear();
        return;
    }

    if (instances_.size() > capacity_) {
        capacity_ = instances_.size() * 2;
        if (instance_buffer_ != gpu::kInvalidBuffer) {
            device.destroy_buffer(instance_buffer_);
        }
        gpu::BufferDesc bdesc{.size = capacity_ * sizeof(gpu::ImageInstance),
                              .usage = gpu::BufferUsage::Vertex,
                              .dynamic = true};
        auto buf = device.create_buffer(bdesc);
        if (!buf) {
            instances_.clear();
            return;
        }
        instance_buffer_ = *buf;
    }

    std::span<const uint8_t> byte_span(reinterpret_cast<const uint8_t*>(instances_.data()),
                                       instances_.size() * sizeof(gpu::ImageInstance));
    device.update_buffer(instance_buffer_, 0, byte_span);

    device.set_pipeline(pipeline_);
    device.set_texture(0, current_texture_);
    device.set_instance_buffer(instance_buffer_, sizeof(gpu::ImageInstance));
    device.draw_instanced(4, static_cast<uint32_t>(instances_.size()));

    instances_.clear();
}

} // namespace north::render
