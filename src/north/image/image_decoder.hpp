#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace north::image {

struct DecodedImage {
    uint32_t width{0};
    uint32_t height{0};
    std::vector<uint8_t> pixels; // 32-bit RGBA (4 bytes per pixel)
};

[[nodiscard]] std::optional<DecodedImage> decode_image_file(std::string_view file_path);
[[nodiscard]] std::optional<DecodedImage> decode_image_memory(std::span<const uint8_t> bytes);

} // namespace north::image
