#include "north/core/arena.hpp"
#include "north/core/tree.hpp"
#include "north/gpu/d3d11/d3d11_device.hpp"
#include "north/image/image_cache.hpp"
#include "north/image/image_decoder.hpp"
#include "north/image/image_types.hpp"
#include "north/render/display_list.hpp"
#include "north/render/image_batcher.hpp"
#include "north/widget/image.hpp"

#include <gtest/gtest.h>

namespace north::image::testing {

TEST(ContentScaleSolverTest, FitPreservesAspectRatioLetterbox) {
    core::Size src{100.0f, 50.0f};  // 2:1 ratio
    core::Size dest{200.0f, 200.0f}; // Square dest

    auto res = solve_content_scale(src, dest, ContentScale::Fit, ImageAlignment::Center);

    // Should scale by 2x: width = 200, height = 100
    // Centered vertically: y = (200 - 100) * 0.5 = 50
    EXPECT_FLOAT_EQ(res.draw_rect.width, 200.0f);
    EXPECT_FLOAT_EQ(res.draw_rect.height, 100.0f);
    EXPECT_FLOAT_EQ(res.draw_rect.x, 0.0f);
    EXPECT_FLOAT_EQ(res.draw_rect.y, 50.0f);

    EXPECT_FLOAT_EQ(res.uv_rect.x, 0.0f);
    EXPECT_FLOAT_EQ(res.uv_rect.y, 0.0f);
    EXPECT_FLOAT_EQ(res.uv_rect.width, 1.0f);
    EXPECT_FLOAT_EQ(res.uv_rect.height, 1.0f);
}

TEST(ContentScaleSolverTest, FitPreservesAspectRatioPillarbox) {
    core::Size src{50.0f, 100.0f};  // 1:2 ratio
    core::Size dest{200.0f, 200.0f}; // Square dest

    auto res = solve_content_scale(src, dest, ContentScale::Fit, ImageAlignment::Center);

    // Should scale by 2x: width = 100, height = 200
    // Centered horizontally: x = (200 - 100) * 0.5 = 50
    EXPECT_FLOAT_EQ(res.draw_rect.width, 100.0f);
    EXPECT_FLOAT_EQ(res.draw_rect.height, 200.0f);
    EXPECT_FLOAT_EQ(res.draw_rect.x, 50.0f);
    EXPECT_FLOAT_EQ(res.draw_rect.y, 0.0f);
}

TEST(ContentScaleSolverTest, CoverFillsAndCropsExcess) {
    core::Size src{100.0f, 50.0f};  // 2:1 ratio
    core::Size dest{100.0f, 100.0f}; // Square dest

    auto res = solve_content_scale(src, dest, ContentScale::Cover, ImageAlignment::Center);

    // Draw rect fills dest exactly
    EXPECT_FLOAT_EQ(res.draw_rect.width, 100.0f);
    EXPECT_FLOAT_EQ(res.draw_rect.height, 100.0f);
    EXPECT_FLOAT_EQ(res.draw_rect.x, 0.0f);
    EXPECT_FLOAT_EQ(res.draw_rect.y, 0.0f);

    // Scale is 100/50 = 2.0. Visible width = 100/2 = 50.
    // Crop in X: u0 = (100 - 50)*0.5 / 100 = 0.25, u1 = 0.75
    EXPECT_FLOAT_EQ(res.uv_rect.x, 0.25f);
    EXPECT_FLOAT_EQ(res.uv_rect.y, 0.0f);
    EXPECT_FLOAT_EQ(res.uv_rect.width, 0.75f);
    EXPECT_FLOAT_EQ(res.uv_rect.height, 1.0f);
}

TEST(ContentScaleSolverTest, FillStretchesToDestination) {
    core::Size src{100.0f, 50.0f};
    core::Size dest{300.0f, 400.0f};

    auto res = solve_content_scale(src, dest, ContentScale::Fill);

    EXPECT_FLOAT_EQ(res.draw_rect.width, 300.0f);
    EXPECT_FLOAT_EQ(res.draw_rect.height, 400.0f);
    EXPECT_FLOAT_EQ(res.draw_rect.x, 0.0f);
    EXPECT_FLOAT_EQ(res.draw_rect.y, 0.0f);
    EXPECT_FLOAT_EQ(res.uv_rect.x, 0.0f);
    EXPECT_FLOAT_EQ(res.uv_rect.y, 0.0f);
    EXPECT_FLOAT_EQ(res.uv_rect.width, 1.0f);
    EXPECT_FLOAT_EQ(res.uv_rect.height, 1.0f);
}

TEST(ContentScaleSolverTest, NonePreservesOriginalDimensions) {
    core::Size src{80.0f, 60.0f};
    core::Size dest{200.0f, 200.0f};

    auto res = solve_content_scale(src, dest, ContentScale::None, ImageAlignment::Center);

    EXPECT_FLOAT_EQ(res.draw_rect.width, 80.0f);
    EXPECT_FLOAT_EQ(res.draw_rect.height, 60.0f);
    EXPECT_FLOAT_EQ(res.draw_rect.x, 60.0f);
    EXPECT_FLOAT_EQ(res.draw_rect.y, 70.0f);
}

TEST(ContentScaleSolverTest, ScaleDownOnlyShrinksIfLarger) {
    core::Size small_src{50.0f, 50.0f};
    core::Size large_dest{200.0f, 200.0f};

    auto res1 = solve_content_scale(small_src, large_dest, ContentScale::ScaleDown, ImageAlignment::Center);
    // Should NOT upscale: stays 50x50 centered
    EXPECT_FLOAT_EQ(res1.draw_rect.width, 50.0f);
    EXPECT_FLOAT_EQ(res1.draw_rect.height, 50.0f);

    core::Size large_src{400.0f, 400.0f};
    core::Size small_dest{100.0f, 100.0f};

    auto res2 = solve_content_scale(large_src, small_dest, ContentScale::ScaleDown, ImageAlignment::Center);
    // SHOULD scale down to 100x100
    EXPECT_FLOAT_EQ(res2.draw_rect.width, 100.0f);
    EXPECT_FLOAT_EQ(res2.draw_rect.height, 100.0f);
}

TEST(ImageDecoderTest, DecodesValidBmpStream) {
    // 2x2 32-bit BMP image stream in memory
    const uint8_t bmp_data[70] = {
        'B', 'M',
        70, 0, 0, 0,      // File size: 70 bytes
        0, 0, 0, 0,       // Reserved
        54, 0, 0, 0,      // Offset to pixel array: 54
        40, 0, 0, 0,      // DIB header size: 40
        2, 0, 0, 0,       // Width: 2
        2, 0, 0, 0,       // Height: 2
        1, 0,             // Color planes: 1
        32, 0,            // Bits per pixel: 32
        0, 0, 0, 0,       // Compression: BI_RGB
        16, 0, 0, 0,      // Image size: 16 bytes
        0, 0, 0, 0,       // X ppm
        0, 0, 0, 0,       // Y ppm
        0, 0, 0, 0,       // Colors used
        0, 0, 0, 0,       // Important colors
        // Row 1 (bottom): Red (BGRA), Green (BGRA)
        0, 0, 255, 255,   0, 255, 0, 255,
        // Row 2 (top): Blue (BGRA), White (BGRA)
        255, 0, 0, 255,   255, 255, 255, 255
    };

    auto decoded = decode_image_memory(std::span<const uint8_t>(bmp_data, sizeof(bmp_data)));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->width, 2u);
    EXPECT_EQ(decoded->height, 2u);
    EXPECT_EQ(decoded->pixels.size(), 16u); // 2 * 2 * 4 bytes
}

