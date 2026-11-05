#include "north/event/input_dispatcher.hpp"

#include "north/overlay/overlay_manager.hpp"
#include "north/platform/win32/win32_window.hpp"
#include "north/text/shaper.hpp"
#include "north/widget/widget_data.hpp"

#include <algorithm>
#include <vector>

namespace north::event {

InputDispatcher::InputDispatcher(core::NodeTree& tree) noexcept : tree_(tree) {}

core::Handle<core::Node> InputDispatcher::hit_test_recursive(core::Handle<core::Node> handle,
                                                             core::Point current_origin,
                                                             core::Point screen_point) {
    auto* node = tree_.get(handle);
    if (!node) {
        return {};
    }

    core::Point node_origin = current_origin + node->layout.offset;
    core::Rect bounds{node_origin.x, node_origin.y, node->layout.size.width,
                      node->layout.size.height};

    bool is_hit = false;
    if (node->hit_test_fn) {
        core::Point local_pt = screen_point - node_origin;
        is_hit = node->hit_test_fn(tree_, handle, local_pt, node->user_data);
    } else {
        is_hit = bounds.contains(screen_point);
    }

    if (!is_hit) {
        return {};
    }

    for (auto it = node->children.rbegin(); it != node->children.rend(); ++it) {
        auto hit_child = hit_test_recursive(*it, node_origin, screen_point);
        if (hit_child.is_valid()) {
            return hit_child;
        }
    }

    return handle;
}

core::Handle<core::Node> InputDispatcher::hit_test(core::Point screen_point) {
    if (overlay_manager_ && overlay_manager_->has_active_overlays()) {
        auto entries = overlay_manager_->entries();
        for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
            if (!it->active || !it->content_node.is_valid()) {
                continue;
            }
            if (it->computed_bounds.contains(screen_point)) {
                auto hit = hit_test_recursive(
                    it->content_node, core::Point{it->computed_bounds.x, it->computed_bounds.y},
                    screen_point);
                if (hit.is_valid()) {
                    return hit;
                }
            }
            if (it->config.modal) {
                return {};
            }
        }
    }

    if (!tree_.root().is_valid()) {
        return {};
    }
    return hit_test_recursive(tree_.root(), {0.0f, 0.0f}, screen_point);
}

core::Handle<core::Node> InputDispatcher::find_interactive_target(core::Handle<core::Node> hit) {
    core::Handle<core::Node> curr = hit;
    while (curr.is_valid()) {
        auto* n = tree_.get(curr);
        if (!n) {
            break;
        }
        if (n->user_data) {
            const auto* kind = reinterpret_cast<const widget::WidgetKind*>(n->user_data);
            if (*kind == widget::WidgetKind::Button || *kind == widget::WidgetKind::TextInput ||
                *kind == widget::WidgetKind::TextArea ||
                *kind == widget::WidgetKind::Checkbox || *kind == widget::WidgetKind::Switch ||
                *kind == widget::WidgetKind::Slider || *kind == widget::WidgetKind::RadioButton ||
                *kind == widget::WidgetKind::TabBar ||
                *kind == widget::WidgetKind::SegmentedButton ||
                *kind == widget::WidgetKind::Accordion ||
                *kind == widget::WidgetKind::SplitView ||
                *kind == widget::WidgetKind::TreeView ||
                *kind == widget::WidgetKind::DataTable ||
                *kind == widget::WidgetKind::CommandPalette ||
                *kind == widget::WidgetKind::Select ||
                *kind == widget::WidgetKind::ComboBox) {
                return curr;
            }
        }
        curr = n->parent;
    }
    return hit;
}

