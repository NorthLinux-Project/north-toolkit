#pragma once

#include "north/core/tree.hpp"
#include "north/platform/window_listener.hpp"

namespace north::platform::win32 {
class Win32Window;
}

namespace north::overlay {
class OverlayManager;
}

namespace north::event {

using AcceleratorCallbackFn = void (*)(void* user_data);

struct KeyShortcut {
    uint32_t key_code{0};
    uint32_t modifiers{0};

    [[nodiscard]] constexpr bool operator==(const KeyShortcut& other) const noexcept = default;
};

struct AcceleratorEntry {
    KeyShortcut shortcut;
    AcceleratorCallbackFn callback{nullptr};
    void* user_data{nullptr};
};

class InputDispatcher : public platform::IWindowListener {
public:
    explicit InputDispatcher(core::NodeTree& tree) noexcept;
    ~InputDispatcher() override = default;

    [[nodiscard]] core::Handle<core::Node> hit_test(core::Point screen_point);
    [[nodiscard]] core::Handle<core::Node> find_interactive_target(core::Handle<core::Node> hit);

    void set_focus(core::Handle<core::Node> node);
    void clear_focus();
    [[nodiscard]] core::Handle<core::Node> focused_node() const noexcept { return focused_node_; }

    void reset() noexcept {
        hovered_node_ = {};
        pressed_node_ = {};
        focused_node_ = {};
        scrollbar_drag_node_ = {};
    }

    void set_window(platform::win32::Win32Window* window) noexcept { window_ = window; }
    void set_overlay_manager(overlay::OverlayManager* mgr) noexcept { overlay_manager_ = mgr; }
    [[nodiscard]] overlay::OverlayManager* overlay_manager() const noexcept {
        return overlay_manager_;
    }

    void on_pointer(const platform::PointerEvent& event) override;
    void on_scroll(float delta_x, float delta_y, core::Point pointer_pos) override;
    void on_resize(core::Size new_size) override;
    void on_dpi_changed(float dpi_scale, core::Rect suggested_rect) override;
    void on_key(const platform::KeyEvent& event) override;
    void on_ime_composition(std::string_view comp_str, int cursor_pos) override;
    void on_ime_commit(std::string_view result_str) override;
    void on_close() override;

    void set_fallback_listener(platform::IWindowListener* listener) noexcept {
        fallback_listener_ = listener;
    }

    void register_accelerator(KeyShortcut shortcut, AcceleratorCallbackFn callback, void* user_data = nullptr);
    void unregister_accelerator(KeyShortcut shortcut);
    void clear_accelerators();

    [[nodiscard]] core::Handle<core::Node> hovered_node() const noexcept { return hovered_node_; }
    [[nodiscard]] core::Handle<core::Node> pressed_node() const noexcept { return pressed_node_; }

private:
    core::Handle<core::Node> hit_test_recursive(core::Handle<core::Node> handle,
                                                core::Point current_origin,
                                                core::Point screen_point);
    void cycle_focus(bool reverse);
    void update_ime_window_position();

    core::NodeTree& tree_;
    std::vector<AcceleratorEntry> accelerators_;
    core::Handle<core::Node> hovered_node_{};
    core::Handle<core::Node> pressed_node_{};
    core::Handle<core::Node> focused_node_{};
    core::Handle<core::Node> scrollbar_drag_node_{};
    bool scrollbar_drag_is_horizontal_{false};
    float scrollbar_drag_start_ptr_{0.0f};
    float scrollbar_drag_start_scroll_{0.0f};
    platform::win32::Win32Window* window_{nullptr};
    overlay::OverlayManager* overlay_manager_{nullptr};
    platform::IWindowListener* fallback_listener_{nullptr};
};

} // namespace north::event
