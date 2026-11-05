#include "north/widget/popover.hpp"

#include "north/widget/box.hpp"

namespace north::widget {

PopoverBuilder::PopoverBuilder(core::NodeTree& tree, overlay::OverlayManager& mgr,
                               core::Handle<core::Node> content)
    : tree_(tree), mgr_(mgr), content_(content) {}

PopoverBuilder& PopoverBuilder::anchor(core::Handle<core::Node> a) {
    anchor_ = a;
    has_point_ = false;
    return *this;
}

PopoverBuilder& PopoverBuilder::point(core::Point pt) {
    point_ = pt;
    has_point_ = true;
    anchor_ = {};
    return *this;
}

PopoverBuilder& PopoverBuilder::placement(overlay::Placement p) {
    config_.placement = p;
    return *this;
}

PopoverBuilder& PopoverBuilder::offset(float off) {
    config_.offset = off;
    return *this;
}

PopoverBuilder& PopoverBuilder::modal(bool m) {
    config_.modal = m;
    return *this;
}

PopoverBuilder& PopoverBuilder::padding(core::Edges pad) {
    config_.padding = pad;
    return *this;
}

PopoverBuilder& PopoverBuilder::corner_radii(core::CornerRadii r) {
    config_.radii = r;
    return *this;
}

PopoverBuilder& PopoverBuilder::background(core::Color bg) {
    config_.background = bg;
    config_.custom_background = true;
    return *this;
}

PopoverBuilder& PopoverBuilder::border_color(core::Color bc) {
    config_.border_color = bc;
    config_.custom_border = true;
    return *this;
}

PopoverBuilder& PopoverBuilder::theme(const style::Theme& th) {
    config_.theme = th;
    return *this;
}

PopoverBuilder& PopoverBuilder::theme_signal(core::Signal<style::Theme>* sig) {
    config_.theme_signal = sig;
    return *this;
}

overlay::OverlayId PopoverBuilder::show() {
    const auto& th = config_.theme_signal ? config_.theme_signal->get() : config_.theme;
    const auto& c = th.colors();
    core::Color bg = config_.custom_background ? config_.background : th.surface_container_high();
    core::Color border = config_.custom_border ? config_.border_color : c.outline_variant;

    auto card_builder = BoxBuilder(tree_)
                            .padding(config_.padding)
                            .background(bg)
                            .border(border, config_.border_width)
                            .corner_radii(config_.radii)
                            .shadow(core::Color::from_rgba8(0, 0, 0, th.is_dark() ? 100 : 35),
                                    16.0f, 0.0f, core::Point{0.0f, 4.0f})
                            .child(content_);

    if (config_.theme_signal && !config_.custom_background && !config_.custom_border) {
        card_builder.theme_signal(config_.theme_signal, [](const style::Theme& t, BoxData& d) {
            d.style.background = t.surface_container_high();
            d.style.border_color = t.colors().outline_variant;
        });
    }

    auto card = card_builder.build();

    overlay::OverlayConfig cfg{
        .placement = config_.placement,
        .offset = config_.offset,
        .flip_on_collision = true,
        .clamp_to_viewport = true,
        .modal = config_.modal,
        .barrier_dismissible = config_.barrier_dismissible,
        .barrier_color = core::Color{0.0f, 0.0f, 0.0f, 0.0f},
    };

    if (has_point_) {
        return mgr_.show_overlay_at_point(card, point_, cfg);
    }
    return mgr_.show_overlay(card, cfg, anchor_);
}

} // namespace north::widget
