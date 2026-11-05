#pragma once

#include "north/core/tree.hpp"
#include "north/overlay/overlay_manager.hpp"
#include "north/style/theme.hpp"

#include <string>
#include <string_view>

namespace north::widget {

using DialogActionFn = void (*)(void* user_data);

struct DialogAction {
    std::string label;
    DialogActionFn on_click{nullptr};
    void* user_data{nullptr};
    bool is_primary{false};
};

struct DialogConfig {
    std::string title;
    std::string message;
    std::vector<DialogAction> actions;
    float max_width{440.0f};
    bool barrier_dismissible{true};
    style::Theme theme{style::Theme::dark()};
    core::Signal<style::Theme>* theme_signal{nullptr};
};

class DialogBuilder {
public:
    DialogBuilder(core::NodeTree& tree, overlay::OverlayManager& mgr);

    DialogBuilder& title(std::string_view t);
    DialogBuilder& message(std::string_view msg);
    DialogBuilder& max_width(float w);
    DialogBuilder& action(std::string_view label, DialogActionFn callback = nullptr,
                          void* user_data = nullptr, bool is_primary = false);
    DialogBuilder& barrier_dismissible(bool dismissible);
    DialogBuilder& theme(const style::Theme& th);
    DialogBuilder& theme_signal(core::Signal<style::Theme>* sig);

    overlay::OverlayId show();

private:
    core::NodeTree& tree_;
    overlay::OverlayManager& mgr_;
    DialogConfig config_;
};

overlay::OverlayId show_dialog(core::NodeTree& tree, overlay::OverlayManager& mgr,
                               const DialogConfig& config);

} // namespace north::widget
