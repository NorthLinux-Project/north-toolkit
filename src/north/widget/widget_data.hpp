#pragma once

#include "north/animation/animated_value.hpp"
#include "north/animation/curve.hpp"
#include "north/core/signal.hpp"
#include "north/core/types.hpp"
#include "north/image/image_types.hpp"
#include "north/layout/flex.hpp"
#include "north/render/path.hpp"
#include "north/render/path_tessellator.hpp"
#include "north/style/style.hpp"
#include "north/style/theme.hpp"
#include "north/text/font.hpp"
#include "north/text/paragraph.hpp"
#include "north/text/text_buffer.hpp"

#include <memory>
#include <string>

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
    Image
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

struct ButtonData {
    WidgetKind kind{WidgetKind::Button};
    core::Signal<ButtonState> state{ButtonState::Normal};
    ClickCallbackFn on_click{nullptr};
    void* click_user_data{nullptr};
    style::BoxStyle normal_style;
    style::BoxStyle hover_style;
    style::BoxStyle pressed_style;
    core::Color text_color{1.0f, 1.0f, 1.0f, 1.0f};
    std::string label;
    std::shared_ptr<text::Font> font;
    float font_size{14.0f};
    core::Edges padding{8.0f, 16.0f, 8.0f, 16.0f};
    std::unique_ptr<text::Paragraph> paragraph;
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    animation::AnimatedValue<core::Color> animated_bg;
    animation::AnimatedValue<core::Color> animated_border;
    animation::AnimatedValue<core::Color> animated_text;
    core::Signal<style::Theme>* theme_signal{nullptr};

    ButtonData() = default;
    ButtonData(ButtonData&&) noexcept = default;
    ButtonData& operator=(ButtonData&&) noexcept = default;
    ~ButtonData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    ButtonData(const ButtonData& other)
        : kind(other.kind), state(other.state.get()), on_click(other.on_click),
          click_user_data(other.click_user_data), normal_style(other.normal_style),
          hover_style(other.hover_style), pressed_style(other.pressed_style),
          text_color(other.text_color), label(other.label), font(other.font),
          font_size(other.font_size), padding(other.padding), tree(other.tree),
          node_handle(other.node_handle), theme_signal(other.theme_signal) {}

    void apply_theme(const style::Theme& theme);
    void update_animation_targets(float duration = 0.12f);
};

struct TextInputData {
    WidgetKind kind{WidgetKind::TextInput};
    text::TextBuffer buffer;
    std::string placeholder;
    std::string composition_text;
    int composition_cursor{0};
    std::shared_ptr<text::Font> font;
    float font_size{16.0f};
    style::BoxStyle normal_style;
    style::BoxStyle focus_style;
    core::Color text_color{1.0f, 1.0f, 1.0f, 1.0f};
    core::Color placeholder_color{0.6f, 0.6f, 0.6f, 1.0f};
    core::Color selection_color{0.22f, 0.55f, 0.99f, 0.50f};
    core::Color cursor_color{1.0f, 1.0f, 1.0f, 1.0f};
    core::Edges padding{8.0f, 12.0f, 8.0f, 12.0f};
    float width{-1.0f};
    float height{-1.0f};
    bool is_focused{false};
    bool cursor_visible{true};
    float scroll_offset{0.0f};
    void (*on_change)(std::string_view text, void* user_data){nullptr};
    void* change_user_data{nullptr};
    void (*on_submit)(std::string_view text, void* user_data){nullptr};
    void* submit_user_data{nullptr};
    std::unique_ptr<text::Paragraph> paragraph;
    std::unique_ptr<text::Paragraph> placeholder_paragraph;
    core::Signal<style::Theme>* theme_signal{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    animation::AnimatedValue<core::Color> animated_bg;
    animation::AnimatedValue<core::Color> animated_border;
    animation::AnimatedValue<core::Color> animated_text;

    TextInputData() = default;
    TextInputData(TextInputData&&) noexcept = default;
    TextInputData& operator=(TextInputData&&) noexcept = default;
    ~TextInputData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    TextInputData(const TextInputData& other)
        : kind(other.kind), buffer(other.buffer), placeholder(other.placeholder),
          composition_text(other.composition_text), composition_cursor(other.composition_cursor),
          font(other.font), font_size(other.font_size), normal_style(other.normal_style),
          focus_style(other.focus_style), text_color(other.text_color),
          placeholder_color(other.placeholder_color), selection_color(other.selection_color),
          cursor_color(other.cursor_color), padding(other.padding), width(other.width),
          height(other.height), is_focused(other.is_focused), cursor_visible(other.cursor_visible),
          scroll_offset(other.scroll_offset), on_change(other.on_change),
          change_user_data(other.change_user_data), on_submit(other.on_submit),
          submit_user_data(other.submit_user_data), theme_signal(other.theme_signal),
          tree(other.tree), node_handle(other.node_handle) {}

    void apply_theme(const style::Theme& theme);
};

enum class ScrollDirection : uint8_t { Vertical, Horizontal, Both };

struct ScrollbarStyle {
    float thickness{4.0f};
    float hover_thickness{8.0f};
    float min_thumb_length{24.0f};
    core::Color track_color{0.0f, 0.0f, 0.0f, 0.0f};
    core::Color thumb_color{1.0f, 1.0f, 1.0f, 0.45f};
    core::Color thumb_hover_color{1.0f, 1.0f, 1.0f, 0.70f};
    core::Color thumb_drag_color{1.0f, 1.0f, 1.0f, 0.90f};

