#include "north/gpu/d3d11/d3d11_device.hpp"

#include "north/core/assert.hpp"

#include <algorithm>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace north::gpu {

namespace {

DXGI_FORMAT to_dxgi_format(TextureFormat format) noexcept {
    switch (format) {
    case TextureFormat::RGBA8:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::R8:
        return DXGI_FORMAT_R8_UNORM;
    case TextureFormat::BGRA8:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    }
    return DXGI_FORMAT_R8G8B8A8_UNORM;
}

size_t format_pixel_size(TextureFormat format) noexcept {
    switch (format) {
    case TextureFormat::RGBA8:
    case TextureFormat::BGRA8:
        return 4;
    case TextureFormat::R8:
        return 1;
    }
    return 4;
}

} // namespace

D3D11Device::D3D11Device() = default;

D3D11Device::~D3D11Device() {
    if (context_) {
        context_->ClearState();
    }
}

bool D3D11Device::create_internal_device() {
    if (device_) {
        return true;
    }

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if !defined(NDEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                                          D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};

    D3D_FEATURE_LEVEL active_level{};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                   feature_levels, static_cast<UINT>(std::size(feature_levels)),
                                   D3D11_SDK_VERSION, &device_, &active_level, &context_);

    if (FAILED(hr)) {
        // Fallback to WARP software driver
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, feature_levels,
                               static_cast<UINT>(std::size(feature_levels)), D3D11_SDK_VERSION,
                               &device_, &active_level, &context_);
    }

    if (FAILED(hr)) {
        return false;
    }

    // Create linear clamp sampler state
    D3D11_SAMPLER_DESC samp_desc{};
    samp_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samp_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samp_desc.MinLOD = 0;
    samp_desc.MaxLOD = D3D11_FLOAT32_MAX;
    device_->CreateSamplerState(&samp_desc, &linear_sampler_);

    // Create rasterizer state with scissor enabled
    D3D11_RASTERIZER_DESC rast_desc{};
    rast_desc.FillMode = D3D11_FILL_SOLID;
    rast_desc.CullMode = D3D11_CULL_NONE;
    rast_desc.ScissorEnable = FALSE;
    rast_desc.DepthClipEnable = TRUE;
    device_->CreateRasterizerState(&rast_desc, &rasterizer_state_);

    return true;
}

bool D3D11Device::init_offscreen(uint32_t width, uint32_t height) {
    if (!create_internal_device()) {
        return false;
    }

    current_vp_width_ = static_cast<float>(width);
    current_vp_height_ = static_cast<float>(height);

    TextureDesc desc{
        .width = width, .height = height, .format = TextureFormat::RGBA8, .render_target = true};

    auto tex_id = create_texture(desc);
    if (!tex_id.has_value()) {
        return false;
    }

    default_rtv_ = textures_[*tex_id - 1].rtv;
    return default_rtv_ != nullptr;
}

bool D3D11Device::init_swapchain(HWND hwnd, uint32_t width, uint32_t height) {
    if (!create_internal_device()) {
        return false;
    }

    current_vp_width_ = static_cast<float>(width);
    current_vp_height_ = static_cast<float>(height);

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    if (FAILED(device_.As(&dxgi_device)))
        return false;

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgi_device->GetAdapter(&adapter)))
        return false;

    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))))
        return false;

    DXGI_SWAP_CHAIN_DESC1 sc_desc{};
    sc_desc.Width = width;
    sc_desc.Height = height;
    sc_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sc_desc.SampleDesc.Count = 1;
    sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc_desc.BufferCount = 2;
    sc_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sc_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    HRESULT hr = factory->CreateSwapChainForHwnd(device_.Get(), hwnd, &sc_desc, nullptr, nullptr,
                                                 &swapchain_);

    if (FAILED(hr))
        return false;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    if (FAILED(swapchain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer))))
        return false;

    if (FAILED(device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &default_rtv_))) {
        return false;
    }

    return true;
}

