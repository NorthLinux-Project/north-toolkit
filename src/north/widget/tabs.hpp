#pragma once

#include "north/core/tree.hpp"
#include "north/render/path.hpp"
#include "north/style/theme.hpp"
#include "north/widget/widget_data.hpp"

#include <memory>
#include <string_view>

namespace north::widget {

class TabBarBuilder {
public:
    explicit TabBarBuilder(core::NodeTree& tree);

    TabBarBuilder& tab(std::string_view label, bool closable = true, bool show_badge = false,
                       std::string_view icon_svg = {});
    TabBarBuilder& tab_with_badge(std::string_view label, std::string_view badge_text,
                                  core::Color badge_color = {0.92f, 0.34f, 0.34f, 1.0f},
                                  bool closable = true, std::string_view icon_svg = {});
    TabBarBuilder& active_tab(size_t index);
    TabBarBuilder& reorderable(bool enabled);
    TabBarBuilder& add_button(bool show);
    TabBarBuilder& font(std::shared_ptr<text::Font> font);
    TabBarBuilder& font_size(float size);
    TabBarBuilder& bar_height(float height);
    TabBarBuilder& tab_width_bounds(float min_w, float max_w);
    TabBarBuilder& theme(const style::Theme& theme);
    TabBarBuilder& theme_signal(core::Signal<style::Theme>* sig, TabBarThemeFn fn = nullptr);
    TabBarBuilder& on_select(TabSelectCallbackFn fn, void* user_data = nullptr);
    TabBarBuilder& on_close(TabCloseCallbackFn fn, void* user_data = nullptr);
    TabBarBuilder& on_add(ClickCallbackFn fn, void* user_data = nullptr);
    TabBarBuilder& on_reorder(TabReorderCallbackFn fn, void* user_data = nullptr);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    TabBarData data_;
};

class TabsViewBuilder {
public:
    explicit TabsViewBuilder(core::NodeTree& tree);

    TabsViewBuilder& tab(std::string_view label, core::Handle<core::Node> content_node,
                         bool closable = true, bool show_badge = false,
                         std::string_view icon_svg = {});
    TabsViewBuilder& lazy_tab(std::string_view label, TabContentFactoryFn build_fn,
                              void* user_data = nullptr, bool closable = true,
                              bool show_badge = false, std::string_view icon_svg = {});
    TabsViewBuilder& retention_policy(TabRetentionPolicy policy);
    TabsViewBuilder& active_tab(size_t index);
    TabsViewBuilder& reorderable(bool enabled);
    TabsViewBuilder& add_button(bool show);
    TabsViewBuilder& font(std::shared_ptr<text::Font> font);
    TabsViewBuilder& font_size(float size);
    TabsViewBuilder& bar_height(float height);
    TabsViewBuilder& width(float width);
    TabsViewBuilder& height(float height);
    TabsViewBuilder& theme(const style::Theme& theme);
    TabsViewBuilder& theme_signal(core::Signal<style::Theme>* sig);
    TabsViewBuilder& on_select(TabSelectCallbackFn fn, void* user_data = nullptr);
    TabsViewBuilder& on_close(TabCloseCallbackFn fn, void* user_data = nullptr);
    TabsViewBuilder& on_add(ClickCallbackFn fn, void* user_data = nullptr);
    TabsViewBuilder& on_reorder(TabReorderCallbackFn fn, void* user_data = nullptr);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    TabsViewData view_data_;
    TabBarData bar_data_;
};

} // namespace north::widget
