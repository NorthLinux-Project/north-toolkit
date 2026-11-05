#pragma once

#include "north/core/tree.hpp"
#include "north/overlay/overlay_manager.hpp"
#include "north/render/path.hpp"
#include "north/style/theme.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace north::widget {

using MenuItemActionFn = void (*)(void* user_data);

struct MenuItem {
    std::string label;
    std::string shortcut;
    render::Path icon_path{};
    MenuItemActionFn on_click{nullptr};
    void* user_data{nullptr};
    bool is_divider{false};
    bool disabled{false};

    static MenuItem action(std::string_view label, MenuItemActionFn on_click = nullptr,
                           void* user_data = nullptr, std::string_view shortcut = "") {
        return MenuItem{
            .label = std::string(label),
            .shortcut = std::string(shortcut),
            .on_click = on_click,
            .user_data = user_data,
            .is_divider = false,
            .disabled = false,
        };
    }

    static MenuItem divider() {
        return MenuItem{
            .label = {},
            .shortcut = {},
            .is_divider = true,
            .disabled = true,
        };
    }
};

class MenuBuilder {
public:
    MenuBuilder(core::NodeTree& tree, overlay::OverlayManager& mgr);

    MenuBuilder& item(const MenuItem& it);
    MenuBuilder& item(std::string_view label, MenuItemActionFn on_click = nullptr,
                      void* user_data = nullptr, std::string_view shortcut = "");
    MenuBuilder& divider();
    MenuBuilder& min_width(float w);
    MenuBuilder& placement(overlay::Placement p);
    MenuBuilder& theme(const style::Theme& th);
    MenuBuilder& theme_signal(core::Signal<style::Theme>* sig);

    overlay::OverlayId show_at_anchor(core::Handle<core::Node> anchor);
    overlay::OverlayId show_at_point(core::Point point);

private:
    core::NodeTree& tree_;
    overlay::OverlayManager& mgr_;
    std::vector<MenuItem> items_;
    float min_width_{160.0f};
    overlay::Placement placement_{overlay::Placement::BottomStart};
    style::Theme theme_{style::Theme::dark()};
    core::Signal<style::Theme>* theme_signal_{nullptr};
};

} // namespace north::widget