void InputDispatcher::set_focus(core::Handle<core::Node> node) {
    if (focused_node_ == node) {
        return;
    }

    if (focused_node_.is_valid()) {
        auto* prev_n = tree_.get(focused_node_);
        if (prev_n && prev_n->user_data) {
            auto* kind = reinterpret_cast<widget::WidgetKind*>(prev_n->user_data);
            if (*kind == widget::WidgetKind::TextInput) {
                auto* ti = reinterpret_cast<widget::TextInputData*>(prev_n->user_data);
                ti->is_focused = false;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::TextArea) {
                auto* ta = reinterpret_cast<widget::TextAreaData*>(prev_n->user_data);
                ta->is_focused = false;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::Checkbox) {
                auto* cb = reinterpret_cast<widget::CheckboxData*>(prev_n->user_data);
                cb->is_focused = false;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::Switch) {
                auto* sw = reinterpret_cast<widget::SwitchData*>(prev_n->user_data);
                sw->is_focused = false;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::Slider) {
                auto* sl = reinterpret_cast<widget::SliderData*>(prev_n->user_data);
                sl->is_focused = false;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::RadioButton) {
                auto* rb = reinterpret_cast<widget::RadioButtonData*>(prev_n->user_data);
                rb->is_focused = false;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::TabBar) {
                auto* tb = reinterpret_cast<widget::TabBarData*>(prev_n->user_data);
                tb->is_focused = false;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::SegmentedButton) {
                auto* sb = reinterpret_cast<widget::SegmentedButtonData*>(prev_n->user_data);
                sb->is_focused = false;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::Accordion) {
                auto* acc = reinterpret_cast<widget::AccordionData*>(prev_n->user_data);
                acc->is_focused = false;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::TreeView) {
                auto* tv = reinterpret_cast<widget::TreeViewData*>(prev_n->user_data);
                tv->is_focused = false;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::DataTable) {
                auto* dt = reinterpret_cast<widget::DataTableData*>(prev_n->user_data);
                dt->is_focused = false;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::Select) {
                auto* sel = reinterpret_cast<widget::SelectData*>(prev_n->user_data);
                sel->is_focused = false;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::ComboBox) {
                auto* cb = reinterpret_cast<widget::ComboBoxData*>(prev_n->user_data);
                cb->is_focused = false;
                tree_.mark_dirty(focused_node_, core::kPaint);
            }
        }
    }

    focused_node_ = node;

    if (focused_node_.is_valid()) {
        auto* new_n = tree_.get(focused_node_);
        if (new_n && new_n->user_data) {
            auto* kind = reinterpret_cast<widget::WidgetKind*>(new_n->user_data);
            if (*kind == widget::WidgetKind::TextInput) {
                auto* ti = reinterpret_cast<widget::TextInputData*>(new_n->user_data);
                ti->is_focused = true;
                tree_.mark_dirty(focused_node_, core::kPaint);
                update_ime_window_position();
            } else if (*kind == widget::WidgetKind::TextArea) {
                auto* ta = reinterpret_cast<widget::TextAreaData*>(new_n->user_data);
                ta->is_focused = true;
                tree_.mark_dirty(focused_node_, core::kPaint);
                update_ime_window_position();
            } else if (*kind == widget::WidgetKind::Checkbox) {
                auto* cb = reinterpret_cast<widget::CheckboxData*>(new_n->user_data);
                cb->is_focused = true;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::Switch) {
                auto* sw = reinterpret_cast<widget::SwitchData*>(new_n->user_data);
                sw->is_focused = true;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::Slider) {
                auto* sl = reinterpret_cast<widget::SliderData*>(new_n->user_data);
                sl->is_focused = true;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::RadioButton) {
                auto* rb = reinterpret_cast<widget::RadioButtonData*>(new_n->user_data);
                rb->is_focused = true;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::TabBar) {
                auto* tb = reinterpret_cast<widget::TabBarData*>(new_n->user_data);
                tb->is_focused = true;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::SegmentedButton) {
                auto* sb = reinterpret_cast<widget::SegmentedButtonData*>(new_n->user_data);
                sb->is_focused = true;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::Accordion) {
                auto* acc = reinterpret_cast<widget::AccordionData*>(new_n->user_data);
                acc->is_focused = true;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::TreeView) {
                auto* tv = reinterpret_cast<widget::TreeViewData*>(new_n->user_data);
                tv->is_focused = true;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::DataTable) {
                auto* dt = reinterpret_cast<widget::DataTableData*>(new_n->user_data);
                dt->is_focused = true;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::CommandPalette) {
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::Select) {
                auto* sel = reinterpret_cast<widget::SelectData*>(new_n->user_data);
                sel->is_focused = true;
                tree_.mark_dirty(focused_node_, core::kPaint);
            } else if (*kind == widget::WidgetKind::ComboBox) {
                auto* cb = reinterpret_cast<widget::ComboBoxData*>(new_n->user_data);
                cb->is_focused = true;
                tree_.mark_dirty(focused_node_, core::kPaint);
            }
        }
    }
}

void InputDispatcher::clear_focus() {
    set_focus({});
}

