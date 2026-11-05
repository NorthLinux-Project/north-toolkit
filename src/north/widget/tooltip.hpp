#pragma once

#include "north/core/tree.hpp"
#include "north/overlay/overlay_manager.hpp"
#include "north/style/theme.hpp"

#include <string>
#include <string_view>

namespace north::widget {

struct TooltipConfig {
    std::string text;
    overlay::Placement placement{overlay::Placement::Top};
    float offset{6.0f};
    float delay_seconds{0.4f};
    style::Theme theme{style::Theme::dark()};
    core::Signal<style::Theme>* theme_signal{nullptr};
};

class TooltipBuilder {
public:
    TooltipBuilder(core::NodeTree& tree, overlay::OverlayManager& mgr,
                   core::Handle<core::Node> anchor);

    TooltipBuilder& text(std::string_view t);
    TooltipBuilder& placement(overlay::Placement p);
    TooltipBuilder& offset(float off);
    TooltipBuilder& theme(const style::Theme& th);
    TooltipBuilder& theme_signal(core::Signal<style::Theme>* sig);

    overlay::OverlayId show();

private:
    core::NodeTree& tree_;
    overlay::OverlayManager& mgr_;
    core::Handle<core::Node> anchor_;
    TooltipConfig config_;
};

overlay::OverlayId show_tooltip(core::NodeTree& tree, overlay::OverlayManager& mgr,
                                core::Handle<core::Node> anchor, std::string_view text,
                                overlay::Placement placement = overlay::Placement::Top,
                                const style::Theme& theme = style::Theme::dark(),
                                core::Signal<style::Theme>* theme_signal = nullptr);

} // namespace north::widget
