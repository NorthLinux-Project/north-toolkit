#pragma once

#include "north/core/types.hpp"
#include "north/platform/event.hpp"

#include <string_view>

namespace north::platform {

class IWindowListener {
public:
    virtual ~IWindowListener() = default;

    virtual void on_resize(core::Size new_size) { (void)new_size; }

    virtual void on_dpi_changed(float dpi_scale, core::Rect suggested_rect) {
        (void)dpi_scale;
        (void)suggested_rect;
    }

    virtual void on_pointer(const PointerEvent& event) { (void)event; }

    virtual void on_scroll(float delta_x, float delta_y, core::Point pointer_pos) {
        (void)delta_x;
        (void)delta_y;
        (void)pointer_pos;
    }

    virtual void on_key(const KeyEvent& event) { (void)event; }

    virtual void on_ime_composition(std::string_view comp_str, int cursor_pos) {
        (void)comp_str;
        (void)cursor_pos;
    }

    virtual void on_ime_commit(std::string_view result_str) { (void)result_str; }

    virtual void on_close() {}
};

} // namespace north::platform