std::optional<TextureId> D3D11Device::create_texture(const TextureDesc& desc) {
    if (!device_)
        return std::nullopt;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = desc.width;
    td.Height = desc.height;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = to_dxgi_format(desc.format);
    td.SampleDesc.Count = 1;
    td.Usage = desc.render_target ? D3D11_USAGE_DEFAULT : D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (desc.render_target) {
        td.BindFlags |= D3D11_BIND_RENDER_TARGET;
    }

    TextureRecord rec;
    rec.desc = desc;

    HRESULT hr = device_->CreateTexture2D(&td, nullptr, &rec.texture);
    if (FAILED(hr))
        return std::nullopt;

    hr = device_->CreateShaderResourceView(rec.texture.Get(), nullptr, &rec.srv);
    if (FAILED(hr))
        return std::nullopt;

    if (desc.render_target) {
        hr = device_->CreateRenderTargetView(rec.texture.Get(), nullptr, &rec.rtv);
        if (FAILED(hr))
            return std::nullopt;
    }

    rec.active = true;

    for (size_t i = 0; i < textures_.size(); ++i) {
        if (!textures_[i].active) {
            textures_[i] = std::move(rec);
            return static_cast<TextureId>(i + 1);
        }
    }

    textures_.push_back(std::move(rec));
    return static_cast<TextureId>(textures_.size());
}

void D3D11Device::update_texture(TextureId id, core::Rect region, std::span<const uint8_t> data) {
    if (id == kInvalidTexture || id > textures_.size() || !context_)
        return;
    auto& rec = textures_[id - 1];
    if (!rec.active || !rec.texture)
        return;

    D3D11_BOX box{};
    box.left = static_cast<UINT>((std::max)(0.0f, region.x));
    box.top = static_cast<UINT>((std::max)(0.0f, region.y));
    box.right =
        static_cast<UINT>((std::min)(static_cast<float>(rec.desc.width), region.x + region.width));
    box.bottom = static_cast<UINT>(
        (std::min)(static_cast<float>(rec.desc.height), region.y + region.height));
    box.front = 0;
    box.back = 1;

    UINT row_pitch = static_cast<UINT>((box.right - box.left) * format_pixel_size(rec.desc.format));

    context_->UpdateSubresource(rec.texture.Get(), 0, &box, data.data(), row_pitch, 0);
}

void D3D11Device::destroy_texture(TextureId id) {
    if (id == kInvalidTexture || id > textures_.size())
        return;
    textures_[id - 1].active = false;
    textures_[id - 1].texture.Reset();
    textures_[id - 1].srv.Reset();
    textures_[id - 1].rtv.Reset();
}

std::optional<BufferId> D3D11Device::create_buffer(const BufferDesc& desc) {
    if (!device_ || desc.size == 0)
        return std::nullopt;

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = static_cast<UINT>(desc.size);
    bd.Usage = desc.dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    bd.CPUAccessFlags = desc.dynamic ? D3D11_CPU_ACCESS_WRITE : 0;

    switch (desc.usage) {
    case BufferUsage::Vertex:
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        break;
    case BufferUsage::Index:
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        break;
    case BufferUsage::Constant:
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        // Constant buffers in D3D11 must be 16-byte multiples
        bd.ByteWidth = static_cast<UINT>((desc.size + 15) & ~15);
        break;
    }

    BufferRecord rec;
    rec.desc = desc;

    HRESULT hr = device_->CreateBuffer(&bd, nullptr, &rec.buffer);
    if (FAILED(hr))
        return std::nullopt;

    rec.active = true;

    for (size_t i = 0; i < buffers_.size(); ++i) {
        if (!buffers_[i].active) {
            buffers_[i] = std::move(rec);
            return static_cast<BufferId>(i + 1);
        }
    }

    buffers_.push_back(std::move(rec));
    return static_cast<BufferId>(buffers_.size());
}

void D3D11Device::update_buffer(BufferId id, size_t offset, std::span<const uint8_t> data) {
    if (id == kInvalidBuffer || id > buffers_.size() || !context_)
        return;
    auto& rec = buffers_[id - 1];
    if (!rec.active || !rec.buffer || data.empty())
        return;

    if (rec.desc.dynamic) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = context_->Map(rec.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            std::memcpy(static_cast<uint8_t*>(mapped.pData) + offset, data.data(), data.size());
            context_->Unmap(rec.buffer.Get(), 0);
        }
    } else {
        context_->UpdateSubresource(rec.buffer.Get(), 0, nullptr, data.data(), 0, 0);
    }
}

void D3D11Device::destroy_buffer(BufferId id) {
    if (id == kInvalidBuffer || id > buffers_.size())
        return;
    buffers_[id - 1].active = false;
    buffers_[id - 1].buffer.Reset();
}

