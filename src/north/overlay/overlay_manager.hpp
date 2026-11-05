#pragma once

#include "north/core/tree.hpp"
#include "north/core/types.hpp"
#include "north/overlay/overlay_types.hpp"
#include "north/render/display_list.hpp"

#include <span>
#include <vector>

namespace north::overlay {

struct OverlayEntry {
    OverlayId id{kInvalidOverlayId};
    core::Handle<core::Node> content_node{};
    core::Handle<core::Node> anchor_node{};
    core::Rect explicit_anchor_rect{};
    bool has_explicit_anchor{false};
    OverlayConfig config{};
    core::Rect computed_bounds{};
    bool active{true};
};

class OverlayManager {
public:
    OverlayManager() = default;
    ~OverlayManager() = default;

    OverlayManager(const OverlayManager&) = delete;
    OverlayManager& operator=(const OverlayManager&) = delete;
    OverlayManager(OverlayManager&&) noexcept = default;
    OverlayManager& operator=(OverlayManager&&) noexcept = default;

    OverlayId show_overlay(core::Handle<core::Node> content, const OverlayConfig& config,
                           core::Handle<core::Node> anchor = {});

    OverlayId show_overlay_at_point(core::Handle<core::Node> content, core::Point point,
                                    const OverlayConfig& config);

    OverlayId show_overlay_at_rect(core::Handle<core::Node> content, core::Rect rect,
                                   const OverlayConfig& config);

    void hide_overlay(OverlayId id);
    void dismiss_top_modal();
    void dismiss_all();

    [[nodiscard]] bool has_active_overlays() const noexcept;
    [[nodiscard]] bool has_active_modal() const noexcept;
    [[nodiscard]] const OverlayEntry* top_modal() const noexcept;
    [[nodiscard]] const OverlayEntry* find_overlay(OverlayId id) const noexcept;
    [[nodiscard]] std::span<const OverlayEntry> entries() const noexcept { return entries_; }

    void layout_overlays(core::NodeTree& tree, core::Size viewport_size);
    void paint_overlays(core::NodeTree& tree, render::DisplayList& dl, core::Size viewport_size);

private:
    OverlayId next_id_{1};
    std::vector<OverlayEntry> entries_;
};

} // namespace north::overlay
