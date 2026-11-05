#include "north/widget/image.hpp"

#include "north/render/display_list.hpp"

#include <algorithm>

namespace north::widget {

void ImageData::apply_theme(const style::Theme& theme) {
    if (on_theme) {
        on_theme(theme, *this);
    }
    float duration = theme.transition_duration();
    if (duration <= 0.0f) {
        animated_tint.set_immediate(tint);
    } else {
        animated_tint.tween_to(tint, duration, animation::CubicBezier::ease_out());
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

namespace {

core::Size image_layout_fn(core::NodeTree&, core::Handle<core::Node>,
                           const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<ImageData*>(user_data);
    if (!data) {
        return constraints.constrain({0.0f, 0.0f});
    }

    if (data->explicit_size.width >= 0.0f && data->explicit_size.height >= 0.0f) {
        return constraints.constrain(data->explicit_size);
    }

    float ratio = data->aspect_ratio;
    if (ratio <= 0.0f && data->natural_size.width > 0.0f && data->natural_size.height > 0.0f) {
        ratio = data->natural_size.width / data->natural_size.height;
    }

    if (data->explicit_size.width >= 0.0f) {
        float w = data->explicit_size.width;
        float h = (ratio > 0.0f) ? (w / ratio) : (data->natural_size.height > 0.0f ? data->natural_size.height : 0.0f);
        return constraints.constrain({w, h});
    }

    if (data->explicit_size.height >= 0.0f) {
        float h = data->explicit_size.height;
        float w = (ratio > 0.0f) ? (h * ratio) : (data->natural_size.width > 0.0f ? data->natural_size.width : 0.0f);
        return constraints.constrain({w, h});
    }

    if (data->natural_size.width > 0.0f && data->natural_size.height > 0.0f) {
        if (data->natural_size.width <= constraints.max_width &&
            data->natural_size.height <= constraints.max_height) {
            return constraints.constrain(data->natural_size);
        }
        float scale_x = constraints.max_width / data->natural_size.width;
        float scale_y = constraints.max_height / data->natural_size.height;
        float s = (std::min)(scale_x, scale_y);
        return constraints.constrain({data->natural_size.width * s, data->natural_size.height * s});
    }

    return constraints.constrain({0.0f, 0.0f});
}

void image_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                    void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    if (!dl) {
        return;
    }
    auto* data = reinterpret_cast<ImageData*>(user_data);
    if (!data || data->texture == gpu::kInvalidTexture) {
        return;
    }

    auto* node = tree.get(handle);
    if (!node) {
        return;
    }

    core::Size natural = data->natural_size;
    if (natural.width <= 0.0f || natural.height <= 0.0f) {
        natural = node->layout.size;
    }

    auto scale_res = image::solve_content_scale(natural, node->layout.size, data->content_scale,
                                                data->alignment);

    core::Rect draw_bounds{origin.x + scale_res.draw_rect.x, origin.y + scale_res.draw_rect.y,
                           scale_res.draw_rect.width, scale_res.draw_rect.height};

    core::Color tint = data->animated_tint.get();
    dl->push_image(data->texture, draw_bounds, data->corner_radii, scale_res.uv_rect, tint,
                   data->border_color, data->border_width);
}

bool image_hit_test_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                       core::Point local_point, void*) {
    auto* node = tree.get(handle);
    if (!node) {
        return false;
    }
    core::Rect local_bounds{0.0f, 0.0f, node->layout.size.width, node->layout.size.height};
    return local_bounds.contains(local_point);
}

void image_destroy_fn(void* user_data) {
    delete reinterpret_cast<ImageData*>(user_data);
}

} // namespace

ImageBuilder::ImageBuilder(core::NodeTree& tree) : tree_(tree) {}

ImageBuilder& ImageBuilder::texture(gpu::TextureId texture, core::Size natural_size) {
    data_.texture = texture;
    data_.natural_size = natural_size;
    return *this;
}

ImageBuilder& ImageBuilder::load_file(gpu::Device& device, std::string_view file_path) {
    auto info = image::ImageCache::instance().get_or_load(device, file_path);
    if (info) {
        data_.texture = info->texture;
        data_.natural_size = {static_cast<float>(info->width), static_cast<float>(info->height)};
    }
    return *this;
}

ImageBuilder& ImageBuilder::load_memory(gpu::Device& device, std::string_view key,
                                        std::span<const uint8_t> bytes) {
    auto info = image::ImageCache::instance().load_from_memory(device, key, bytes);
    if (info) {
        data_.texture = info->texture;
        data_.natural_size = {static_cast<float>(info->width), static_cast<float>(info->height)};
    }
    return *this;
}

ImageBuilder& ImageBuilder::load_pixels(gpu::Device& device, std::string_view key,
                                        uint32_t width, uint32_t height,
                                        std::span<const uint8_t> rgba_pixels) {
    auto info =
        image::ImageCache::instance().load_from_pixels(device, key, width, height, rgba_pixels);
    if (info) {
        data_.texture = info->texture;
        data_.natural_size = {static_cast<float>(info->width), static_cast<float>(info->height)};
    }
    return *this;
}

ImageBuilder& ImageBuilder::content_scale(image::ContentScale scale) {
    data_.content_scale = scale;
    return *this;
}

ImageBuilder& ImageBuilder::alignment(image::ImageAlignment align) {
    data_.alignment = align;
    return *this;
}

ImageBuilder& ImageBuilder::corner_radii(core::CornerRadii radii) {
    data_.corner_radii = radii;
    return *this;
}

ImageBuilder& ImageBuilder::tint(core::Color tint) {
    data_.tint = tint;
    return *this;
}

ImageBuilder& ImageBuilder::opacity(float alpha) {
    data_.tint.a = alpha;
    return *this;
}

ImageBuilder& ImageBuilder::border(core::Color color, core::Edges width) {
    data_.border_color = color;
    data_.border_width = width;
    return *this;
}

ImageBuilder& ImageBuilder::border(core::Color color, float uniform_width) {
    data_.border_color = color;
    data_.border_width = core::Edges::uniform(uniform_width);
    return *this;
}

ImageBuilder& ImageBuilder::width(float w) {
    data_.explicit_size.width = w;
    return *this;
}

ImageBuilder& ImageBuilder::height(float h) {
    data_.explicit_size.height = h;
    return *this;
}

ImageBuilder& ImageBuilder::size(core::Size sz) {
    data_.explicit_size = sz;
    return *this;
}

ImageBuilder& ImageBuilder::size(float w, float h) {
    data_.explicit_size = {w, h};
    return *this;
}

ImageBuilder& ImageBuilder::aspect_ratio(float ratio) {
    data_.aspect_ratio = ratio;
    return *this;
}

ImageBuilder& ImageBuilder::theme_signal(core::Signal<style::Theme>* sig, ImageThemeFn on_theme) {
    data_.theme_signal = sig;
    data_.on_theme = on_theme;
    return *this;
}

core::Handle<core::Node> ImageBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node) {
        return {};
    }

    auto* heap_data = new ImageData(std::move(data_));
    heap_data->node_handle = handle;
    heap_data->tree = &tree_;
    heap_data->animated_tint.set_immediate(heap_data->tint);

    heap_data->animated_tint.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* img = static_cast<ImageData*>(ctx);
            if (img->tree && img->node_handle.is_valid()) {
                img->tree->mark_dirty(img->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    if (heap_data->theme_signal) {
        heap_data->theme_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* img = static_cast<ImageData*>(ctx);
                if (img && img->theme_signal) {
                    img->apply_theme(img->theme_signal->get());
                }
            },
            heap_data, core::kPaint);
    }

    node->user_data = heap_data;
    node->layout_fn = &image_layout_fn;
    node->paint_fn = &image_paint_fn;
    node->hit_test_fn = &image_hit_test_fn;
    node->destroy_fn = &image_destroy_fn;

    return handle;
}

} // namespace north::widget
