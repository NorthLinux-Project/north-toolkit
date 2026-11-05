#pragma once

#include "north/core/types.hpp"

#include <algorithm>
#include <cstdint>

namespace north::image {

enum class ContentScale : uint8_t {
    Fit,       // Preserve aspect ratio; fit entire image within destination bounds (letterbox/pillarbox)
    Cover,     // Preserve aspect ratio; fill destination bounds, center-cropping excess
    Fill,      // Stretch image to fill destination bounds, ignoring aspect ratio
    None,      // Keep original image size
    ScaleDown  // Scale down like Fit if image is larger than destination; otherwise keep original size
};

enum class ImageAlignment : uint8_t {
    Center,
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

struct ScaleResult {
    core::Rect draw_rect;
    core::Rect uv_rect;
};

[[nodiscard]] inline ScaleResult solve_content_scale(core::Size source_sz, core::Size dest_sz,
                                                     ContentScale scale,
                                                     ImageAlignment align = ImageAlignment::Center) noexcept {
    if (source_sz.width <= 0.0f || source_sz.height <= 0.0f ||
        dest_sz.width <= 0.0f || dest_sz.height <= 0.0f) {
        return {core::Rect{}, core::Rect{0.0f, 0.0f, 1.0f, 1.0f}};
    }

    auto align_offset = [](float avail, float content, ImageAlignment a, bool is_x) -> float {
        float factor = 0.5f;
        if (is_x) {
            if (a == ImageAlignment::TopLeft || a == ImageAlignment::CenterLeft || a == ImageAlignment::BottomLeft) {
                factor = 0.0f;
            } else if (a == ImageAlignment::TopRight || a == ImageAlignment::CenterRight || a == ImageAlignment::BottomRight) {
                factor = 1.0f;
            }
        } else {
            if (a == ImageAlignment::TopLeft || a == ImageAlignment::TopCenter || a == ImageAlignment::TopRight) {
                factor = 0.0f;
            } else if (a == ImageAlignment::BottomLeft || a == ImageAlignment::BottomCenter || a == ImageAlignment::BottomRight) {
                factor = 1.0f;
            }
        }
        return (avail - content) * factor;
    };

    if (scale == ContentScale::Fill) {
        return {core::Rect{0.0f, 0.0f, dest_sz.width, dest_sz.height},
                core::Rect{0.0f, 0.0f, 1.0f, 1.0f}};
    }

    if (scale == ContentScale::Cover) {
        float sx = dest_sz.width / source_sz.width;
        float sy = dest_sz.height / source_sz.height;
        float s = (std::max)(sx, sy);

        float visible_w = dest_sz.width / s;
        float visible_h = dest_sz.height / s;

        float crop_x = align_offset(source_sz.width, visible_w, align, true);
        float crop_y = align_offset(source_sz.height, visible_h, align, false);

        float u0 = crop_x / source_sz.width;
        float v0 = crop_y / source_sz.height;
        float u1 = u0 + visible_w / source_sz.width;
        float v1 = v0 + visible_h / source_sz.height;

        return {core::Rect{0.0f, 0.0f, dest_sz.width, dest_sz.height},
                core::Rect{u0, v0, u1, v1}};
    }

    float eff_scale = 1.0f;
    if (scale == ContentScale::Fit) {
        float sx = dest_sz.width / source_sz.width;
        float sy = dest_sz.height / source_sz.height;
        eff_scale = (std::min)(sx, sy);
    } else if (scale == ContentScale::ScaleDown) {
        float sx = dest_sz.width / source_sz.width;
        float sy = dest_sz.height / source_sz.height;
        float fit_s = (std::min)(sx, sy);
        eff_scale = (std::min)(1.0f, fit_s);
    }

    float draw_w = source_sz.width * eff_scale;
    float draw_h = source_sz.height * eff_scale;

    float draw_x = align_offset(dest_sz.width, draw_w, align, true);
    float draw_y = align_offset(dest_sz.height, draw_h, align, false);

    return {core::Rect{draw_x, draw_y, draw_w, draw_h}, core::Rect{0.0f, 0.0f, 1.0f, 1.0f}};
}

} // namespace north::image
