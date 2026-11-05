#include "north/overlay/overlay_manager.hpp"
#include "north/overlay/positioning.hpp"

#include <algorithm>

namespace north::overlay {

OverlayId OverlayManager::show_overlay(core::Handle<core::Node> content,
                                       const OverlayConfig& config,
                                       core::Handle<core::Node> anchor) {
    OverlayEntry entry;
    entry.id = next_id_++;
    entry.content_node = content;
    entry.anchor_node = anchor;
    entry.has_explicit_anchor = false;
    entry.config = config;
    entry.active = true;

    entries_.push_back(std::move(entry));
    return entries_.back().id;
}

OverlayId OverlayManager::show_overlay_at_point(core::Handle<core::Node> content, core::Point point,
                                                const OverlayConfig& config) {
    return show_overlay_at_rect(content, core::Rect{point.x, point.y, 0.0f, 0.0f}, config);
}

OverlayId OverlayManager::show_overlay_at_rect(core::Handle<core::Node> content, core::Rect rect,
                                               const OverlayConfig& config) {
    OverlayEntry entry;
    entry.id = next_id_++;
    entry.content_node = content;
    entry.explicit_anchor_rect = rect;
    entry.has_explicit_anchor = true;
    entry.config = config;
    entry.active = true;

    entries_.push_back(std::move(entry));
    return entries_.back().id;
}

void OverlayManager::hide_overlay(OverlayId id) {
    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [id](const OverlayEntry& e) { return e.id == id; });
    if (it == entries_.end()) {
        return;
    }

    DismissCallbackFn on_dismiss = it->config.on_dismiss;
    void* dismiss_user_data = it->config.dismiss_user_data;

    entries_.erase(it);

    if (on_dismiss) {
        on_dismiss(dismiss_user_data);
    }
}

void OverlayManager::dismiss_top_modal() {
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (it->active && it->config.modal && it->config.barrier_dismissible) {
            OverlayId id = it->id;
            hide_overlay(id);
            return;
        }
    }
}

void OverlayManager::dismiss_all() {
    while (!entries_.empty()) {
        OverlayId id = entries_.back().id;
        hide_overlay(id);
    }
}

bool OverlayManager::has_active_overlays() const noexcept {
    return !entries_.empty();
}

bool OverlayManager::has_active_modal() const noexcept {
    for (const auto& e : entries_) {
        if (e.active && e.config.modal) {
            return true;
        }
    }
    return false;
}

const OverlayEntry* OverlayManager::top_modal() const noexcept {
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (it->active && it->config.modal) {
            return &(*it);
        }
    }
    return nullptr;
}

const OverlayEntry* OverlayManager::find_overlay(OverlayId id) const noexcept {
    for (const auto& e : entries_) {
        if (e.id == id) {
            return &e;
        }
    }
    return nullptr;
}

void OverlayManager::layout_overlays(core::NodeTree& tree, core::Size viewport_size) {
    for (auto& entry : entries_) {
        if (!entry.active || !entry.content_node.is_valid()) {
            continue;
        }

        core::Rect anchor_rect{};
        if (entry.has_explicit_anchor) {
            anchor_rect = entry.explicit_anchor_rect;
        } else if (entry.anchor_node.is_valid()) {
            const auto* an = tree.get(entry.anchor_node);
            if (an) {
                core::Point gpos = tree.global_offset(entry.anchor_node);
                anchor_rect =
                    core::Rect{gpos.x, gpos.y, an->layout.size.width, an->layout.size.height};
            }
        } else if (entry.config.modal) {
            anchor_rect = core::Rect{viewport_size.width * 0.5f, viewport_size.height * 0.5f,
                                     0.0f, 0.0f};
        }

        layout::BoxConstraints constraints{
            .min_width = 0.0f,
            .max_width = viewport_size.width,
            .min_height = 0.0f,
            .max_height = viewport_size.height,
        };
        core::Size content_sz = tree.layout_subtree(entry.content_node, constraints);

        core::Point pos{0.0f, 0.0f};
        if (!entry.has_explicit_anchor && !entry.anchor_node.is_valid() && entry.config.modal) {
            pos = core::Point{(viewport_size.width - content_sz.width) * 0.5f,
                              (viewport_size.height - content_sz.height) * 0.5f};
        } else {
            pos = solve_overlay_position(anchor_rect, content_sz, viewport_size,
                                         entry.config.placement, entry.config.offset,
                                         entry.config.flip_on_collision,
                                         entry.config.clamp_to_viewport);
        }

        entry.computed_bounds = core::Rect{pos.x, pos.y, content_sz.width, content_sz.height};
    }
}

void OverlayManager::paint_overlays(core::NodeTree& tree, render::DisplayList& dl,
                                    core::Size viewport_size) {
    for (const auto& entry : entries_) {
        if (!entry.active || !entry.content_node.is_valid()) {
            continue;
        }

        if (entry.config.modal && entry.config.barrier_color.a > 0.0f) {
            core::Rect barrier_rect{0.0f, 0.0f, viewport_size.width, viewport_size.height};
            dl.push_quad(barrier_rect, core::CornerRadii{}, entry.config.barrier_color);
        }

        tree.paint_subtree(entry.content_node,
                           core::Point{entry.computed_bounds.x, entry.computed_bounds.y}, &dl);
    }
}

} // namespace north::overlay
