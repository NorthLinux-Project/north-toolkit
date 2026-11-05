#pragma once

#include "north/text/text_buffer.hpp"
#include "north/text/text_editor.hpp"
#include "north/widget/data/core_data.hpp"

namespace north::widget {

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

struct TextAreaData;
using TextAreaThemeFn = void (*)(const style::Theme& theme, TextAreaData& data);

struct TextAreaData {
    WidgetKind kind{WidgetKind::TextArea};
    text::TextEditor editor;
    std::string placeholder;
    std::string composition_text;
    int composition_cursor{0};
    std::shared_ptr<text::Font> font;
    float font_size{16.0f};
    bool word_wrap{true};
    bool show_line_numbers{false};
    float line_numbers_width{0.0f};
    style::BoxStyle normal_style;
    style::BoxStyle focus_style;
    core::Color text_color{1.0f, 1.0f, 1.0f, 1.0f};
    core::Color placeholder_color{0.6f, 0.6f, 0.6f, 1.0f};
    core::Color selection_color{0.22f, 0.55f, 0.99f, 0.50f};
    core::Color cursor_color{1.0f, 1.0f, 1.0f, 1.0f};
    core::Color line_number_color{0.5f, 0.5f, 0.5f, 1.0f};
    core::Color line_number_bg{0.12f, 0.12f, 0.14f, 1.0f};
    core::Edges padding{8.0f, 12.0f, 8.0f, 12.0f};
    float width{-1.0f};
    float height{-1.0f};
    bool is_focused{false};
    bool cursor_visible{true};
    float scroll_x{0.0f};
    float scroll_y{0.0f};
    bool is_dragging{false};
    void (*on_change)(std::string_view text, void* user_data){nullptr};
    void* change_user_data{nullptr};
    std::unique_ptr<text::Paragraph> paragraph;
    std::unique_ptr<text::Paragraph> placeholder_paragraph;
    core::Signal<style::Theme>* theme_signal{nullptr};
    core::NodeTree* tree{nullptr};
    core::Handle<core::Node> node_handle{};
    animation::AnimatedValue<core::Color> animated_bg;
    animation::AnimatedValue<core::Color> animated_border;
    animation::AnimatedValue<core::Color> animated_text;

    TextAreaData() = default;
    TextAreaData(TextAreaData&&) noexcept = default;
    TextAreaData& operator=(TextAreaData&&) noexcept = default;
    ~TextAreaData() {
        if (theme_signal) {
            theme_signal->unbind_dirty(this);
        }
    }
    TextAreaData(const TextAreaData& other)
        : kind(other.kind), editor(other.editor), placeholder(other.placeholder),
          composition_text(other.composition_text), composition_cursor(other.composition_cursor),
          font(other.font), font_size(other.font_size), word_wrap(other.word_wrap),
          show_line_numbers(other.show_line_numbers), line_numbers_width(other.line_numbers_width),
          normal_style(other.normal_style), focus_style(other.focus_style),
          text_color(other.text_color), placeholder_color(other.placeholder_color),
          selection_color(other.selection_color), cursor_color(other.cursor_color),
          line_number_color(other.line_number_color), line_number_bg(other.line_number_bg),
          padding(other.padding), width(other.width), height(other.height),
          is_focused(other.is_focused), cursor_visible(other.cursor_visible),
          scroll_x(other.scroll_x), scroll_y(other.scroll_y),
          is_dragging(other.is_dragging), on_change(other.on_change),
          change_user_data(other.change_user_data), theme_signal(other.theme_signal),
          tree(other.tree), node_handle(other.node_handle) {}

    void apply_theme(const style::Theme& theme);
    void update_animation_targets(float duration = 0.12f);
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

} // namespace north::widget
