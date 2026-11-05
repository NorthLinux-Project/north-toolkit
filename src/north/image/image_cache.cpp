#include "north/image/image_cache.hpp"

#include "north/image/image_decoder.hpp"

namespace north::image {

ImageCache& ImageCache::instance() {
    static ImageCache instance;
    return instance;
}

std::optional<TextureInfo> ImageCache::find(std::string_view key) const {
    auto it = cache_.find(std::string(key));
    if (it != cache_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<TextureInfo> ImageCache::get_or_load(gpu::Device& device,
                                                   std::string_view file_path) {
    std::string key(file_path);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        return it->second;
    }

    auto decoded = decode_image_file(file_path);
    if (!decoded) {
        return std::nullopt;
    }

    return load_from_pixels(device, file_path, decoded->width, decoded->height, decoded->pixels);
}

std::optional<TextureInfo> ImageCache::load_from_memory(gpu::Device& device, std::string_view key,
                                                        std::span<const uint8_t> bytes) {
    std::string skey(key);
    auto it = cache_.find(skey);
    if (it != cache_.end()) {
        return it->second;
    }

    auto decoded = decode_image_memory(bytes);
    if (!decoded) {
        return std::nullopt;
    }

    return load_from_pixels(device, key, decoded->width, decoded->height, decoded->pixels);
}

std::optional<TextureInfo> ImageCache::load_from_pixels(gpu::Device& device, std::string_view key,
                                                        uint32_t width, uint32_t height,
                                                        std::span<const uint8_t> rgba_pixels) {
    if (width == 0 || height == 0 || rgba_pixels.empty()) {
        return std::nullopt;
    }

    gpu::TextureDesc desc{
        .width = width,
        .height = height,
        .format = gpu::TextureFormat::RGBA8,
        .render_target = false,
    };

    auto tex = device.create_texture(desc);
    if (!tex) {
        return std::nullopt;
    }

    core::Rect region{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
    device.update_texture(*tex, region, rgba_pixels);

    TextureInfo info{
        .texture = *tex,
        .width = width,
        .height = height,
    };

    cache_[std::string(key)] = info;
    return info;
}

void ImageCache::clear(gpu::Device& device) {
    for (auto& [key, info] : cache_) {
        if (info.texture != gpu::kInvalidTexture) {
            device.destroy_texture(info.texture);
        }
    }
    cache_.clear();
}

} // namespace north::image
