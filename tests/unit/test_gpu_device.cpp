#include "north/gpu/d3d11/d3d11_device.hpp"
#include "north/gpu/d3d11/shaders/glyph_ps.h"
#include "north/gpu/d3d11/shaders/glyph_vs.h"
#include "north/gpu/d3d11/shaders/path_ps.h"
#include "north/gpu/d3d11/shaders/path_vs.h"
#include "north/gpu/d3d11/shaders/quad_ps.h"
#include "north/gpu/d3d11/shaders/quad_vs.h"

#include <gtest/gtest.h>

#include <vector>

namespace north::gpu::testing {

class GPUDeviceTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        device = std::make_unique<D3D11Device>();
        initialized = device->init_offscreen(800, 600);
    }

    void TearDown() override { device.reset(); }

    std::unique_ptr<D3D11Device> device;
    bool initialized{false};
};

TEST_F(GPUDeviceTestFixture, InitializesOffscreenDeviceCleanly) {
    ASSERT_TRUE(initialized);
    EXPECT_NE(device->raw_device(), nullptr);
    EXPECT_NE(device->raw_context(), nullptr);
}

TEST_F(GPUDeviceTestFixture, CreateUpdateAndDestroyTexture) {
    ASSERT_TRUE(initialized);

    TextureDesc desc{
        .width = 128, .height = 128, .format = TextureFormat::RGBA8, .render_target = false};

    auto tex_id = device->create_texture(desc);
    ASSERT_TRUE(tex_id.has_value());
    EXPECT_NE(*tex_id, kInvalidTexture);

    // Update 32x32 pixel subregion
    std::vector<uint8_t> pixels(32 * 32 * 4, 0xFF);
    core::Rect region{10.0f, 10.0f, 32.0f, 32.0f};
    device->update_texture(*tex_id, region, pixels);

    device->destroy_texture(*tex_id);
}

TEST_F(GPUDeviceTestFixture, CreateUpdateAndDestroyBuffer) {
    ASSERT_TRUE(initialized);

    BufferDesc desc{.size = 1024, .usage = BufferUsage::Vertex, .dynamic = true};

    auto buf_id = device->create_buffer(desc);
    ASSERT_TRUE(buf_id.has_value());
    EXPECT_NE(*buf_id, kInvalidBuffer);

    std::vector<uint8_t> data(256, 0xAB);
    device->update_buffer(*buf_id, 0, data);

    device->destroy_buffer(*buf_id);
}

TEST_F(GPUDeviceTestFixture, CreatePipelineWithEmbeddedShaders) {
    ASSERT_TRUE(initialized);

    std::span<const uint8_t> vs_bytes(g_quad_vs, sizeof(g_quad_vs));
    std::span<const uint8_t> ps_bytes(g_quad_ps, sizeof(g_quad_ps));

    auto pipe_id = device->create_pipeline(vs_bytes, ps_bytes, BlendMode::PremultipliedAlpha);
    ASSERT_TRUE(pipe_id.has_value());
    EXPECT_NE(*pipe_id, kInvalidPipeline);

    device->destroy_pipeline(*pipe_id);

    std::span<const uint8_t> gvs_bytes(g_glyph_vs, sizeof(g_glyph_vs));
    std::span<const uint8_t> gps_bytes(g_glyph_ps, sizeof(g_glyph_ps));
    auto gpipe_id = device->create_pipeline(gvs_bytes, gps_bytes, BlendMode::PremultipliedAlpha,
                                            VertexLayout::Glyph);
    ASSERT_TRUE(gpipe_id.has_value());
    EXPECT_NE(*gpipe_id, kInvalidPipeline);
    device->destroy_pipeline(*gpipe_id);
}

