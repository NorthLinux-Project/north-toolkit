#pragma once

#include "north/gpu/device.hpp"
#include "north/gpu/types.hpp"
#include "north/render/display_list.hpp"

#include <vector>

namespace north::render {

class QuadBatcher {
public:
    QuadBatcher() = default;
    ~QuadBatcher();

    QuadBatcher(const QuadBatcher&) = delete;
    QuadBatcher& operator=(const QuadBatcher&) = delete;
    QuadBatcher(QuadBatcher&& other) noexcept;
    QuadBatcher& operator=(QuadBatcher&& other) noexcept;

    [[nodiscard]] bool init(gpu::Device& device, size_t initial_capacity = 1024);
    void shutdown(gpu::Device& device);

    void begin_batch() noexcept;
    void add_quad(const DrawQuadCmd& cmd, core::Rect clip_rect);
    void add_shadow(const DrawShadowCmd& cmd, core::Rect clip_rect);
    void batch(std::span<const DrawCommand> commands);
    void flush(gpu::Device& device);

    [[nodiscard]] size_t instance_count() const noexcept { return instances_.size(); }
    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }

private:
    gpu::PipelineId pipeline_{gpu::kInvalidPipeline};
    gpu::BufferId instance_buffer_{gpu::kInvalidBuffer};
    size_t capacity_{0};
    std::vector<gpu::QuadInstance> instances_;
};

} // namespace north::render
