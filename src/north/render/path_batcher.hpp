#pragma once

#include "north/gpu/device.hpp"
#include "north/gpu/types.hpp"
#include "north/render/display_list.hpp"

#include <vector>

namespace north::render {

class PathBatcher {
public:
    PathBatcher() = default;
    ~PathBatcher();

    PathBatcher(const PathBatcher&) = delete;
    PathBatcher& operator=(const PathBatcher&) = delete;
    PathBatcher(PathBatcher&& other) noexcept;
    PathBatcher& operator=(PathBatcher&& other) noexcept;

    [[nodiscard]] bool init(gpu::Device& device, size_t initial_vertex_capacity = 4096,
                           size_t initial_index_capacity = 8192);
    void shutdown(gpu::Device& device);

    void begin_batch() noexcept;
    void add_mesh(std::span<const gpu::PathVertex> vertices,
                  std::span<const uint16_t> indices, core::Rect clip_rect);
    void batch(std::span<const DrawCommand> commands);
    void flush(gpu::Device& device);

    [[nodiscard]] size_t vertex_count() const noexcept { return vertices_.size(); }
    [[nodiscard]] size_t index_count() const noexcept { return indices_.size(); }
    [[nodiscard]] size_t vertex_capacity() const noexcept { return vertex_capacity_; }
    [[nodiscard]] size_t index_capacity() const noexcept { return index_capacity_; }

private:
    gpu::PipelineId pipeline_{gpu::kInvalidPipeline};
    gpu::BufferId vertex_buffer_{gpu::kInvalidBuffer};
    gpu::BufferId index_buffer_{gpu::kInvalidBuffer};
    size_t vertex_capacity_{0};
    size_t index_capacity_{0};
    std::vector<gpu::PathVertex> vertices_;
    std::vector<uint16_t> indices_;
};

} // namespace north::render
