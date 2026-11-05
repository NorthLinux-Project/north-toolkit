#include "north/widget/menu.hpp"

#include "north/widget/box.hpp"
#include "north/widget/button.hpp"
#include "north/widget/icon.hpp"
#include "north/widget/text.hpp"

namespace north::widget {

MenuBuilder::MenuBuilder(core::NodeTree& tree, overlay::OverlayManager& mgr)
    : tree_(tree), mgr_(mgr) {}

MenuBuilder& MenuBuilder::item(const MenuItem& it) {
    items_.push_back(it);
    return *this;
}

MenuBuilder& MenuBuilder::item(std::string_view label, MenuItemActionFn on_click, void* user_data,
                               std::string_view shortcut) {
    items_.push_back(MenuItem::action(label, on_click, user_data, shortcut));
    return *this;
}

MenuBuilder& MenuBuilder::divider() {
    items_.push_back(MenuItem::divider());
    return *this;
}

MenuBuilder& MenuBuilder::min_width(float w) {
    min_width_ = w;
    return *this;
}

MenuBuilder& MenuBuilder::placement(overlay::Placement p) {
    placement_ = p;
    return *this;
}

MenuBuilder& MenuBuilder::theme(const style::Theme& th) {
    theme_ = th;
    return *this;
}

MenuBuilder& MenuBuilder::theme_signal(core::Signal<style::Theme>* sig) {
    theme_signal_ = sig;
    return *this;
}

namespace {

struct MenuItemCallbackWrapper {
    MenuItemActionFn user_callback{nullptr};
    void* user_data{nullptr};
    overlay::OverlayManager* mgr{nullptr};
    overlay::OverlayId overlay_id{overlay::kInvalidOverlayId};
};

core::Handle<core::Node> build_menu_node(core::NodeTree& tree, overlay::OverlayManager& mgr,
                                         const std::vector<MenuItem>& items, float min_width,
                                         const style::Theme& theme,
                                         core::Signal<style::Theme>* theme_signal,
                                         std::vector<MenuItemCallbackWrapper*>& out_wrappers) {
    const auto& c = theme.colors();
    std::vector<core::Handle<core::Node>> item_nodes;

    for (const auto& it : items) {
        if (it.is_divider) {
            auto div_builder = BoxBuilder(tree)
                                   .height(1.0f)
                                   .background(c.outline_variant)
                                   .margin(core::Edges{4.0f, 0.0f, 4.0f, 0.0f});
            if (theme_signal) {
                div_builder.theme_signal(theme_signal, [](const style::Theme& t, BoxData& d) {
                    d.style.background = t.colors().outline_variant;
                });
            }
            item_nodes.push_back(div_builder.build());
            continue;
        }

        auto* wrapper = new MenuItemCallbackWrapper{
            .user_callback = it.on_click,
            .user_data = it.user_data,
            .mgr = &mgr,
            .overlay_id = overlay::kInvalidOverlayId,
        };
        out_wrappers.push_back(wrapper);

        style::BoxStyle norm_style;
        norm_style.background = core::Color{0.0f, 0.0f, 0.0f, 0.0f};
        norm_style.corner_radii = core::CornerRadii::uniform(4.0f);

        style::BoxStyle hov_style;
        hov_style.background = core::Color{c.primary.r, c.primary.g, c.primary.b,
                                           theme.is_dark() ? 0.16f : 0.10f};
        hov_style.corner_radii = core::CornerRadii::uniform(4.0f);

        style::BoxStyle press_style;
        press_style.background = core::Color{c.primary.r, c.primary.g, c.primary.b,
                                             theme.is_dark() ? 0.28f : 0.20f};
        press_style.corner_radii = core::CornerRadii::uniform(4.0f);

        auto btn_builder = ButtonBuilder(tree, it.label)
                               .theme(theme)
                               .style(norm_style, hov_style, press_style)
                               .text_color(c.on_surface)
                               .padding(core::Edges{6.0f, 12.0f, 6.0f, 12.0f})
                               .font_size(13.0f);

        if (theme_signal) {
            btn_builder.theme_signal(theme_signal);
        }

        btn_builder.on_click(
            [](void* ud) {
                auto* w = static_cast<MenuItemCallbackWrapper*>(ud);
                if (w->user_callback) {
                    w->user_callback(w->user_data);
                }
                if (w->mgr && w->overlay_id != overlay::kInvalidOverlayId) {
                    w->mgr->hide_overlay(w->overlay_id);
                }
                delete w;
            },
            wrapper);

        item_nodes.push_back(btn_builder.build());
    }

    auto card_builder =
        BoxBuilder(tree)
            .width(min_width)
            .padding(core::Edges::uniform(4.0f))
            .background(theme.surface_container_high())
            .border(c.outline_variant, core::Edges::uniform(1.0f))
            .corner_radii(core::CornerRadii::uniform(8.0f))
            .shadow(core::Color::from_rgba8(0, 0, 0, theme.is_dark() ? 120 : 35), 16.0f, 0.0f,
                    core::Point{0.0f, 4.0f})
            .flex(layout::FlexDirection::Column, layout::MainAxisAlignment::Start,
                  layout::CrossAxisAlignment::Stretch, 2.0f)
            .children(item_nodes);

    if (theme_signal) {
        card_builder.theme_signal(theme_signal, [](const style::Theme& t, BoxData& d) {
            d.style.background = t.surface_container_high();
            d.style.border_color = t.colors().outline_variant;
        });
    }

    return card_builder.build();
}

} // namespace

overlay::OverlayId MenuBuilder::show_at_anchor(core::Handle<core::Node> anchor) {
    const auto& th = theme_signal_ ? theme_signal_->get() : theme_;
    std::vector<MenuItemCallbackWrapper*> wrappers;
    auto node = build_menu_node(tree_, mgr_, items_, min_width_, th, theme_signal_, wrappers);

    overlay::OverlayConfig cfg{
        .placement = placement_,
        .offset = 4.0f,
        .flip_on_collision = true,
        .clamp_to_viewport = true,
        .modal = true,
        .barrier_dismissible = true,
        .barrier_color = core::Color{0.0f, 0.0f, 0.0f, 0.0f}, // Transparent barrier for dropdowns
    };

    overlay::OverlayId id = mgr_.show_overlay(node, cfg, anchor);
    for (auto* w : wrappers) {
        w->overlay_id = id;
    }
    return id;
}

overlay::OverlayId MenuBuilder::show_at_point(core::Point point) {
    const auto& th = theme_signal_ ? theme_signal_->get() : theme_;
    std::vector<MenuItemCallbackWrapper*> wrappers;
    auto node = build_menu_node(tree_, mgr_, items_, min_width_, th, theme_signal_, wrappers);

    overlay::OverlayConfig cfg{
        .placement = placement_,
        .offset = 2.0f,
        .flip_on_collision = true,
        .clamp_to_viewport = true,
        .modal = true,
        .barrier_dismissible = true,
        .barrier_color = core::Color{0.0f, 0.0f, 0.0f, 0.0f},
    };

    overlay::OverlayId id = mgr_.show_overlay_at_point(node, point, cfg);
    for (auto* w : wrappers) {
        w->overlay_id = id;
    }
    return id;
}

} // namespace north::widget
