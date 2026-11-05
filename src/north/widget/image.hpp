#pragma once

#include "north/core/tree.hpp"
#include "north/gpu/device.hpp"
#include "north/image/image_cache.hpp"
#include "north/image/image_types.hpp"
#include "north/widget/widget_data.hpp"

#include <span>
#include <string_view>

namespace north::widget {

class ImageBuilder {
public:
    explicit ImageBuilder(core::NodeTree& tree);

    ImageBuilder& texture(gpu::TextureId texture, core::Size natural_size);
    ImageBuilder& load_file(gpu::Device& device, std::string_view file_path);
    ImageBuilder& load_memory(gpu::Device& device, std::string_view key,
                              std::span<const uint8_t> bytes);
    ImageBuilder& load_pixels(gpu::Device& device, std::string_view key,
                              uint32_t width, uint32_t height,
                              std::span<const uint8_t> rgba_pixels);

    ImageBuilder& content_scale(image::ContentScale scale);
    ImageBuilder& alignment(image::ImageAlignment align);
    ImageBuilder& corner_radii(core::CornerRadii radii);
    ImageBuilder& tint(core::Color tint);
    ImageBuilder& opacity(float alpha);
    ImageBuilder& border(core::Color color, core::Edges width);
    ImageBuilder& border(core::Color color, float uniform_width);

    ImageBuilder& width(float w);
    ImageBuilder& height(float h);
    ImageBuilder& size(core::Size sz);
    ImageBuilder& size(float w, float h);
    ImageBuilder& aspect_ratio(float ratio);

    ImageBuilder& theme_signal(core::Signal<style::Theme>* sig, ImageThemeFn on_theme = nullptr);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    ImageData data_;
};

} // namespace north::widget
