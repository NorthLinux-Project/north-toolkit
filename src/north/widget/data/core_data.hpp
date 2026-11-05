#pragma once

#include "north/animation/animated_value.hpp"
#include "north/animation/curve.hpp"
#include "north/core/signal.hpp"
#include "north/core/tree.hpp"
#include "north/core/types.hpp"
#include "north/image/image_types.hpp"
#include "north/layout/flex.hpp"
#include "north/render/path.hpp"
#include "north/render/path_tessellator.hpp"
#include "north/style/style.hpp"
#include "north/style/theme.hpp"
#include "north/text/font.hpp"
#include "north/text/paragraph.hpp"

#include <memory>
#include <string>

namespace north::overlay {
class OverlayManager;
}

namespace north::widget {

enum class WidgetKind : uint8_t {
    Box,
    Text,
    Button,
    TextInput,
    ScrollArea,
    VirtualList,
    Checkbox,
    Switch,
    Slider,
    RadioButton,
    Icon,
    Image,
    TextArea,
    TabBar,
    TabsView,
    SegmentedButton,
    Accordion,
    SplitView,
    TreeView,
    DataTable,
    CommandPalette,
    Select,
    ComboBox
};

enum class ButtonState : uint8_t { Normal, Hovered, Pressed };

using ClickCallbackFn = void (*)(void* user_data);

struct BoxData;
using BoxThemeFn = void (*)(const style::Theme& theme, BoxData& data);

struct BoxData {
    WidgetKind kind{WidgetKind::Box};
    style::BoxStyle style{};
    core::Edges padding{};
    core::Edges margin{};
    layout::FlexStyle flex_style{};
    float width{-1.0f};
    float height{-1.0f};
    float flex_grow{0.0f};
    float flex_shrink{1.0f};
    float flex_basis{-1.0f};
    bool is_flex{false};
    core::Signal<style::Theme>* theme_signal{nullptr};
    BoxThemeFn on_theme{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    animation::AnimatedValue<core::Color> animated_bg;
    animation::AnimatedValue<core::Color> animated_border;

    BoxData() = default;
    BoxData(BoxData&&) noexcept = default;
    BoxData& operator=(BoxData&&) noexcept = default;
    ~BoxData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    BoxData(const BoxData& other)
        : kind(other.kind), style(other.style), padding(other.padding), margin(other.margin),
          flex_style(other.flex_style), width(other.width), height(other.height),
          flex_grow(other.flex_grow), flex_shrink(other.flex_shrink), flex_basis(other.flex_basis),
          is_flex(other.is_flex), theme_signal(other.theme_signal), on_theme(other.on_theme),
          tree(other.tree), node_handle(other.node_handle) {}

    void apply_theme(const style::Theme& theme);
};

using TextThemeFn = core::Color (*)(const style::ColorTokens& tokens);

struct TextData {
    WidgetKind kind{WidgetKind::Text};
    std::string text_string;
    std::shared_ptr<text::Font> font;
    float font_size{16.0f};
    core::Color color{1.0f, 1.0f, 1.0f, 1.0f};
    text::TextAlign align{text::TextAlign::Left};
    float max_width{0.0f};
    std::unique_ptr<text::Paragraph> paragraph;
    core::Signal<style::Theme>* theme_signal{nullptr};
    TextThemeFn color_token_fn{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    animation::AnimatedValue<core::Color> animated_color;

    TextData() = default;
    TextData(TextData&&) noexcept = default;
    TextData& operator=(TextData&&) noexcept = default;
    ~TextData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    TextData(const TextData& other)
        : kind(other.kind), text_string(other.text_string), font(other.font),
          font_size(other.font_size), color(other.color), align(other.align),
          max_width(other.max_width), theme_signal(other.theme_signal),
          color_token_fn(other.color_token_fn), tree(other.tree), node_handle(other.node_handle) {}

    void apply_theme(const style::Theme& theme);
};

struct IconData;
using IconThemeFn = void (*)(const style::Theme& theme, IconData& data);

struct IconData {
    WidgetKind kind{WidgetKind::Icon};
    render::Path path{};
    render::PathMesh cached_mesh{};
    core::Size size{24.0f, 24.0f};
    core::Color color{1.0f, 1.0f, 1.0f, 1.0f};
    bool is_stroke{false};
    render::StrokeStyle stroke_style{};
    core::Signal<style::Theme>* theme_signal{nullptr};
    IconThemeFn on_theme{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    animation::AnimatedValue<core::Color> animated_color;

    IconData() = default;
    IconData(IconData&&) noexcept = default;
    IconData& operator=(IconData&&) noexcept = default;
    ~IconData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    IconData(const IconData& other)
        : kind(other.kind), path(other.path), cached_mesh(other.cached_mesh),
          size(other.size), color(other.color), is_stroke(other.is_stroke),
          stroke_style(other.stroke_style), theme_signal(other.theme_signal),
          on_theme(other.on_theme), tree(other.tree), node_handle(other.node_handle) {}

    void apply_theme(const style::Theme& theme);
};

struct ImageData;
using ImageThemeFn = void (*)(const style::Theme& theme, ImageData& data);

struct ImageData {
    WidgetKind kind{WidgetKind::Image};
    gpu::TextureId texture{gpu::kInvalidTexture};
    core::Size natural_size{0.0f, 0.0f};
    core::Size explicit_size{-1.0f, -1.0f};
    float aspect_ratio{-1.0f};
    image::ContentScale content_scale{image::ContentScale::Fit};
    image::ImageAlignment alignment{image::ImageAlignment::Center};
    core::CornerRadii corner_radii{};
    core::Color tint{1.0f, 1.0f, 1.0f, 1.0f};
    core::Color border_color{0.0f, 0.0f, 0.0f, 0.0f};
    core::Edges border_width{};
    core::Signal<style::Theme>* theme_signal{nullptr};
    ImageThemeFn on_theme{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    animation::AnimatedValue<core::Color> animated_tint;

    ImageData() = default;
    ImageData(ImageData&&) noexcept = default;
    ImageData& operator=(ImageData&&) noexcept = default;
    ~ImageData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    ImageData(const ImageData& other)
        : kind(other.kind), texture(other.texture), natural_size(other.natural_size),
          explicit_size(other.explicit_size), aspect_ratio(other.aspect_ratio),
          content_scale(other.content_scale), alignment(other.alignment),
          corner_radii(other.corner_radii), tint(other.tint), border_color(other.border_color),
          border_width(other.border_width), theme_signal(other.theme_signal),
          on_theme(other.on_theme), tree(other.tree), node_handle(other.node_handle) {}

    void apply_theme(const style::Theme& theme);
};

} // namespace north::widget
