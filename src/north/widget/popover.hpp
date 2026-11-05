#pragma once

#include "north/core/tree.hpp"
#include "north/overlay/overlay_manager.hpp"
#include "north/style/theme.hpp"

namespace north::widget {

struct PopoverConfig {
    overlay::Placement placement{overlay::Placement::Bottom};
    float offset{8.0f};
    bool modal{true};
    bool barrier_dismissible{true};
    core::Edges padding{core::Edges::uniform(16.0f)};
    core::CornerRadii radii{core::CornerRadii::uniform(12.0f)};
    core::Color background{core::Color::from_rgba8(32, 32, 38, 255)};
    core::Color border_color{core::Color::from_rgba8(60, 60, 72, 200)};
    core::Edges border_width{core::Edges::uniform(1.0f)};
    style::Theme theme{style::Theme::dark()};
    core::Signal<style::Theme>* theme_signal{nullptr};
    bool custom_background{false};
    bool custom_border{false};
};

class PopoverBuilder {
public:
    PopoverBuilder(core::NodeTree& tree, overlay::OverlayManager& mgr,
                   core::Handle<core::Node> content);

    PopoverBuilder& anchor(core::Handle<core::Node> a);
    PopoverBuilder& point(core::Point pt);
    PopoverBuilder& placement(overlay::Placement p);
    PopoverBuilder& offset(float off);
    PopoverBuilder& modal(bool m);
    PopoverBuilder& padding(core::Edges pad);
    PopoverBuilder& corner_radii(core::CornerRadii r);
    PopoverBuilder& background(core::Color bg);
    PopoverBuilder& border_color(core::Color bc);
    PopoverBuilder& theme(const style::Theme& th);
    PopoverBuilder& theme_signal(core::Signal<style::Theme>* sig);

    overlay::OverlayId show();

private:
    core::NodeTree& tree_;
    overlay::OverlayManager& mgr_;
    core::Handle<core::Node> content_;
    core::Handle<core::Node> anchor_{};
    core::Point point_{0.0f, 0.0f};
    bool has_point_{false};
    PopoverConfig config_;
};

} // namespace north::widget
