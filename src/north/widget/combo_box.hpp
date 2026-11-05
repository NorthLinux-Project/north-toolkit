#pragma once

#include "north/core/tree.hpp"
#include "north/overlay/overlay_manager.hpp"
#include "north/style/theme.hpp"
#include "north/widget/widget_data.hpp"

namespace north::widget {

class ComboBoxBuilder {
public:
    explicit ComboBoxBuilder(core::NodeTree& tree);

    ComboBoxBuilder& item(uint32_t id, std::string_view label,
                          std::string_view secondary_label = "",
                          render::Path icon = {},
                          bool disabled = false,
                          void* user_data = nullptr);
    ComboBoxBuilder& items(std::vector<SelectItem> items);
    ComboBoxBuilder& text(std::string_view text);
    ComboBoxBuilder& placeholder(std::string_view placeholder);
    ComboBoxBuilder& width(float width);
    ComboBoxBuilder& height(float height);
    ComboBoxBuilder& dropdown_max_height(float height);
    ComboBoxBuilder& font(std::shared_ptr<text::Font> font);
    ComboBoxBuilder& font_size(float size);
    ComboBoxBuilder& theme(const style::Theme& theme);
    ComboBoxBuilder& theme_signal(core::Signal<style::Theme>* sig, ComboBoxThemeFn fn = nullptr);
    ComboBoxBuilder& on_select(ComboBoxSelectCallbackFn fn, void* user_data = nullptr);
    ComboBoxBuilder& on_text_change(ComboBoxTextCallbackFn fn, void* user_data = nullptr);
    ComboBoxBuilder& overlay_manager(overlay::OverlayManager& manager);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    ComboBoxData data_;
};

} // namespace north::widget
