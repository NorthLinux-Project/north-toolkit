#include "north/widget/tooltip.hpp"

#include "north/widget/box.hpp"
#include "north/widget/text.hpp"

namespace north::widget {

TooltipBuilder::TooltipBuilder(core::NodeTree& tree, overlay::OverlayManager& mgr,
                               core::Handle<core::Node> anchor)
    : tree_(tree), mgr_(mgr), anchor_(anchor) {}

TooltipBuilder& TooltipBuilder::text(std::string_view t) {
    config_.text = std::string(t);
    return *this;
}

TooltipBuilder& TooltipBuilder::placement(overlay::Placement p) {
    config_.placement = p;
    return *this;
}

TooltipBuilder& TooltipBuilder::offset(float off) {
    config_.offset = off;
    return *this;
}

TooltipBuilder& TooltipBuilder::theme(const style::Theme& th) {
    config_.theme = th;
    return *this;
}

TooltipBuilder& TooltipBuilder::theme_signal(core::Signal<style::Theme>* sig) {
    config_.theme_signal = sig;
    return *this;
}

overlay::OverlayId TooltipBuilder::show() {
    return show_tooltip(tree_, mgr_, anchor_, config_.text, config_.placement, config_.theme,
                        config_.theme_signal);
}

overlay::OverlayId show_tooltip(core::NodeTree& tree, overlay::OverlayManager& mgr,
                                core::Handle<core::Node> anchor, std::string_view text,
                                overlay::Placement placement, const style::Theme& theme,
                                core::Signal<style::Theme>* theme_signal) {
    const auto& th = theme_signal ? theme_signal->get() : theme;
    const auto& c = th.colors();

    auto text_builder = TextBuilder(tree, text)
                            .font_size(12.0f)
                            .color(c.on_surface);
    if (theme_signal) {
        text_builder.theme_signal(theme_signal, [](const style::ColorTokens& tok) {
            return tok.on_surface;
        });
    }

    auto badge_builder =
        BoxBuilder(tree)
            .padding(core::Edges{4.0f, 8.0f, 4.0f, 8.0f})
            .background(th.surface_container_highest())
            .border(c.outline_variant, core::Edges::uniform(1.0f))
            .corner_radii(core::CornerRadii::uniform(4.0f))
            .shadow(core::Color::from_rgba8(0, 0, 0, th.is_dark() ? 100 : 30), 8.0f, 0.0f,
                    core::Point{0.0f, 2.0f})
            .child(text_builder.build());

    if (theme_signal) {
        badge_builder.theme_signal(theme_signal, [](const style::Theme& t, BoxData& d) {
            d.style.background = t.surface_container_highest();
            d.style.border_color = t.colors().outline_variant;
        });
    }

    auto badge = badge_builder.build();

    overlay::OverlayConfig cfg{
        .placement = placement,
        .offset = 6.0f,
        .flip_on_collision = true,
        .clamp_to_viewport = true,
        .modal = false,
        .barrier_dismissible = false,
        .barrier_color = core::Color{0.0f, 0.0f, 0.0f, 0.0f},
    };

    return mgr.show_overlay(badge, cfg, anchor);
}

} // namespace north::widget