TEST(ImageDecoderTest, InvalidStreamFailsGracefully) {
    const uint8_t corrupt_data[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    auto decoded = decode_image_memory(std::span<const uint8_t>(corrupt_data, sizeof(corrupt_data)));
    EXPECT_FALSE(decoded.has_value());

    auto non_existent = decode_image_file("non_existent_file_path.png");
    EXPECT_FALSE(non_existent.has_value());
}

TEST(ImageCacheTest, CachesAndDeduplicatesTextures) {
    gpu::D3D11Device device;
    ASSERT_TRUE(device.init_offscreen(400, 300));

    uint8_t rgba[16] = {
        255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 255, 255,   255, 255, 255, 255
    };

    auto& cache = ImageCache::instance();
    auto tex1 = cache.load_from_pixels(device, "test_pixels_key", 2, 2, rgba);
    ASSERT_TRUE(tex1.has_value());
    EXPECT_NE(tex1->texture, gpu::kInvalidTexture);
    EXPECT_EQ(tex1->width, 2u);
    EXPECT_EQ(tex1->height, 2u);

    // Querying with same key returns cached texture
    auto tex2 = cache.find("test_pixels_key");
    ASSERT_TRUE(tex2.has_value());
    EXPECT_EQ(tex1->texture, tex2->texture);

    cache.clear(device);
    EXPECT_FALSE(cache.find("test_pixels_key").has_value());
}

TEST(ImageWidgetTest, BuildsLaysOutAndPaintsToDisplayList) {
    core::NodeTree tree;
    core::Arena arena(1024 * 64);

    auto img_node = widget::ImageBuilder(tree)
                        .texture(42, core::Size{100.0f, 50.0f})
                        .content_scale(ContentScale::Fit)
                        .alignment(ImageAlignment::Center)
                        .corner_radii(core::CornerRadii::uniform(8.0f))
                        .tint(core::Color{1.0f, 1.0f, 1.0f, 0.9f})
                        .border(core::Color{0.2f, 0.4f, 0.8f, 1.0f}, 2.0f)
                        .size(200.0f, 200.0f)
                        .build();

    tree.set_root(img_node);
    tree.layout(layout::BoxConstraints::loose({800.0f, 600.0f}));

    auto* node = tree.get(img_node);
    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->layout.size.width, 200.0f);
    EXPECT_FLOAT_EQ(node->layout.size.height, 200.0f);

    // Hit test
    ASSERT_NE(node->hit_test_fn, nullptr);
    EXPECT_TRUE(node->hit_test_fn(tree, img_node, {100.0f, 100.0f}, node->user_data));
    EXPECT_FALSE(node->hit_test_fn(tree, img_node, {-10.0f, 50.0f}, node->user_data));

    // Paint to display list
    render::DisplayList dl(arena, core::Rect{0.0f, 0.0f, 800.0f, 600.0f});
    tree.paint(&dl);

    ASSERT_EQ(dl.size(), 1u);
    const auto& cmd = dl.commands()[0];
    EXPECT_EQ(cmd.type, render::CommandType::Image);
    EXPECT_EQ(cmd.image.texture, 42u);
    EXPECT_FLOAT_EQ(cmd.image.bounds.width, 200.0f);
    EXPECT_FLOAT_EQ(cmd.image.bounds.height, 100.0f);
    EXPECT_FLOAT_EQ(cmd.image.bounds.y, 50.0f); // Letterboxed
    EXPECT_FLOAT_EQ(cmd.image.radii.top_left, 8.0f);
    EXPECT_FLOAT_EQ(cmd.image.tint.a, 0.9f);
    EXPECT_FLOAT_EQ(cmd.image.border_width.top, 2.0f);
}

