#include "north/widget/dialog.hpp"

#include "north/widget/box.hpp"
#include "north/widget/button.hpp"
#include "north/widget/text.hpp"

#include <memory>

namespace north::widget {

DialogBuilder::DialogBuilder(core::NodeTree& tree, overlay::OverlayManager& mgr)
    : tree_(tree), mgr_(mgr) {}

DialogBuilder& DialogBuilder::title(std::string_view t) {
    config_.title = std::string(t);
    return *this;
}

DialogBuilder& DialogBuilder::message(std::string_view msg) {
    config_.message = std::string(msg);
    return *this;
}

DialogBuilder& DialogBuilder::max_width(float w) {
    config_.max_width = w;
    return *this;
}

DialogBuilder& DialogBuilder::action(std::string_view label, DialogActionFn callback,
                                     void* user_data, bool is_primary) {
    config_.actions.push_back(DialogAction{
        .label = std::string(label),
        .on_click = callback,
        .user_data = user_data,
        .is_primary = is_primary,
    });
    return *this;
}

DialogBuilder& DialogBuilder::barrier_dismissible(bool dismissible) {
    config_.barrier_dismissible = dismissible;
    return *this;
}

DialogBuilder& DialogBuilder::theme(const style::Theme& th) {
    config_.theme = th;
    return *this;
}

DialogBuilder& DialogBuilder::theme_signal(core::Signal<style::Theme>* sig) {
    config_.theme_signal = sig;
    return *this;
}

overlay::OverlayId DialogBuilder::show() {
    return show_dialog(tree_, mgr_, config_);
}

namespace {

struct DialogDismissContext;

struct DialogActionWrapper {
    DialogActionFn user_callback{nullptr};
    void* user_data{nullptr};
    DialogDismissContext* ctx{nullptr};
};

struct DialogDismissContext {
    core::NodeTree* tree{nullptr};
    overlay::OverlayManager* mgr{nullptr};
    core::Handle<core::Node> card_node{};
    overlay::OverlayId overlay_id{overlay::kInvalidOverlayId};
    std::vector<std::unique_ptr<DialogActionWrapper>> wrappers;
};

} // namespace

overlay::OverlayId show_dialog(core::NodeTree& tree, overlay::OverlayManager& mgr,
                               const DialogConfig& config) {
    const auto& th = config.theme_signal ? config.theme_signal->get() : config.theme;
    const auto& c = th.colors();

    auto* ctx = new DialogDismissContext{
        .tree = &tree,
        .mgr = &mgr,
    };

    std::vector<core::Handle<core::Node>> children;

    // Title
    if (!config.title.empty()) {
        auto title_builder = TextBuilder(tree, config.title)
                                 .font_size(20.0f)
                                 .color(c.on_surface);
        if (config.theme_signal) {
            title_builder.theme_signal(config.theme_signal, [](const style::ColorTokens& tok) {
                return tok.on_surface;
            });
        }
        children.push_back(title_builder.build());
    }

    // Message
    if (!config.message.empty()) {
        auto msg_builder = TextBuilder(tree, config.message)
                               .font_size(14.0f)
                               .color(c.on_surface_variant);
        if (config.theme_signal) {
            msg_builder.theme_signal(config.theme_signal, [](const style::ColorTokens& tok) {
                return tok.on_surface_variant;
            });
        }
        children.push_back(msg_builder.build());
    }

    // Actions
    if (!config.actions.empty()) {
        std::vector<core::Handle<core::Node>> button_handles;
        for (const auto& act : config.actions) {
            auto wrapper = std::make_unique<DialogActionWrapper>(DialogActionWrapper{
                .user_callback = act.on_click,
                .user_data = act.user_data,
                .ctx = ctx,
            });
            DialogActionWrapper* raw_wrapper = wrapper.get();
            ctx->wrappers.push_back(std::move(wrapper));

            auto btn_builder = ButtonBuilder(tree, act.label)
                                   .theme(th)
                                   .padding(core::Edges{8.0f, 16.0f, 8.0f, 16.0f})
                                   .corner_radii(core::CornerRadii::uniform(8.0f));

            if (!act.is_primary) {
                style::BoxStyle norm_style;
                norm_style.background = core::Color{0.0f, 0.0f, 0.0f, 0.0f};
                norm_style.border_color = c.outline_variant;
                norm_style.border_width = core::Edges::uniform(1.0f);
                norm_style.corner_radii = core::CornerRadii::uniform(8.0f);

                style::BoxStyle hov_style = norm_style;
                hov_style.background = core::Color{c.primary.r, c.primary.g, c.primary.b,
                                                   th.is_dark() ? 0.16f : 0.10f};

                style::BoxStyle press_style = norm_style;
                press_style.background = core::Color{c.primary.r, c.primary.g, c.primary.b,
                                                     th.is_dark() ? 0.28f : 0.20f};

                btn_builder.style(norm_style, hov_style, press_style);
                btn_builder.text_color(c.on_surface);
            }

            if (config.theme_signal) {
                btn_builder.theme_signal(config.theme_signal);
            }

            btn_builder.on_click(
                [](void* ud) {
                    auto* w = static_cast<DialogActionWrapper*>(ud);
                    DialogActionFn cb = w->user_callback;
                    void* cb_ud = w->user_data;
                    overlay::OverlayManager* m = w->ctx ? w->ctx->mgr : nullptr;
                    overlay::OverlayId oid =
                        w->ctx ? w->ctx->overlay_id : overlay::kInvalidOverlayId;
                    if (cb) {
                        cb(cb_ud);
                    }
                    if (m && oid != overlay::kInvalidOverlayId) {
                        m->hide_overlay(oid);
                    }
                },
                raw_wrapper);

            button_handles.push_back(btn_builder.build());
        }

        auto actions_row = BoxBuilder(tree)
                               .flex(layout::FlexDirection::Row, layout::MainAxisAlignment::End,
                                     layout::CrossAxisAlignment::Center, 8.0f)
                               .children(button_handles)
                               .build();
        children.push_back(actions_row);
    }

    auto card_builder =
        BoxBuilder(tree)
            .width(config.max_width)
            .padding(core::Edges::uniform(24.0f))
            .background(th.surface_container_high())
            .border(c.outline_variant, core::Edges::uniform(1.0f))
            .corner_radii(core::CornerRadii::uniform(16.0f))
            .shadow(core::Color::from_rgba8(0, 0, 0, th.is_dark() ? 100 : 35), 24.0f, 0.0f,
                    core::Point{0.0f, 8.0f})
            .flex(layout::FlexDirection::Column, layout::MainAxisAlignment::Start,
                  layout::CrossAxisAlignment::Stretch, 16.0f)
            .children(children);

    if (config.theme_signal) {
        card_builder.theme_signal(config.theme_signal, [](const style::Theme& t, BoxData& d) {
            d.style.background = t.surface_container_high();
            d.style.border_color = t.colors().outline_variant;
        });
    }

    auto dialog_card = card_builder.build();
    ctx->card_node = dialog_card;

    overlay::OverlayConfig o_cfg{
        .placement = overlay::Placement::Auto,
        .modal = true,
        .barrier_dismissible = config.barrier_dismissible,
        .barrier_color = th.is_dark() ? core::Color{0.0f, 0.0f, 0.0f, 0.5f}
                                      : core::Color{0.0f, 0.0f, 0.0f, 0.35f},
        .on_dismiss =
            [](void* ud) {
                auto* d_ctx = static_cast<DialogDismissContext*>(ud);
                if (d_ctx->tree && d_ctx->card_node.is_valid()) {
                    d_ctx->tree->destroy_node(d_ctx->card_node);
                }
                delete d_ctx;
            },
        .dismiss_user_data = ctx,
    };

    overlay::OverlayId id = mgr.show_overlay(dialog_card, o_cfg);
    ctx->overlay_id = id;
    return id;
}

} // namespace north::widget