Microsoft::WRL::ComPtr<ID3D11BlendState> D3D11Device::create_blend_state(BlendMode mode) {
    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable = (mode != BlendMode::Opaque);
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    if (mode == BlendMode::PremultipliedAlpha) {
        bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    } else if (mode == BlendMode::Alpha) {
        bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    }

    Microsoft::WRL::ComPtr<ID3D11BlendState> state;
    device_->CreateBlendState(&bd, &state);
    return state;
}

std::optional<PipelineId> D3D11Device::create_pipeline(std::span<const uint8_t> vs_bytecode,
                                                       std::span<const uint8_t> ps_bytecode,
                                                       BlendMode blend_mode, VertexLayout layout) {
    if (!device_ || vs_bytecode.empty() || ps_bytecode.empty())
        return std::nullopt;

    PipelineRecord rec;

    HRESULT hr =
        device_->CreateVertexShader(vs_bytecode.data(), vs_bytecode.size(), nullptr, &rec.vs);
    if (FAILED(hr))
        return std::nullopt;

    hr = device_->CreatePixelShader(ps_bytecode.data(), ps_bytecode.size(), nullptr, &rec.ps);
    if (FAILED(hr))
        return std::nullopt;

    const D3D11_INPUT_ELEMENT_DESC quad_layout_desc[] = {
        {"RECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"RADII", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"FILL_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"BORDER_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_INSTANCE_DATA,
         1},
        {"BORDER_WIDTH", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 64, D3D11_INPUT_PER_INSTANCE_DATA,
         1},
        {"CLIP_RECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 80, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"SHADOW_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 96, D3D11_INPUT_PER_INSTANCE_DATA,
         1},
        {"SHADOW_PARAMS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 112, D3D11_INPUT_PER_INSTANCE_DATA,
         1}};

    const D3D11_INPUT_ELEMENT_DESC glyph_layout_desc[] = {
        {"DST_RECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"UV_RECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"CLIP_RECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1}};

    const D3D11_INPUT_ELEMENT_DESC* layout_elements = quad_layout_desc;
    UINT num_elements = static_cast<UINT>(std::size(quad_layout_desc));

    if (layout == VertexLayout::Glyph) {
        layout_elements = glyph_layout_desc;
        num_elements = static_cast<UINT>(std::size(glyph_layout_desc));
    }

    hr = device_->CreateInputLayout(layout_elements, num_elements, vs_bytecode.data(),
                                    vs_bytecode.size(), &rec.layout);
    if (FAILED(hr))
        return std::nullopt;

    rec.blend = create_blend_state(blend_mode);
    rec.active = true;

    for (size_t i = 0; i < pipelines_.size(); ++i) {
        if (!pipelines_[i].active) {
            pipelines_[i] = std::move(rec);
            return static_cast<PipelineId>(i + 1);
        }
    }

    pipelines_.push_back(std::move(rec));
    return static_cast<PipelineId>(pipelines_.size());
}

void D3D11Device::destroy_pipeline(PipelineId id) {
    if (id == kInvalidPipeline || id > pipelines_.size())
        return;
    pipelines_[id - 1].active = false;
    pipelines_[id - 1].vs.Reset();
    pipelines_[id - 1].ps.Reset();
    pipelines_[id - 1].layout.Reset();
    pipelines_[id - 1].blend.Reset();
}

void D3D11Device::begin_frame() {
    if (!context_ || !default_rtv_)
        return;
    context_->OMSetRenderTargets(1, default_rtv_.GetAddressOf(), nullptr);

    if (rasterizer_state_) {
        context_->RSSetState(rasterizer_state_.Get());
    }
    if (linear_sampler_) {
        context_->PSSetSamplers(0, 1, linear_sampler_.GetAddressOf());
    }

    set_viewport(current_vp_width_, current_vp_height_);
}

void D3D11Device::clear(core::Color color) {
    if (!context_ || !default_rtv_)
        return;
    float rgba[4] = {color.r, color.g, color.b, color.a};
    context_->ClearRenderTargetView(default_rtv_.Get(), rgba);
}

void D3D11Device::set_viewport(float width, float height) {
    if (!context_)
        return;
    current_vp_width_ = width;
    current_vp_height_ = height;

    D3D11_VIEWPORT vp{};
    vp.Width = width;
    vp.Height = height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);
}

void D3D11Device::set_pipeline(PipelineId id) {
    if (id == kInvalidPipeline || id > pipelines_.size() || !context_)
        return;
    auto& rec = pipelines_[id - 1];
    if (!rec.active)
        return;

    active_pipeline_ = id;
    context_->VSSetShader(rec.vs.Get(), nullptr, 0);
    context_->PSSetShader(rec.ps.Get(), nullptr, 0);
    if (rec.layout) {
        context_->IASetInputLayout(rec.layout.Get());
    }
    if (rec.blend) {
        float blend_factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        context_->OMSetBlendState(rec.blend.Get(), blend_factor, 0xFFFFFFFF);
    }
}

void D3D11Device::set_instance_buffer(BufferId id, uint32_t stride) {
    if (id == kInvalidBuffer || id > buffers_.size() || !context_)
        return;
    auto& rec = buffers_[id - 1];
    if (!rec.active || !rec.buffer)
        return;

    UINT strides[1] = {stride};
    UINT offsets[1] = {0};
    ID3D11Buffer* bufs[1] = {rec.buffer.Get()};
    context_->IASetVertexBuffers(0, 1, bufs, strides, offsets);
}

void D3D11Device::set_constants(uint32_t slot, std::span<const uint8_t> data) {
    if (!context_ || !device_ || data.empty())
        return;

    while (constant_buffers_.size() <= slot) {
        constant_buffers_.emplace_back();
    }

    if (!constant_buffers_[slot]) {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = static_cast<UINT>((data.size() + 15) & ~15);
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device_->CreateBuffer(&bd, nullptr, &constant_buffers_[slot]);
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr =
        context_->Map(constant_buffers_[slot].Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        std::memcpy(mapped.pData, data.data(), data.size());
        context_->Unmap(constant_buffers_[slot].Get(), 0);
    }

    ID3D11Buffer* cb[1] = {constant_buffers_[slot].Get()};
    context_->VSSetConstantBuffers(slot, 1, cb);
    context_->PSSetConstantBuffers(slot, 1, cb);
}

void D3D11Device::set_texture(uint32_t slot, TextureId id) {
    if (!context_)
        return;
    if (id == kInvalidTexture || id > textures_.size()) {
        ID3D11ShaderResourceView* null_srv[1] = {nullptr};
        context_->PSSetShaderResources(slot, 1, null_srv);
        return;
    }
    auto& rec = textures_[id - 1];
    if (rec.active && rec.srv) {
        context_->PSSetShaderResources(slot, 1, rec.srv.GetAddressOf());
    }
}

void D3D11Device::set_scissor(core::Rect rect) {
    if (!context_)
        return;
    D3D11_RECT r{};
    r.left = static_cast<LONG>(rect.x);
    r.top = static_cast<LONG>(rect.y);
    r.right = static_cast<LONG>(rect.x + rect.width);
    r.bottom = static_cast<LONG>(rect.y + rect.height);
    context_->RSSetScissorRects(1, &r);
}

void D3D11Device::draw_instanced(uint32_t vertex_count_per_instance, uint32_t instance_count) {
    if (!context_ || instance_count == 0)
        return;
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context_->DrawInstanced(vertex_count_per_instance, instance_count, 0, 0);
}

void D3D11Device::end_frame() {
    if (!context_)
        return;
    // Unbind render targets
    ID3D11RenderTargetView* null_rtv[1] = {nullptr};
    context_->OMSetRenderTargets(1, null_rtv, nullptr);
}

void D3D11Device::present() {
    if (swapchain_) {
        swapchain_->Present(1, 0);
    }
}

void D3D11Device::resize_swapchain(uint32_t width, uint32_t height) {
    if (!swapchain_ || !device_ || width == 0 || height == 0)
        return;

    if (context_) {
        ID3D11RenderTargetView* null_rtv[1] = {nullptr};
        context_->OMSetRenderTargets(1, null_rtv, nullptr);
    }
    default_rtv_.Reset();

    HRESULT hr = swapchain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr))
        return;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    hr = swapchain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (FAILED(hr))
        return;

    device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &default_rtv_);
    current_vp_width_ = static_cast<float>(width);
    current_vp_height_ = static_cast<float>(height);
}

} // namespace north::gpu
