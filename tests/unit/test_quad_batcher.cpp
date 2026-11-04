#include "north/gpu/d3d11/d3d11_device.hpp"
#include "north/render/quad_batcher.hpp"

#include <gtest/gtest.h>

namespace north::render::testing {

class QuadBatcherTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        device = std::make_unique<gpu::D3D11Device>();
        initialized = device->init_offscreen(800, 600);
    }

    void TearDown() override { device.reset(); }

    std::unique_ptr<gpu::D3D11Device> device;
    bool initialized{false};
};

TEST_F(QuadBatcherTestFixture, InitializesAndShutsDownCleanly) {
    ASSERT_TRUE(initialized);

    QuadBatcher batcher;
    EXPECT_TRUE(batcher.init(*device, 512));
    EXPECT_EQ(batcher.capacity(), 512);
    EXPECT_EQ(batcher.instance_count(), 0);

    batcher.shutdown(*device);
    EXPECT_EQ(batcher.capacity(), 0);
}

TEST_F(QuadBatcherTestFixture, BatchesQuadsAndShadowsWithPremultipliedColors) {
    ASSERT_TRUE(initialized);

    QuadBatcher batcher;
    ASSERT_TRUE(batcher.init(*device, 64));

    DrawQuadCmd quad_cmd{.bounds = core::Rect{10.0f, 20.0f, 100.0f, 50.0f},
                         .radii = core::CornerRadii::uniform(4.0f),
                         .fill_color = core::Color{1.0f, 0.5f, 0.25f, 0.5f},
                         .border_color = core::Color{0.0f, 0.0f, 1.0f, 0.5f},
                         .border_width = core::Edges::uniform(1.0f)};
    batcher.add_quad(quad_cmd, core::Rect{0.0f, 0.0f, 800.0f, 600.0f});

    DrawShadowCmd shadow_cmd{.bounds = core::Rect{10.0f, 20.0f, 100.0f, 50.0f},
                             .radii = core::CornerRadii::uniform(4.0f),
                             .color = core::Color{0.0f, 0.0f, 0.0f, 0.8f},
                             .blur_radius = 8.0f,
                             .spread = 2.0f,
                             .offset = core::Point{0.0f, 4.0f}};
    batcher.add_shadow(shadow_cmd, core::Rect{0.0f, 0.0f, 800.0f, 600.0f});

    EXPECT_EQ(batcher.instance_count(), 2);

    device->begin_frame();
    device->set_viewport(800.0f, 600.0f);
    batcher.flush(*device);
    device->end_frame();

    EXPECT_EQ(batcher.instance_count(), 0);
    batcher.shutdown(*device);
}

TEST_F(QuadBatcherTestFixture, DynamicBufferExpansionOnLargeBatch) {
    ASSERT_TRUE(initialized);

    QuadBatcher batcher;
    // Start with small capacity of 64
    ASSERT_TRUE(batcher.init(*device, 64));
    EXPECT_EQ(batcher.capacity(), 64);

    // Add 250 quads
    for (size_t i = 0; i < 250; ++i) {
        DrawQuadCmd cmd{.bounds = core::Rect{static_cast<float>(i), 10.0f, 50.0f, 50.0f},
                        .radii = {},
                        .fill_color = core::Color{1.0f, 1.0f, 1.0f, 1.0f},
                        .border_color = {},
                        .border_width = {}};
        batcher.add_quad(cmd, core::Rect{0.0f, 0.0f, 800.0f, 600.0f});
    }

    EXPECT_EQ(batcher.instance_count(), 250);

    device->begin_frame();
    device->set_viewport(800.0f, 600.0f);
    batcher.flush(*device);
    device->end_frame();

    EXPECT_EQ(batcher.instance_count(), 0);
    EXPECT_GE(batcher.capacity(), 250);

    batcher.shutdown(*device);
}