    [[nodiscard]] static ScrollbarStyle from_theme(const style::Theme& theme) noexcept {
        const auto& c = theme.colors();
        ScrollbarStyle style;
        style.track_color = core::Color{0.0f, 0.0f, 0.0f, 0.0f};
        style.thumb_color = core::Color{c.on_surface.r, c.on_surface.g, c.on_surface.b, 0.45f};
        style.thumb_hover_color =
            core::Color{c.on_surface.r, c.on_surface.g, c.on_surface.b, 0.70f};
        style.thumb_drag_color = core::Color{c.on_surface.r, c.on_surface.g, c.on_surface.b, 0.90f};
        return style;
    }
};

struct ScrollAreaData {
    WidgetKind kind{WidgetKind::ScrollArea};
    ScrollDirection direction{ScrollDirection::Vertical};
    core::Point scroll_offset{0.0f, 0.0f};
    core::Point target_scroll{0.0f, 0.0f};
    core::Point max_scroll{0.0f, 0.0f};
    core::Size content_size{0.0f, 0.0f};
    core::Size viewport_size{0.0f, 0.0f};
    ScrollbarStyle scrollbar_style{};
    bool smooth_scroll{false};
    bool is_hovered{false};
    bool is_v_thumb_hovered{false};
    bool is_h_thumb_hovered{false};
    bool is_v_thumb_dragging{false};
    bool is_h_thumb_dragging{false};
    float drag_start_pointer_y{0.0f};
    float drag_start_scroll_y{0.0f};
    float drag_start_pointer_x{0.0f};
    float drag_start_scroll_x{0.0f};
    void (*on_scroll)(core::Point offset, void* user_data){nullptr};
    void* scroll_user_data{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    animation::AnimatedValue<core::Point> animated_scroll;
    animation::AnimatedValue<float> animated_v_thickness;
    animation::AnimatedValue<float> animated_h_thickness;
    animation::AnimatedValue<core::Color> animated_thumb_color;
    core::Signal<style::Theme>* theme_signal{nullptr};

    ScrollAreaData() = default;
    ScrollAreaData(ScrollAreaData&&) noexcept = default;
    ScrollAreaData& operator=(ScrollAreaData&&) noexcept = default;
    ~ScrollAreaData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    ScrollAreaData(const ScrollAreaData& other)
        : kind(other.kind), direction(other.direction), scroll_offset(other.scroll_offset),
          target_scroll(other.target_scroll), max_scroll(other.max_scroll),
          content_size(other.content_size), viewport_size(other.viewport_size),
          scrollbar_style(other.scrollbar_style), smooth_scroll(other.smooth_scroll),
          is_hovered(other.is_hovered), is_v_thumb_hovered(other.is_v_thumb_hovered),
          is_h_thumb_hovered(other.is_h_thumb_hovered),
          is_v_thumb_dragging(other.is_v_thumb_dragging),
          is_h_thumb_dragging(other.is_h_thumb_dragging),
          drag_start_pointer_y(other.drag_start_pointer_y),
          drag_start_scroll_y(other.drag_start_scroll_y),
          drag_start_pointer_x(other.drag_start_pointer_x),
          drag_start_scroll_x(other.drag_start_scroll_x), on_scroll(other.on_scroll),
          scroll_user_data(other.scroll_user_data), tree(other.tree),
          node_handle(other.node_handle), theme_signal(other.theme_signal) {}

    void apply_theme(const style::Theme& theme);
};

using ItemBuilderFn = core::Handle<core::Node> (*)(core::NodeTree& tree, size_t index,
                                                   void* user_data);
using ItemExtentFn = float (*)(size_t index, void* user_data);

struct VirtualListData {
    WidgetKind kind{WidgetKind::VirtualList};
    size_t item_count{0};
    float fixed_item_extent{40.0f};
    ItemExtentFn item_extent_fn{nullptr};
    void* extent_user_data{nullptr};
    ItemBuilderFn item_builder{nullptr};
    void* builder_user_data{nullptr};
    core::Point scroll_offset{0.0f, 0.0f};
    float target_scroll_y{0.0f};
    core::Point max_scroll{0.0f, 0.0f};
    core::Size viewport_size{0.0f, 0.0f};
    size_t first_visible_idx{0};
    size_t last_visible_idx{0};
    bool window_initialized{false};
    ScrollbarStyle scrollbar_style{};
    bool smooth_scroll{false};
    bool is_hovered{false};
    bool is_thumb_hovered{false};
    bool is_thumb_dragging{false};
    float drag_start_pointer_y{0.0f};
    float drag_start_scroll_y{0.0f};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    animation::AnimatedValue<float> animated_scroll_y;
    animation::AnimatedValue<float> animated_thickness;
    animation::AnimatedValue<core::Color> animated_thumb_color;
    core::Signal<style::Theme>* theme_signal{nullptr};

