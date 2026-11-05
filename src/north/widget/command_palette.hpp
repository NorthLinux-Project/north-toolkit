#pragma once

#include "north/core/tree.hpp"
#include "north/overlay/overlay_manager.hpp"
#include "north/style/theme.hpp"
#include "north/widget/widget_data.hpp"

namespace north::widget {

class CommandPaletteBuilder {
public:
    explicit CommandPaletteBuilder(core::NodeTree& tree);

    CommandPaletteBuilder& item(uint32_t id, std::string_view title,
                                std::string_view category = "",
                                std::string_view shortcut_hint = "",
                                render::Path icon = {},
                                PaletteActionFn on_action = nullptr,
                                void* user_data = nullptr);
    CommandPaletteBuilder& items(std::vector<PaletteItem> items);
    CommandPaletteBuilder& placeholder(std::string_view placeholder);
    CommandPaletteBuilder& mode_prefix(char prefix);
    CommandPaletteBuilder& width(float width);
    CommandPaletteBuilder& max_height(float height);
    CommandPaletteBuilder& row_height(float height);
    CommandPaletteBuilder& font(std::shared_ptr<text::Font> font);
    CommandPaletteBuilder& font_size(float size);
    CommandPaletteBuilder& theme(const style::Theme& theme);
    CommandPaletteBuilder& theme_signal(core::Signal<style::Theme>* sig, CommandPaletteThemeFn fn = nullptr);
    CommandPaletteBuilder& on_action(PaletteActionFn fn, void* user_data = nullptr);
    CommandPaletteBuilder& on_dismiss(PaletteDismissFn fn, void* user_data = nullptr);
    CommandPaletteBuilder& overlay_manager(overlay::OverlayManager& manager);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    CommandPaletteData data_;
};

} // namespace north::widget