void InputDispatcher::cycle_focus(bool reverse) {
    std::vector<core::Handle<core::Node>> focusables;

    auto collect = [&](auto& self, core::Handle<core::Node> h) -> void {
        auto* n = tree_.get(h);
        if (!n)
            return;
        if (n->user_data) {
            auto* kind = reinterpret_cast<const widget::WidgetKind*>(n->user_data);
            if (*kind == widget::WidgetKind::TextInput || *kind == widget::WidgetKind::TextArea ||
                *kind == widget::WidgetKind::Button || *kind == widget::WidgetKind::Checkbox ||
                *kind == widget::WidgetKind::Switch || *kind == widget::WidgetKind::Slider ||
                *kind == widget::WidgetKind::RadioButton || *kind == widget::WidgetKind::TabBar ||
                *kind == widget::WidgetKind::SegmentedButton ||
                *kind == widget::WidgetKind::Accordion ||
                *kind == widget::WidgetKind::TreeView ||
                *kind == widget::WidgetKind::DataTable ||
                *kind == widget::WidgetKind::Select ||
                *kind == widget::WidgetKind::ComboBox) {
                focusables.push_back(h);
            }
        }
        for (auto c : n->children) {
            self(self, c);
        }
    };

    if (tree_.root().is_valid()) {
        collect(collect, tree_.root());
    }

    if (focusables.empty()) {
        return;
    }

    size_t idx = 0;
    bool found = false;
    for (size_t i = 0; i < focusables.size(); ++i) {
        if (focusables[i] == focused_node_) {
            idx = i;
            found = true;
            break;
        }
    }

    if (!found) {
        set_focus(reverse ? focusables.back() : focusables.front());
    } else {
        if (reverse) {
            set_focus(focusables[(idx + focusables.size() - 1) % focusables.size()]);
        } else {
            set_focus(focusables[(idx + 1) % focusables.size()]);
        }
    }
}

void InputDispatcher::update_ime_window_position() {
    if (!window_ || !focused_node_.is_valid()) {
        return;
    }

    auto* n = tree_.get(focused_node_);
    if (!n || !n->user_data) {
        return;
    }

    auto* kind = reinterpret_cast<widget::WidgetKind*>(n->user_data);
    if (*kind == widget::WidgetKind::TextArea) {
        auto* ta = reinterpret_cast<widget::TextAreaData*>(n->user_data);
        if (!ta->font) {
            return;
        }
        core::Point origin = tree_.global_offset(focused_node_);
        float line_h = ta->font->line_height(ta->font_size);
        size_t c_pos = ta->editor.cursor_pos();
        size_t c_line = ta->editor.line_of(c_pos);
        size_t l_st = ta->editor.line_start(c_line);
        std::string_view pfx(ta->editor.text().data() + l_st, c_pos - l_st);
        float cursor_x = text::Shaper::shape_text(pfx, ta->font, ta->font_size).total_advance;
        float screen_x = origin.x + ta->padding.left + ta->line_numbers_width + cursor_x - ta->scroll_x;
        float screen_y = origin.y + ta->padding.top + (static_cast<float>(c_line) + 1.0f) * line_h - ta->scroll_y;
        window_->set_ime_position({screen_x, screen_y});
        return;
    }

    if (*kind != widget::WidgetKind::TextInput) {
        return;
    }

    auto* ti = reinterpret_cast<widget::TextInputData*>(n->user_data);
    if (!ti->font) {
        return;
    }

    core::Point origin = tree_.global_offset(focused_node_);
    float line_h = ti->font->line_height(ti->font_size);
    float cursor_x = text::Shaper::shape_text(
                         std::string_view(ti->buffer.text().data(), ti->buffer.cursor_pos()),
                         ti->font, ti->font_size)
                         .total_advance;
    float screen_cursor_x = origin.x + ti->padding.left + cursor_x - ti->scroll_offset;

    window_->set_ime_position({screen_cursor_x, origin.y + ti->padding.top + line_h});
}

void InputDispatcher::on_resize(core::Size new_size) {
    if (new_size.width > 0.0f && new_size.height > 0.0f && tree_.root().is_valid()) {
        tree_.layout(layout::BoxConstraints::tight(new_size));
        update_ime_window_position();
    }
    if (fallback_listener_) {
        fallback_listener_->on_resize(new_size);
    }
}

void InputDispatcher::on_dpi_changed(float dpi_scale, core::Rect suggested_rect) {
    if (suggested_rect.width > 0.0f && suggested_rect.height > 0.0f && tree_.root().is_valid()) {
        tree_.layout(layout::BoxConstraints::tight({suggested_rect.width, suggested_rect.height}));
        update_ime_window_position();
    }
    if (fallback_listener_) {
        fallback_listener_->on_dpi_changed(dpi_scale, suggested_rect);
    }
}

void InputDispatcher::on_close() {
    if (fallback_listener_) {
        fallback_listener_->on_close();
    }
}

void InputDispatcher::register_accelerator(KeyShortcut shortcut, AcceleratorCallbackFn callback, void* user_data) {
    accelerators_.push_back(AcceleratorEntry{shortcut, callback, user_data});
}

void InputDispatcher::unregister_accelerator(KeyShortcut shortcut) {
    accelerators_.erase(std::remove_if(accelerators_.begin(), accelerators_.end(),
                                       [&](const AcceleratorEntry& e) {
                                           return e.shortcut == shortcut;
                                       }),
                        accelerators_.end());
}

void InputDispatcher::clear_accelerators() {
    accelerators_.clear();
}

} // namespace north::event
