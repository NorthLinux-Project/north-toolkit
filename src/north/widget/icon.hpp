#pragma once

#include "north/core/tree.hpp"
#include "north/render/path.hpp"
#include "north/render/svg_path.hpp"
#include "north/widget/widget_data.hpp"

#include <string_view>

namespace north::widget {

class IconBuilder {
public:
    IconBuilder(core::NodeTree& tree, const render::Path& path);
    IconBuilder(core::NodeTree& tree, std::string_view svg_path_d);

    IconBuilder& size(float width, float height);
    IconBuilder& size(core::Size sz);
    IconBuilder& color(core::Color c);
    IconBuilder& stroke(const render::StrokeStyle& style);
    IconBuilder& theme_signal(core::Signal<style::Theme>* sig, IconThemeFn fn = nullptr);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    IconData data_;
};

} // namespace north::widget