    VirtualListData() = default;
    VirtualListData(VirtualListData&&) noexcept = default;
    VirtualListData& operator=(VirtualListData&&) noexcept = default;
    ~VirtualListData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    VirtualListData(const VirtualListData& other)
        : kind(other.kind), item_count(other.item_count),
          fixed_item_extent(other.fixed_item_extent), item_extent_fn(other.item_extent_fn),
          extent_user_data(other.extent_user_data), item_builder(other.item_builder),
          builder_user_data(other.builder_user_data), scroll_offset(other.scroll_offset),
          target_scroll_y(other.target_scroll_y), max_scroll(other.max_scroll),
          viewport_size(other.viewport_size), first_visible_idx(other.first_visible_idx),
          last_visible_idx(other.last_visible_idx), window_initialized(other.window_initialized),
          scrollbar_style(other.scrollbar_style),
          smooth_scroll(other.smooth_scroll), is_hovered(other.is_hovered),
          is_thumb_hovered(other.is_thumb_hovered), is_thumb_dragging(other.is_thumb_dragging),
          drag_start_pointer_y(other.drag_start_pointer_y),
          drag_start_scroll_y(other.drag_start_scroll_y), tree(other.tree),
          node_handle(other.node_handle), theme_signal(other.theme_signal) {}

