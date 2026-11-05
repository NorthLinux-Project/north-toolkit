#pragma once

#include "north/core/types.hpp"
#include "north/gpu/types.hpp"

#include <optional>
#include <span>

namespace north::gpu {

class Device {
public:
    virtual ~Device() = default;

    [[nodiscard]] virtual std::optional<TextureId> create_texture(const TextureDesc& desc) = 0;
    virtual void update_texture(TextureId id, core::Rect region, std::span<const uint8_t> data) = 0;
    virtual void destroy_texture(TextureId id) = 0;

    [[nodiscard]] virtual std::optional<BufferId> create_buffer(const BufferDesc& desc) = 0;
    virtual void update_buffer(BufferId id, size_t offset, std::span<const uint8_t> data) = 0;
    virtual void destroy_buffer(BufferId id) = 0;

    [[nodiscard]] virtual std::optional<PipelineId>
    create_pipeline(std::span<const uint8_t> vs_bytecode, std::span<const uint8_t> ps_bytecode,
                    BlendMode blend_mode, VertexLayout layout = VertexLayout::Quad) = 0;
    virtual void destroy_pipeline(PipelineId id) = 0;

    virtual void begin_frame() = 0;
    virtual void clear(core::Color color) = 0;
    virtual void set_viewport(float width, float height) = 0;
    virtual void set_pipeline(PipelineId id) = 0;
    virtual void set_instance_buffer(BufferId id, uint32_t stride) = 0;
    virtual void set_vertex_buffer(BufferId id, uint32_t stride, uint32_t offset = 0) = 0;
    virtual void set_index_buffer(BufferId id, IndexFormat format = IndexFormat::UInt16,
                                  uint32_t offset = 0) = 0;
    virtual void set_constants(uint32_t slot, std::span<const uint8_t> data) = 0;
    virtual void set_texture(uint32_t slot, TextureId id) = 0;
    virtual void set_scissor(core::Rect rect) = 0;
    virtual void draw_instanced(uint32_t vertex_count_per_instance, uint32_t instance_count) = 0;
    virtual void draw_indexed(uint32_t index_count, uint32_t start_index = 0,
                              int32_t base_vertex = 0) = 0;
    virtual void end_frame() = 0;
    virtual void present() = 0;
    virtual void resize_swapchain(uint32_t width, uint32_t height) = 0;
};

} // namespace north::gpu
