#include "north/render/path_batcher.hpp"

#include <cstdint>
using BYTE = uint8_t;

#include "north/gpu/d3d11/shaders/path_ps.h"
#include "north/gpu/d3d11/shaders/path_vs.h"

#include <algorithm>
#include <cstring>

namespace north::render {

PathBatcher::~PathBatcher() = default;

PathBatcher::PathBatcher(PathBatcher&& other) noexcept
    : pipeline_(other.pipeline_), vertex_buffer_(other.vertex_buffer_),
      index_buffer_(other.index_buffer_), vertex_capacity_(other.vertex_capacity_),
      index_capacity_(other.index_capacity_), vertices_(std::move(other.vertices_)),
      indices_(std::move(other.indices_)) {
    other.pipeline_ = gpu::kInvalidPipeline;
    other.vertex_buffer_ = gpu::kInvalidBuffer;
    other.index_buffer_ = gpu::kInvalidBuffer;
    other.vertex_capacity_ = 0;
    other.index_capacity_ = 0;
}

PathBatcher& PathBatcher::operator=(PathBatcher&& other) noexcept {
    if (this != &other) {
        pipeline_ = other.pipeline_;
        vertex_buffer_ = other.vertex_buffer_;
        index_buffer_ = other.index_buffer_;
        vertex_capacity_ = other.vertex_capacity_;
        index_capacity_ = other.index_capacity_;
        vertices_ = std::move(other.vertices_);
        indices_ = std::move(other.indices_);

        other.pipeline_ = gpu::kInvalidPipeline;
        other.vertex_buffer_ = gpu::kInvalidBuffer;
        other.index_buffer_ = gpu::kInvalidBuffer;
        other.vertex_capacity_ = 0;
        other.index_capacity_ = 0;
    }
    return *this;
}

bool PathBatcher::init(gpu::Device& device, size_t initial_vertex_capacity,
                       size_t initial_index_capacity) {
    std::span<const uint8_t> vs_bytes(g_path_vs, sizeof(g_path_vs));
    std::span<const uint8_t> ps_bytes(g_path_ps, sizeof(g_path_ps));
    auto pipe = device.create_pipeline(vs_bytes, ps_bytes, gpu::BlendMode::PremultipliedAlpha,
                                       gpu::VertexLayout::Path);
    if (!pipe) {
        return false;
    }
    pipeline_ = *pipe;

    vertex_capacity_ = initial_vertex_capacity > 0 ? initial_vertex_capacity : 4096;
    gpu::BufferDesc vb_desc{.size = vertex_capacity_ * sizeof(gpu::PathVertex),
                            .usage = gpu::BufferUsage::Vertex,
                            .dynamic = true};
    auto vbuf = device.create_buffer(vb_desc);
    if (!vbuf) {
        device.destroy_pipeline(pipeline_);
        pipeline_ = gpu::kInvalidPipeline;
        return false;
    }
    vertex_buffer_ = *vbuf;

    index_capacity_ = initial_index_capacity > 0 ? initial_index_capacity : 8192;
    gpu::BufferDesc ib_desc{.size = index_capacity_ * sizeof(uint16_t),
                            .usage = gpu::BufferUsage::Index,
                            .dynamic = true};
    auto ibuf = device.create_buffer(ib_desc);
    if (!ibuf) {
        device.destroy_buffer(vertex_buffer_);
        device.destroy_pipeline(pipeline_);
        vertex_buffer_ = gpu::kInvalidBuffer;
        pipeline_ = gpu::kInvalidPipeline;
        return false;
    }
    index_buffer_ = *ibuf;

    vertices_.reserve(vertex_capacity_);
    indices_.reserve(index_capacity_);
    return true;
}

void PathBatcher::shutdown(gpu::Device& device) {
    if (pipeline_ != gpu::kInvalidPipeline) {
        device.destroy_pipeline(pipeline_);
        pipeline_ = gpu::kInvalidPipeline;
    }
    if (vertex_buffer_ != gpu::kInvalidBuffer) {
        device.destroy_buffer(vertex_buffer_);
        vertex_buffer_ = gpu::kInvalidBuffer;
    }
    if (index_buffer_ != gpu::kInvalidBuffer) {
        device.destroy_buffer(index_buffer_);
        index_buffer_ = gpu::kInvalidBuffer;
    }
    vertices_.clear();
    indices_.clear();
    vertex_capacity_ = 0;
    index_capacity_ = 0;
}

void PathBatcher::begin_batch() noexcept {
    vertices_.clear();
    indices_.clear();
}

void PathBatcher::add_mesh(std::span<const gpu::PathVertex> vertices,
                          std::span<const uint16_t> indices, core::Rect clip_rect) {
    if (vertices.empty() || indices.empty()) {
        return;
    }

    uint16_t base_vertex = static_cast<uint16_t>(vertices_.size());

    for (auto idx : indices) {
        indices_.push_back(base_vertex + idx);
    }

    for (const auto& v : vertices) {
        gpu::PathVertex pv = v;
        if (clip_rect.width > 0.0f && clip_rect.height > 0.0f) {
            pv.clip_rect = pv.clip_rect.intersect(clip_rect);
        }
        vertices_.push_back(pv);
    }
}

void PathBatcher::batch(std::span<const DrawCommand> commands) {
    for (const auto& cmd : commands) {
        if (cmd.type == CommandType::PathMesh) {
            add_mesh(cmd.path_mesh.vertices, cmd.path_mesh.indices, cmd.clip_rect);
        }
    }
}

void PathBatcher::flush(gpu::Device& device) {
    if (indices_.empty() || vertices_.empty() || pipeline_ == gpu::kInvalidPipeline) {
        return;
    }

    // Reallocate vertex buffer if capacity exceeded
    if (vertices_.size() > vertex_capacity_) {
        if (vertex_buffer_ != gpu::kInvalidBuffer) {
            device.destroy_buffer(vertex_buffer_);
            vertex_buffer_ = gpu::kInvalidBuffer;
        }
        size_t new_cap = vertex_capacity_;
        while (new_cap < vertices_.size()) {
            new_cap *= 2;
        }
        vertex_capacity_ = new_cap;
        gpu::BufferDesc bdesc{.size = vertex_capacity_ * sizeof(gpu::PathVertex),
                              .usage = gpu::BufferUsage::Vertex,
                              .dynamic = true};
        auto buf = device.create_buffer(bdesc);
        if (!buf) {
            return;
        }
        vertex_buffer_ = *buf;
    }

    // Reallocate index buffer if capacity exceeded
    if (indices_.size() > index_capacity_) {
        if (index_buffer_ != gpu::kInvalidBuffer) {
            device.destroy_buffer(index_buffer_);
            index_buffer_ = gpu::kInvalidBuffer;
        }
        size_t new_cap = index_capacity_;
        while (new_cap < indices_.size()) {
            new_cap *= 2;
        }
        index_capacity_ = new_cap;
        gpu::BufferDesc bdesc{.size = index_capacity_ * sizeof(uint16_t),
                              .usage = gpu::BufferUsage::Index,
                              .dynamic = true};
        auto buf = device.create_buffer(bdesc);
        if (!buf) {
            return;
        }
        index_buffer_ = *buf;
    }

    // Upload vertices
    const auto* v_bytes = reinterpret_cast<const uint8_t*>(vertices_.data());
    std::span<const uint8_t> v_span(v_bytes, vertices_.size() * sizeof(gpu::PathVertex));
    device.update_buffer(vertex_buffer_, 0, v_span);

    // Upload indices
    const auto* i_bytes = reinterpret_cast<const uint8_t*>(indices_.data());
    std::span<const uint8_t> i_span(i_bytes, indices_.size() * sizeof(uint16_t));
    device.update_buffer(index_buffer_, 0, i_span);

    // Bind and draw
    device.set_pipeline(pipeline_);
    device.set_vertex_buffer(vertex_buffer_, static_cast<uint32_t>(sizeof(gpu::PathVertex)));
    device.set_index_buffer(index_buffer_, gpu::IndexFormat::UInt16);
    device.draw_indexed(static_cast<uint32_t>(indices_.size()));

    vertices_.clear();
    indices_.clear();
}

} // namespace north::render