    void apply_theme(const style::Theme& theme);
};

struct CheckboxData {
    WidgetKind kind{WidgetKind::Checkbox};
    bool checked{false};
    core::Signal<bool>* bound_signal{nullptr};
    core::Signal<bool> internal_signal{false};
    animation::AnimatedValue<float> check_progress{0.0f};
    animation::AnimatedValue<core::Color> animated_bg;
    animation::AnimatedValue<core::Color> animated_border;
    std::string label;
    std::shared_ptr<text::Font> font;
    float font_size{14.0f};
    core::Color text_color{1.0f, 1.0f, 1.0f, 1.0f};
    core::Color box_color;
    core::Color check_color;
    core::Color focus_ring_color;
    bool is_hovered{false};
    bool is_focused{false};
    void (*on_change)(bool checked, void* user_data){nullptr};
    void* change_user_data{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    std::unique_ptr<text::Paragraph> paragraph;
    core::Signal<style::Theme>* theme_signal{nullptr};

    CheckboxData() = default;
    CheckboxData(CheckboxData&&) noexcept = default;
    CheckboxData& operator=(CheckboxData&&) noexcept = default;
    ~CheckboxData() {
        if (bound_signal) {
            bound_signal->unbind_dirty(this);
        }
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    CheckboxData(const CheckboxData& other)
        : kind(other.kind), checked(other.checked), bound_signal(other.bound_signal),
          internal_signal(other.internal_signal.get()), label(other.label), font(other.font),
          font_size(other.font_size), text_color(other.text_color), box_color(other.box_color),
          check_color(other.check_color), focus_ring_color(other.focus_ring_color),
          is_hovered(other.is_hovered), is_focused(other.is_focused), on_change(other.on_change),
          change_user_data(other.change_user_data), tree(other.tree),
          node_handle(other.node_handle), theme_signal(other.theme_signal) {}

    void toggle();
    void update_targets();
    void apply_theme(const style::Theme& theme);
};

struct SwitchData {
    WidgetKind kind{WidgetKind::Switch};
    bool checked{false};
    core::Signal<bool>* bound_signal{nullptr};
    core::Signal<bool> internal_signal{false};
    animation::AnimatedValue<float> thumb_position{0.0f};
    animation::AnimatedValue<core::Color> track_color;
    core::Color track_off_color;
    core::Color track_on_color;
    core::Color thumb_color;
    core::Color focus_ring_color;
    std::string label;
    std::shared_ptr<text::Font> font;
    float font_size{14.0f};
    core::Color text_color{1.0f, 1.0f, 1.0f, 1.0f};
    bool is_hovered{false};
    bool is_focused{false};
    void (*on_change)(bool checked, void* user_data){nullptr};
    void* change_user_data{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    std::unique_ptr<text::Paragraph> paragraph;
    core::Signal<style::Theme>* theme_signal{nullptr};

    SwitchData() = default;
    SwitchData(SwitchData&&) noexcept = default;
    SwitchData& operator=(SwitchData&&) noexcept = default;
    ~SwitchData() {
        if (bound_signal) {
            bound_signal->unbind_dirty(this);
        }
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    SwitchData(const SwitchData& other)
        : kind(other.kind), checked(other.checked), bound_signal(other.bound_signal),
          internal_signal(other.internal_signal.get()), track_off_color(other.track_off_color),
          track_on_color(other.track_on_color), thumb_color(other.thumb_color),
          focus_ring_color(other.focus_ring_color), label(other.label), font(other.font),
          font_size(other.font_size), text_color(other.text_color), is_hovered(other.is_hovered),
          is_focused(other.is_focused), on_change(other.on_change),
          change_user_data(other.change_user_data), tree(other.tree),
          node_handle(other.node_handle), theme_signal(other.theme_signal) {}

    void toggle();
    void update_targets();
    void apply_theme(const style::Theme& theme);
};

struct SliderData {
    WidgetKind kind{WidgetKind::Slider};
    float value{0.0f};
    float min_value{0.0f};
    float max_value{100.0f};
    float step{0.0f};
    core::Signal<float>* bound_signal{nullptr};
    core::Signal<float> internal_signal{0.0f};
    animation::AnimatedValue<float> thumb_radius{8.0f};
    core::Color track_inactive_color;
    core::Color track_active_color;
    core::Color thumb_color;
    core::Color focus_ring_color;
    float width{200.0f};
    float height{24.0f};
    bool is_dragging{false};
    bool is_hovered{false};
    bool is_focused{false};
    void (*on_change)(float value, void* user_data){nullptr};
    void* change_user_data{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    core::Signal<style::Theme>* theme_signal{nullptr};
    animation::AnimatedValue<core::Color> animated_track_inactive;
    animation::AnimatedValue<core::Color> animated_track_active;
    animation::AnimatedValue<core::Color> animated_thumb;

    SliderData() = default;
    SliderData(SliderData&&) noexcept = default;
    SliderData& operator=(SliderData&&) noexcept = default;
    ~SliderData() {
        if (bound_signal) {
            bound_signal->unbind_dirty(this);
        }
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    SliderData(const SliderData& other)
        : kind(other.kind), value(other.value), min_value(other.min_value),
          max_value(other.max_value), step(other.step), bound_signal(other.bound_signal),
          internal_signal(other.internal_signal.get()),
          track_inactive_color(other.track_inactive_color),
          track_active_color(other.track_active_color), thumb_color(other.thumb_color),
          focus_ring_color(other.focus_ring_color), width(other.width), height(other.height),
          is_dragging(other.is_dragging), is_hovered(other.is_hovered),
          is_focused(other.is_focused), on_change(other.on_change),
          change_user_data(other.change_user_data), tree(other.tree),
          node_handle(other.node_handle), theme_signal(other.theme_signal) {}

    void set_value(float new_val);
    void seek_from_pointer(float local_x);
    void apply_theme(const style::Theme& theme);
};

struct RadioButtonData {
    WidgetKind kind{WidgetKind::RadioButton};
    bool selected{false};
    int value{0};
    core::Signal<int>* group_signal{nullptr};
    core::Signal<bool>* bound_signal{nullptr};
    core::Signal<bool> internal_signal{false};
    animation::AnimatedValue<float> dot_scale{0.0f};
    animation::AnimatedValue<core::Color> ring_color;
    core::Color ring_inactive_color;
    core::Color ring_active_color;
    core::Color dot_color;
    core::Color focus_ring_color;
    std::string label;
    std::shared_ptr<text::Font> font;
    float font_size{14.0f};
    core::Color text_color{1.0f, 1.0f, 1.0f, 1.0f};
    bool is_hovered{false};
    bool is_focused{false};
    void (*on_select)(int value, void* user_data){nullptr};
    void* select_user_data{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    std::unique_ptr<text::Paragraph> paragraph;
    core::Signal<style::Theme>* theme_signal{nullptr};

    RadioButtonData() = default;
    RadioButtonData(RadioButtonData&&) noexcept = default;
    RadioButtonData& operator=(RadioButtonData&&) noexcept = default;
    ~RadioButtonData() {
        if (group_signal) {
            group_signal->unbind_dirty(this);
        }
        if (bound_signal) {
            bound_signal->unbind_dirty(this);
        }
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    RadioButtonData(const RadioButtonData& other)
        : kind(other.kind), selected(other.selected), value(other.value),
          group_signal(other.group_signal), bound_signal(other.bound_signal),
          internal_signal(other.internal_signal.get()),
          ring_inactive_color(other.ring_inactive_color),
          ring_active_color(other.ring_active_color), dot_color(other.dot_color),
          focus_ring_color(other.focus_ring_color), label(other.label), font(other.font),
          font_size(other.font_size), text_color(other.text_color), is_hovered(other.is_hovered),
          is_focused(other.is_focused), on_select(other.on_select),
          select_user_data(other.select_user_data), tree(other.tree),
          node_handle(other.node_handle), theme_signal(other.theme_signal) {}

    void select();
    void update_targets();
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
