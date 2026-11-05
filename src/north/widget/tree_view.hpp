#pragma once

#include "north/core/tree.hpp"
#include "north/style/theme.hpp"
#include "north/widget/widget_data.hpp"

namespace north::widget {

class TreeViewBuilder {
public:
    explicit TreeViewBuilder(core::NodeTree& tree);

    TreeViewBuilder& roots(std::vector<TreeNodeItem> roots);
    TreeViewBuilder& root_item(TreeNodeItem item);
    TreeViewBuilder& selected_id(uint32_t id);
    TreeViewBuilder& font(std::shared_ptr<text::Font> font);
    TreeViewBuilder& font_size(float size);
    TreeViewBuilder& row_height(float height);
    TreeViewBuilder& indent_width(float width);
    TreeViewBuilder& indent_guides(bool show);
    TreeViewBuilder& width(float width);
    TreeViewBuilder& height(float height);
    TreeViewBuilder& theme(const style::Theme& theme);
    TreeViewBuilder& theme_signal(core::Signal<style::Theme>* sig, TreeViewThemeFn fn = nullptr);
    TreeViewBuilder& on_select(TreeSelectCallbackFn fn, void* user_data = nullptr);
    TreeViewBuilder& on_toggle(TreeToggleCallbackFn fn, void* user_data = nullptr);
    TreeViewBuilder& on_activate(TreeActivateCallbackFn fn, void* user_data = nullptr);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    TreeViewData data_;
};

} // namespace north::widget
