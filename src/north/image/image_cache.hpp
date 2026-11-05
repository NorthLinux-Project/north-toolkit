#pragma once

#include "north/gpu/device.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

namespace north::image {

struct TextureInfo {
    gpu::TextureId texture{gpu::kInvalidTexture};
    uint32_t width{0};
    uint32_t height{0};
};

class ImageCache {
public:
    static ImageCache& instance();

    std::optional<TextureInfo> get_or_load(gpu::Device& device, std::string_view file_path);
    std::optional<TextureInfo> load_from_memory(gpu::Device& device, std::string_view key,
                                                std::span<const uint8_t> bytes);
    std::optional<TextureInfo> load_from_pixels(gpu::Device& device, std::string_view key,
                                                uint32_t width, uint32_t height,
                                                std::span<const uint8_t> rgba_pixels);

    std::optional<TextureInfo> find(std::string_view key) const;
    void clear(gpu::Device& device);

private:
    std::unordered_map<std::string, TextureInfo> cache_;
};

} // namespace north::image
