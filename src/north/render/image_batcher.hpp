#pragma once

#include "north/gpu/device.hpp"
#include "north/gpu/types.hpp"
#include "north/render/display_list.hpp"

#include <vector>

namespace north::render {

class ImageBatcher {
public:
    ImageBatcher() = default;
    ~ImageBatcher();

    ImageBatcher(const ImageBatcher&) = delete;
    ImageBatcher& operator=(const ImageBatcher&) = delete;
    ImageBatcher(ImageBatcher&& other) noexcept;
    ImageBatcher& operator=(ImageBatcher&& other) noexcept;

    [[nodiscard]] bool init(gpu::Device& device, size_t initial_capacity = 256);
    void shutdown(gpu::Device& device);

    void begin_batch() noexcept;
    void add_image(const DrawImageCmd& cmd, core::Rect clip_rect, gpu::Device& device);
    void flush(gpu::Device& device);

    [[nodiscard]] size_t instance_count() const noexcept { return instances_.size(); }
    [[nodiscard]] bool has_pending() const noexcept { return !instances_.empty(); }

private:
    gpu::PipelineId pipeline_{gpu::kInvalidPipeline};
    gpu::BufferId instance_buffer_{gpu::kInvalidBuffer};
    gpu::TextureId current_texture_{gpu::kInvalidTexture};
    size_t capacity_{0};
    std::vector<gpu::ImageInstance> instances_;
};

} // namespace north::render