TEST(ImageBatcherTest, BatchesAndFlushesImageCommands) {
    gpu::D3D11Device device;
    ASSERT_TRUE(device.init_offscreen(400, 300));

    uint8_t rgba[16] = {255, 255, 255, 255, 255, 255, 255, 255,
                        255, 255, 255, 255, 255, 255, 255, 255};
    gpu::TextureDesc desc{.width = 2, .height = 2, .format = gpu::TextureFormat::RGBA8};
    auto tex = device.create_texture(desc);
    ASSERT_TRUE(tex.has_value());
    device.update_texture(*tex, {0, 0, 2, 2}, rgba);

    render::ImageBatcher batcher;
    ASSERT_TRUE(batcher.init(device));

    batcher.begin_batch();
    render::DrawImageCmd cmd{
        .texture = *tex,
        .bounds = {10.0f, 10.0f, 100.0f, 100.0f},
        .uv_rect = {0.0f, 0.0f, 1.0f, 1.0f},
        .radii = core::CornerRadii::uniform(4.0f),
        .tint = {1.0f, 1.0f, 1.0f, 1.0f},
        .border_color = {0.0f, 0.0f, 0.0f, 0.0f},
        .border_width = {},
    };

    batcher.add_image(cmd, core::Rect{0.0f, 0.0f, 400.0f, 300.0f}, device);
    EXPECT_EQ(batcher.instance_count(), 1u);
    EXPECT_TRUE(batcher.has_pending());

    device.begin_frame();
    batcher.flush(device);
    EXPECT_EQ(batcher.instance_count(), 0u);
    EXPECT_FALSE(batcher.has_pending());
    device.end_frame();

    batcher.shutdown(device);
    device.destroy_texture(*tex);
}

} // namespace north::image::testing