TEST_F(GPUDeviceTestFixture, ExecutesOffscreenRenderPass) {
    ASSERT_TRUE(initialized);

    // Create quad pipeline
    std::span<const uint8_t> vs_bytes(g_quad_vs, sizeof(g_quad_vs));
    std::span<const uint8_t> ps_bytes(g_quad_ps, sizeof(g_quad_ps));
    auto pipe_id = device->create_pipeline(vs_bytes, ps_bytes, BlendMode::PremultipliedAlpha);
    ASSERT_TRUE(pipe_id.has_value());

    // Create instance buffer for 2 quads
    BufferDesc ib_desc{
        .size = sizeof(QuadInstance) * 2, .usage = BufferUsage::Vertex, .dynamic = true};
    auto ib_id = device->create_buffer(ib_desc);
    ASSERT_TRUE(ib_id.has_value());

    QuadInstance instances[2]{};
    // Quad 1: Rounded rect with border
    instances[0].rect = core::Rect{50.0f, 50.0f, 200.0f, 100.0f};
    instances[0].radii = core::CornerRadii::uniform(12.0f);
    instances[0].fill_color = core::Color::from_rgba8(30, 120, 220, 255);
    instances[0].border_color = core::Color::from_rgba8(255, 255, 255, 200);
    instances[0].border_width = core::Edges::uniform(2.0f);
    instances[0].clip_rect = core::Rect{0.0f, 0.0f, 800.0f, 600.0f};

    // Quad 2: Shadow box
    instances[1].rect = core::Rect{300.0f, 50.0f, 150.0f, 150.0f};
    instances[1].radii = core::CornerRadii::uniform(8.0f);
    instances[1].fill_color = core::Color::from_rgba8(40, 40, 45, 255);
    instances[1].shadow_color = core::Color::from_rgba8(0, 0, 0, 150);
    instances[1].shadow_radius = 16.0f;
    instances[1].shadow_offset_y = 6.0f;
    instances[1].clip_rect = core::Rect{0.0f, 0.0f, 800.0f, 600.0f};

    std::span<const uint8_t> inst_bytes(reinterpret_cast<const uint8_t*>(instances),
                                        sizeof(instances));
    device->update_buffer(*ib_id, 0, inst_bytes);

    ViewConstants constants{.viewport_width = 800.0f, .viewport_height = 600.0f};
    std::span<const uint8_t> const_bytes(reinterpret_cast<const uint8_t*>(&constants),
                                         sizeof(constants));

    // Execute Frame Cycle
    device->begin_frame();
    device->clear(core::Color::from_rgba8(20, 20, 25, 255));
    device->set_viewport(800.0f, 600.0f);
    device->set_pipeline(*pipe_id);
    device->set_constants(0, const_bytes);
    device->set_instance_buffer(*ib_id, sizeof(QuadInstance));
    device->draw_instanced(4, 2);
    device->end_frame();

    device->destroy_buffer(*ib_id);
    device->destroy_pipeline(*pipe_id);
}

TEST_F(GPUDeviceTestFixture, ExecutesIndexedPathRenderPass) {
    ASSERT_TRUE(initialized);

    std::span<const uint8_t> vs_bytes(g_path_vs, sizeof(g_path_vs));
    std::span<const uint8_t> ps_bytes(g_path_ps, sizeof(g_path_ps));
    auto pipe_id = device->create_pipeline(vs_bytes, ps_bytes, BlendMode::PremultipliedAlpha,
                                           VertexLayout::Path);
    ASSERT_TRUE(pipe_id.has_value());

    // 3 vertices for a triangle
    PathVertex vertices[3]{};
    vertices[0].position = core::Point{50.0f, 50.0f};
    vertices[0].aa_coverage = 1.0f;
    vertices[0].color = core::Color{1.0f, 0.0f, 0.0f, 1.0f};
    vertices[0].clip_rect = core::Rect{0.0f, 0.0f, 800.0f, 600.0f};

    vertices[1].position = core::Point{150.0f, 50.0f};
    vertices[1].aa_coverage = 1.0f;
    vertices[1].color = core::Color{0.0f, 1.0f, 0.0f, 1.0f};
    vertices[1].clip_rect = core::Rect{0.0f, 0.0f, 800.0f, 600.0f};

    vertices[2].position = core::Point{100.0f, 150.0f};
    vertices[2].aa_coverage = 1.0f;
    vertices[2].color = core::Color{0.0f, 0.0f, 1.0f, 1.0f};
    vertices[2].clip_rect = core::Rect{0.0f, 0.0f, 800.0f, 600.0f};

    BufferDesc vb_desc{
        .size = sizeof(vertices), .usage = BufferUsage::Vertex, .dynamic = true};
    auto vb_id = device->create_buffer(vb_desc);
    ASSERT_TRUE(vb_id.has_value());
    device->update_buffer(*vb_id, 0,
                          std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(vertices),
                                                   sizeof(vertices)));

    uint16_t indices[3] = {0, 1, 2};
    BufferDesc ib_desc{
        .size = sizeof(indices), .usage = BufferUsage::Index, .dynamic = true};
    auto ib_id = device->create_buffer(ib_desc);
    ASSERT_TRUE(ib_id.has_value());
    device->update_buffer(*ib_id, 0,
                          std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(indices),
                                                   sizeof(indices)));

    ViewConstants constants{.viewport_width = 800.0f, .viewport_height = 600.0f};
    device->begin_frame();
    device->clear(core::Color{0.1f, 0.1f, 0.1f, 1.0f});
    device->set_viewport(800.0f, 600.0f);
    device->set_pipeline(*pipe_id);
    device->set_constants(0, std::span<const uint8_t>(
                                 reinterpret_cast<const uint8_t*>(&constants), sizeof(constants)));
    device->set_vertex_buffer(*vb_id, sizeof(PathVertex));
    device->set_index_buffer(*ib_id, IndexFormat::UInt16);
    device->draw_indexed(3);
    device->end_frame();

    device->destroy_buffer(*ib_id);
    device->destroy_buffer(*vb_id);
    device->destroy_pipeline(*pipe_id);
}

} // namespace north::gpu::testing