TEST_F(QuadBatcherTestFixture, RendersSmoothCircularAntiAliasingOnCardinalAxes) {
    ASSERT_TRUE(initialized);

    QuadBatcher batcher;
    ASSERT_TRUE(batcher.init(*device, 64));

    device->begin_frame();
    device->clear(core::Color{0.0f, 0.0f, 0.0f, 0.0f});
    device->set_viewport(800.0f, 600.0f);

    gpu::ViewConstants view_constants{
        .viewport_width = 800.0f, .viewport_height = 600.0f, .pad = {0.0f, 0.0f}};
    const auto* const_bytes = reinterpret_cast<const uint8_t*>(&view_constants);
    std::span<const uint8_t> const_span(const_bytes, sizeof(view_constants));
    device->set_constants(0, const_span);

    DrawQuadCmd circle_cmd{.bounds = core::Rect{100.5f, 100.5f, 40.0f, 40.0f},
                           .radii = core::CornerRadii::uniform(20.0f),
                           .fill_color = core::Color{1.0f, 1.0f, 1.0f, 1.0f},
                           .border_color = core::Color{0.0f, 0.0f, 0.0f, 0.0f},
                           .border_width = core::Edges{0.0f, 0.0f, 0.0f, 0.0f}};
    batcher.add_quad(circle_cmd, core::Rect{0.0f, 0.0f, 800.0f, 600.0f});
    batcher.flush(*device);

    auto* ctx = device->raw_context();
    auto* d3d = device->raw_device();
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    ctx->OMGetRenderTargets(1, rtv.GetAddressOf(), nullptr);
    ASSERT_NE(rtv, nullptr);

    Microsoft::WRL::ComPtr<ID3D11Resource> rt_res;
    rtv->GetResource(rt_res.GetAddressOf());
    ASSERT_NE(rt_res, nullptr);

    device->end_frame();

    D3D11_TEXTURE2D_DESC staging_desc{};
    staging_desc.Width = 800;
    staging_desc.Height = 600;
    staging_desc.MipLevels = 1;
    staging_desc.ArraySize = 1;
    staging_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    staging_desc.SampleDesc.Count = 1;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
    HRESULT hr = d3d->CreateTexture2D(&staging_desc, nullptr, staging.GetAddressOf());
    ASSERT_EQ(hr, S_OK);

    ctx->CopyResource(staging.Get(), rt_res.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = ctx->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    ASSERT_EQ(hr, S_OK);

    const auto* row_bytes = static_cast<const uint8_t*>(mapped.pData);
    auto get_pixel_alpha = [&](int x, int y) -> uint8_t {
        const uint8_t* p = row_bytes + y * mapped.RowPitch + x * 4;
        return p[3];
    };

    // Center at (120, 120) should be fully opaque
    EXPECT_GE(get_pixel_alpha(120, 120), 250);

    // Cardinal tangent boundaries at x=100, x=140, y=100, y=140:
    // Pixel centers (100.5, 120.5), (140.5, 120.5), (120.5, 100.5), (120.5, 140.5)
    // lie precisely on dist = 0, so alpha should be ~128 (0.5 coverage)
    EXPECT_NEAR(get_pixel_alpha(100, 120), 128, 10);
    EXPECT_NEAR(get_pixel_alpha(140, 120), 128, 10);
    EXPECT_NEAR(get_pixel_alpha(120, 100), 128, 10);
    EXPECT_NEAR(get_pixel_alpha(120, 140), 128, 10);

    // Diagonal boundaries at 45 degrees:
    // Center is (120.5, 120.5). At offset (14, 14):
    // Distance from center: sqrt(14^2 + 14^2) = sqrt(392) = 19.799
    // dist = 19.799 - 20 = -0.201 -> alpha = saturate(0.5 - (-0.201)) = ~0.70 (178)
    EXPECT_GT(get_pixel_alpha(120 + 14, 120 + 14), 150);
    EXPECT_LT(get_pixel_alpha(120 + 14, 120 + 14), 220);

    // Outside the circle (e.g. x=95) should be completely 0
    EXPECT_EQ(get_pixel_alpha(95, 120), 0);

    ctx->Unmap(staging.Get(), 0);

    // Test bordered circular ring (transparent fill, solid border)
    device->begin_frame();
    device->clear(core::Color{0.0f, 0.0f, 0.0f, 0.0f});
    device->set_viewport(800.0f, 600.0f);
    device->set_constants(0, const_span);

    DrawQuadCmd ring_cmd{.bounds = core::Rect{200.5f, 200.5f, 40.0f, 40.0f},
                         .radii = core::CornerRadii::uniform(20.0f),
                         .fill_color = core::Color{0.0f, 0.0f, 0.0f, 0.0f},
                         .border_color = core::Color{1.0f, 1.0f, 1.0f, 1.0f},
                         .border_width = core::Edges{4.0f, 4.0f, 4.0f, 4.0f}};
    batcher.add_quad(ring_cmd, core::Rect{0.0f, 0.0f, 800.0f, 600.0f});
    batcher.flush(*device);
    device->end_frame();

    ctx->CopyResource(staging.Get(), rt_res.Get());
    hr = ctx->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    ASSERT_EQ(hr, S_OK);

    // Center of ring (220, 220) must be completely transparent (alpha = 0)
    EXPECT_EQ(get_pixel_alpha(220, 220), 0);

    // Solid ring region at x=202 (distance 18.5 from center (220.5, 220.5), inside the 4px border
    // [16, 20])
    EXPECT_GE(get_pixel_alpha(202, 220), 250);
    EXPECT_GE(get_pixel_alpha(238, 220), 250);

    // Outer anti-aliasing edge at x=200
    EXPECT_NEAR(get_pixel_alpha(200, 220), 128, 10);
    EXPECT_NEAR(get_pixel_alpha(240, 220), 128, 10);

    // Inner anti-aliasing edge at x=204 (distance 16.5 from center)
    EXPECT_NEAR(get_pixel_alpha(204, 220), 128, 10);
    EXPECT_NEAR(get_pixel_alpha(236, 220), 128, 10);

    ctx->Unmap(staging.Get(), 0);
    batcher.shutdown(*device);
}

} // namespace north::render::testing
