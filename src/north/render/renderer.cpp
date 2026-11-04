#include "north/render/renderer.hpp"

namespace north::render {

void default_box_paint(core::NodeTree& tree, core::Node::HandleType handle, core::Point origin,
                       void* stream, void*) {
    auto* dl = static_cast<DisplayList*>(stream);
    if (!dl) {
        return;
    }

    const auto* node = tree.get(handle);
    if (!node) {
        return;
    }

    core::Rect bounds{origin.x, origin.y, node->layout.size.width, node->layout.size.height};
    const auto& paint = node->paint;

    if (paint.shadow_color.a > 0.0f && paint.shadow_radius > 0.0f) {
        dl->push_shadow(bounds, paint.radii, paint.shadow_color, paint.shadow_radius);
    }

    if (paint.background.a > 0.0f ||
        (paint.border_color.a > 0.0f &&
         (paint.border_width.top > 0.0f || paint.border_width.right > 0.0f ||
          paint.border_width.bottom > 0.0f || paint.border_width.left > 0.0f))) {
        dl->push_quad(bounds, paint.radii, paint.background, paint.border_color,
                      paint.border_width);
    }
}

Renderer::Renderer(gpu::Device& device) : device_(device), glyph_atlas_(device) {}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::init() {
    if (initialized_) {
        return true;
    }

    if (!quad_batcher_.init(device_)) {
        return false;
    }

    if (!glyph_batcher_.init(device_)) {
        quad_batcher_.shutdown(device_);
        return false;
    }

    initialized_ = true;
    return true;
}

void Renderer::shutdown() {
    if (initialized_) {
        quad_batcher_.shutdown(device_);
        glyph_batcher_.shutdown(device_);
        glyph_atlas_.clear();
        initialized_ = false;
    }
}

void Renderer::render_frame(core::NodeTree& tree, core::Size viewport_size,
                            core::Color clear_color) {
    frame_arena_.reset();

    if (!tree.default_paint()) {
        tree.set_default_paint(&default_box_paint);
    }

    device_.begin_frame();
    device_.clear(clear_color);
    device_.set_viewport(viewport_size.width, viewport_size.height);

    gpu::ViewConstants view_constants{.viewport_width = viewport_size.width,
                                      .viewport_height = viewport_size.height,
                                      .pad = {0.0f, 0.0f}};
    const auto* const_bytes = reinterpret_cast<const uint8_t*>(&view_constants);
    std::span<const uint8_t> const_span(const_bytes, sizeof(view_constants));
    device_.set_constants(0, const_span);

    core::Rect viewport_rect{0.0f, 0.0f, viewport_size.width, viewport_size.height};
    DisplayList dl(frame_arena_, viewport_rect);
    dl.set_glyph_atlas(&glyph_atlas_);

    tree.paint(&dl);

    quad_batcher_.begin_batch();
    glyph_batcher_.begin_batch();

    for (const auto& cmd : dl.commands()) {
        switch (cmd.type) {
        case CommandType::Quad:
            if (glyph_batcher_.has_pending()) {
                glyph_batcher_.flush(device_);
            }
            quad_batcher_.add_quad(cmd.quad, cmd.clip_rect);
            break;
        case CommandType::Shadow:
            if (glyph_batcher_.has_pending()) {
                glyph_batcher_.flush(device_);
            }
            quad_batcher_.add_shadow(cmd.shadow, cmd.clip_rect);
            break;
        case CommandType::GlyphRun:
            if (quad_batcher_.instance_count() > 0) {
                quad_batcher_.flush(device_);
            }
            glyph_batcher_.add_run(cmd.glyph_run.atlas_texture, cmd.glyph_run.glyphs, cmd.clip_rect,
                                   device_);
            break;
        }
    }

    if (quad_batcher_.instance_count() > 0) {
        quad_batcher_.flush(device_);
    }
    if (glyph_batcher_.has_pending()) {
        glyph_batcher_.flush(device_);
    }

    device_.end_frame();
}

} // namespace north::render
