#pragma once

#include "north/core/tree.hpp"
#include "north/overlay/overlay_manager.hpp"
#include "north/style/theme.hpp"
#include "north/widget/widget_data.hpp"

namespace north::widget {

class SelectBuilder {
public:
    explicit SelectBuilder(core::NodeTree& tree);

    SelectBuilder& item(uint32_t id, std::string_view label,
                        std::string_view secondary_label = "",
                        render::Path icon = {},
                        bool disabled = false,
                        void* user_data = nullptr);
    SelectBuilder& items(std::vector<SelectItem> items);
    SelectBuilder& selected_id(uint32_t id);
    SelectBuilder& width(float width);
    SelectBuilder& height(float height);
    SelectBuilder& dropdown_max_height(float height);
    SelectBuilder& font(std::shared_ptr<text::Font> font);
    SelectBuilder& font_size(float size);
    SelectBuilder& theme(const style::Theme& theme);
    SelectBuilder& theme_signal(core::Signal<style::Theme>* sig, SelectThemeFn fn = nullptr);
    SelectBuilder& on_change(SelectChangeCallbackFn fn, void* user_data = nullptr);
    SelectBuilder& overlay_manager(overlay::OverlayManager& manager);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    SelectData data_;
};

} // namespace north::widget
