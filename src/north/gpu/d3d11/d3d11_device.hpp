#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "north/gpu/device.hpp"

#include <memory>
#include <vector>

#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_3.h>

#include <wrl/client.h>

namespace north::gpu {

class D3D11Device : public Device {
public:
    D3D11Device();
    ~D3D11Device() override;

    [[nodiscard]] bool init_offscreen(uint32_t width = 800, uint32_t height = 600);
    [[nodiscard]] bool init_swapchain(HWND hwnd, uint32_t width, uint32_t height);

    [[nodiscard]] std::optional<TextureId> create_texture(const TextureDesc& desc) override;
    void update_texture(TextureId id, core::Rect region, std::span<const uint8_t> data) override;
    void destroy_texture(TextureId id) override;

    [[nodiscard]] std::optional<BufferId> create_buffer(const BufferDesc& desc) override;
    void update_buffer(BufferId id, size_t offset, std::span<const uint8_t> data) override;
    void destroy_buffer(BufferId id) override;

    [[nodiscard]] std::optional<PipelineId>
    create_pipeline(std::span<const uint8_t> vs_bytecode, std::span<const uint8_t> ps_bytecode,
                    BlendMode blend_mode, VertexLayout layout = VertexLayout::Quad) override;
    void destroy_pipeline(PipelineId id) override;

    void begin_frame() override;
    void clear(core::Color color) override;
    void set_viewport(float width, float height) override;
    void set_pipeline(PipelineId id) override;
    void set_instance_buffer(BufferId id, uint32_t stride) override;
    void set_constants(uint32_t slot, std::span<const uint8_t> data) override;
    void set_texture(uint32_t slot, TextureId id) override;
    void set_scissor(core::Rect rect) override;
    void draw_instanced(uint32_t vertex_count_per_instance, uint32_t instance_count) override;
    void end_frame() override;
    void present() override;
    void resize_swapchain(uint32_t width, uint32_t height) override;

    [[nodiscard]] ID3D11Device* raw_device() const noexcept { return device_.Get(); }
    [[nodiscard]] ID3D11DeviceContext* raw_context() const noexcept { return context_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapchain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> default_rtv_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> linear_sampler_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_state_;

    struct TextureRecord {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        TextureDesc desc{};
        bool active{false};
    };

    struct BufferRecord {
        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
        BufferDesc desc{};
        bool active{false};
    };

    struct PipelineRecord {
        Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
        Microsoft::WRL::ComPtr<ID3D11BlendState> blend;
        bool active{false};
    };

    std::vector<TextureRecord> textures_;
    std::vector<BufferRecord> buffers_;
    std::vector<PipelineRecord> pipelines_;
    std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> constant_buffers_;

    PipelineId active_pipeline_{kInvalidPipeline};
    float current_vp_width_{800.0f};
    float current_vp_height_{600.0f};

    [[nodiscard]] bool create_internal_device();
    [[nodiscard]] Microsoft::WRL::ComPtr<ID3D11BlendState> create_blend_state(BlendMode mode);
};

} // namespace north::gpu
