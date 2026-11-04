#include "north/event/input_dispatcher.hpp"

#include "north/platform/win32/win32_window.hpp"
#include "north/text/shaper.hpp"
#include "north/widget/widget_data.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cmath>
#include <vector>

#include <windows.h>

namespace north::event {

namespace {

size_t find_closest_cursor_pos(std::string_view text, const std::shared_ptr<text::Font>& font,
                               float font_size, float target_x) {
    if (text.empty() || target_x <= 0.0f) {
        return 0;
    }

    size_t closest_pos = 0;
    float closest_dist = (std::abs)(target_x);
    size_t pos = 0;

    while (pos < text.size()) {
        size_t next = pos + 1;
        while (next < text.size() && (static_cast<uint8_t>(text[next]) & 0xC0) == 0x80) {
            ++next;
        }
        float adv = text::Shaper::shape_text(std::string_view(text.data(), next), font, font_size)
                        .total_advance;
        float dist = (std::abs)(target_x - adv);
        if (dist < closest_dist) {
            closest_dist = dist;
            closest_pos = next;
        }
        pos = next;
    }

    return closest_pos;
}

} // namespace

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
                *kind == widget::WidgetKind::Checkbox || *kind == widget::WidgetKind::Switch ||
                *kind == widget::WidgetKind::Slider || *kind == widget::WidgetKind::RadioButton) {
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
            if (*kind == widget::WidgetKind::TextInput || *kind == widget::WidgetKind::Button ||
                *kind == widget::WidgetKind::Checkbox || *kind == widget::WidgetKind::Switch ||
                *kind == widget::WidgetKind::Slider || *kind == widget::WidgetKind::RadioButton) {
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
    if (*kind != widget::WidgetKind::TextInput) {
        return;
    }

    auto* ti = reinterpret_cast<widget::TextInputData*>(n->user_data);
    if (!ti->font) {
        return;
    }

    core::Point origin{0.0f, 0.0f};
    core::Handle<core::Node> cur = focused_node_;
    while (cur.is_valid()) {
        auto* pn = tree_.get(cur);
        if (!pn)
            break;
        origin = origin + pn->layout.offset;
        cur = pn->parent;
    }

    float line_h = ti->font->line_height(ti->font_size);
    float cursor_x = text::Shaper::shape_text(
                         std::string_view(ti->buffer.text().data(), ti->buffer.cursor_pos()),
                         ti->font, ti->font_size)
                         .total_advance;
    float screen_cursor_x = origin.x + ti->padding.left + cursor_x - ti->scroll_offset;

    window_->set_ime_position({screen_cursor_x, origin.y + ti->padding.top + line_h});
}

void InputDispatcher::on_pointer(const platform::PointerEvent& event) {
    core::Handle<core::Node> raw_hit = hit_test(event.position);
    core::Handle<core::Node> target = find_interactive_target(raw_hit);

    switch (event.action) {
    case platform::PointerAction::Move: {
        if (scrollbar_drag_node_.is_valid()) {
            auto* n = tree_.get(scrollbar_drag_node_);
            if (n && n->user_data) {
                auto* kind = reinterpret_cast<widget::WidgetKind*>(n->user_data);
                if (*kind == widget::WidgetKind::ScrollArea) {
                    auto* sa = reinterpret_cast<widget::ScrollAreaData*>(n->user_data);
                    const auto& s = sa->scrollbar_style;
                    if (!scrollbar_drag_is_horizontal_) {
                        float track_h = n->layout.size.height;
                        float thumb_h = (std::max)(s.min_thumb_length,
                                                   (track_h / sa->content_size.height) * track_h);
                        float travel = track_h - thumb_h;
                        if (travel > 0.0f) {
                            float delta_p = event.position.y - scrollbar_drag_start_ptr_;
                            float new_scroll = scrollbar_drag_start_scroll_ +
                                               delta_p * (sa->max_scroll.y / travel);
                            new_scroll = std::clamp(new_scroll, 0.0f, sa->max_scroll.y);
                            if (new_scroll != sa->scroll_offset.y) {
                                sa->scroll_offset.y = new_scroll;
                                sa->target_scroll.y = new_scroll;
                                sa->animated_scroll.set_immediate(sa->scroll_offset);
                                tree_.mark_dirty(scrollbar_drag_node_,
                                                 core::kLayout | core::kPaint);
                            }
                        }
                    } else {
                        float track_w = n->layout.size.width;
                        float thumb_w = (std::max)(s.min_thumb_length,
                                                   (track_w / sa->content_size.width) * track_w);
                        float travel = track_w - thumb_w;
                        if (travel > 0.0f) {
                            float delta_p = event.position.x - scrollbar_drag_start_ptr_;
                            float new_scroll = scrollbar_drag_start_scroll_ +
                                               delta_p * (sa->max_scroll.x / travel);
                            new_scroll = std::clamp(new_scroll, 0.0f, sa->max_scroll.x);
                            if (new_scroll != sa->scroll_offset.x) {
                                sa->scroll_offset.x = new_scroll;
                                sa->target_scroll.x = new_scroll;
                                sa->animated_scroll.set_immediate(sa->scroll_offset);
                                tree_.mark_dirty(scrollbar_drag_node_,
                                                 core::kLayout | core::kPaint);
                            }
                        }
                    }
                    return;
                } else if (*kind == widget::WidgetKind::VirtualList) {
                    auto* vl = reinterpret_cast<widget::VirtualListData*>(n->user_data);
                    const auto& s = vl->scrollbar_style;
                    float total_h =
                        (vl->fixed_item_extent > 0.0f)
                            ? (static_cast<float>(vl->item_count) * vl->fixed_item_extent)
                            : 1.0f;
                    float track_h = n->layout.size.height;
                    float thumb_h = (std::max)(s.min_thumb_length, (track_h / total_h) * track_h);
                    float travel = track_h - thumb_h;
                    if (travel > 0.0f) {
                        float delta_p = event.position.y - scrollbar_drag_start_ptr_;
                        float new_scroll =
                            scrollbar_drag_start_scroll_ + delta_p * (vl->max_scroll.y / travel);
                        new_scroll = std::clamp(new_scroll, 0.0f, vl->max_scroll.y);
                        if (new_scroll != vl->scroll_offset.y) {
                            vl->scroll_offset.y = new_scroll;
                            vl->target_scroll_y = new_scroll;
                            vl->animated_scroll_y.set_immediate(new_scroll);
                            tree_.mark_dirty(scrollbar_drag_node_, core::kLayout | core::kPaint);
                        }
                    }
                    return;
                }
            }
        }

        if (pressed_node_.is_valid() && pressed_node_ == focused_node_) {
            auto* pn = tree_.get(pressed_node_);
            if (pn && pn->user_data) {
                auto* kind = reinterpret_cast<widget::WidgetKind*>(pn->user_data);
                if (*kind == widget::WidgetKind::TextInput) {
                    auto* ti = reinterpret_cast<widget::TextInputData*>(pn->user_data);
                    core::Point node_origin{0.0f, 0.0f};
                    core::Handle<core::Node> cur = pressed_node_;
                    while (cur.is_valid()) {
                        auto* p = tree_.get(cur);
                        if (!p)
                            break;
                        node_origin = node_origin + p->layout.offset;
                        cur = p->parent;
                    }
                    float local_x =
                        event.position.x - (node_origin.x + ti->padding.left) + ti->scroll_offset;
                    if (ti->font) {
                        size_t cpos = find_closest_cursor_pos(ti->buffer.text(), ti->font,
                                                              ti->font_size, local_x);
                        ti->buffer.set_cursor_pos(cpos, true);
                        tree_.mark_dirty(pressed_node_, core::kPaint);
                        update_ime_window_position();
                    }
                } else if (*kind == widget::WidgetKind::Slider) {
                    auto* sl = reinterpret_cast<widget::SliderData*>(pn->user_data);
                    if (sl->is_dragging) {
                        core::Point node_origin{0.0f, 0.0f};
                        for (auto cur = pressed_node_; cur.is_valid();) {
                            auto* p = tree_.get(cur);
                            if (!p)
                                break;
                            node_origin = node_origin + p->layout.offset;
                            cur = p->parent;
                        }
                        sl->seek_from_pointer(event.position.x - node_origin.x);
                    }
                }
            }
        }

        if (target != hovered_node_) {
            if (hovered_node_.is_valid()) {
                auto* prev_n = tree_.get(hovered_node_);
                if (prev_n && prev_n->user_data) {
                    auto* kind = reinterpret_cast<widget::WidgetKind*>(prev_n->user_data);
                    if (*kind == widget::WidgetKind::Button) {
                        auto* btn = reinterpret_cast<widget::ButtonData*>(prev_n->user_data);
                        btn->state.set(widget::ButtonState::Normal);
                        tree_.mark_dirty(hovered_node_, core::kPaint);
                    }
                }
            }

            if (target.is_valid()) {
                auto* new_n = tree_.get(target);
                if (new_n && new_n->user_data) {
                    auto* kind = reinterpret_cast<widget::WidgetKind*>(new_n->user_data);
                    if (*kind == widget::WidgetKind::Button) {
                        auto* btn = reinterpret_cast<widget::ButtonData*>(new_n->user_data);
                        btn->state.set(widget::ButtonState::Hovered);
                        tree_.mark_dirty(target, core::kPaint);
                    }
                }
            }
            hovered_node_ = target;
        }

        {
            core::Handle<core::Node> curr_scroll = raw_hit;
            while (curr_scroll.is_valid()) {
                auto* n = tree_.get(curr_scroll);
                if (!n)
                    break;
                if (n->user_data) {
                    auto* kind = reinterpret_cast<widget::WidgetKind*>(n->user_data);
                    if (*kind == widget::WidgetKind::ScrollArea) {
                        auto* sa = reinterpret_cast<widget::ScrollAreaData*>(n->user_data);
                        const auto& s = sa->scrollbar_style;
                        if (sa->max_scroll.y > 0.0f) {
                            core::Point sa_origin{0.0f, 0.0f};
                            for (auto c = curr_scroll; c.is_valid();) {
                                auto* p = tree_.get(c);
                                if (!p)
                                    break;
                                sa_origin = sa_origin + p->layout.offset;
                                c = p->parent;
                            }
                            float track_h = n->layout.size.height;
                            float thumb_h =
                                (std::max)(s.min_thumb_length,
                                           (track_h / sa->content_size.height) * track_h);
                            float thumb_y = sa_origin.y + (sa->scroll_offset.y / sa->max_scroll.y) *
                                                              (track_h - thumb_h);
                            float thickness = s.hover_thickness + 4.0f;
                            float thumb_x = sa_origin.x + n->layout.size.width - thickness;
                            core::Rect hit_rect{thumb_x, thumb_y, thickness, thumb_h};
                            bool is_hov = hit_rect.contains(event.position);
                            if (is_hov != sa->is_v_thumb_hovered) {
                                sa->is_v_thumb_hovered = is_hov;
                                float target_th = (is_hov || sa->is_v_thumb_dragging)
                                                      ? s.hover_thickness
                                                      : s.thickness;
                                sa->animated_v_thickness.tween_to(
                                    target_th, 0.12f, animation::CubicBezier::ease_out());
                            }
                        }
                    } else if (*kind == widget::WidgetKind::VirtualList) {
                        auto* vl = reinterpret_cast<widget::VirtualListData*>(n->user_data);
                        const auto& s = vl->scrollbar_style;
                        if (vl->max_scroll.y > 0.0f) {
                            core::Point vl_origin{0.0f, 0.0f};
                            for (auto c = curr_scroll; c.is_valid();) {
                                auto* p = tree_.get(c);
                                if (!p)
                                    break;
                                vl_origin = vl_origin + p->layout.offset;
                                c = p->parent;
                            }
                            float total_h =
                                (vl->fixed_item_extent > 0.0f)
                                    ? (static_cast<float>(vl->item_count) * vl->fixed_item_extent)
                                    : 1.0f;
                            float track_h = n->layout.size.height;
                            float thumb_h =
                                (std::max)(s.min_thumb_length, (track_h / total_h) * track_h);
                            float thumb_y = vl_origin.y + (vl->scroll_offset.y / vl->max_scroll.y) *
                                                              (track_h - thumb_h);
                            float thickness = s.hover_thickness + 4.0f;
                            float thumb_x = vl_origin.x + n->layout.size.width - thickness;
                            core::Rect hit_rect{thumb_x, thumb_y, thickness, thumb_h};
                            bool is_hov = hit_rect.contains(event.position);
                            if (is_hov != vl->is_thumb_hovered) {
                                vl->is_thumb_hovered = is_hov;
                                float target_th = (is_hov || vl->is_thumb_dragging)
                                                      ? s.hover_thickness
                                                      : s.thickness;
                                vl->animated_thickness.tween_to(target_th, 0.12f,
                                                                animation::CubicBezier::ease_out());
                            }
                        }
                    }
                }
                curr_scroll = n->parent;
            }
        }
        break;
    }

    case platform::PointerAction::Down: {
        if (event.button == platform::PointerButton::Left) {
            core::Handle<core::Node> curr_scroll = raw_hit;
            while (curr_scroll.is_valid()) {
                auto* n = tree_.get(curr_scroll);
                if (!n)
                    break;
                if (n->user_data) {
                    auto* kind = reinterpret_cast<widget::WidgetKind*>(n->user_data);
                    if (*kind == widget::WidgetKind::ScrollArea) {
                        auto* sa = reinterpret_cast<widget::ScrollAreaData*>(n->user_data);
                        core::Point sa_origin{0.0f, 0.0f};
                        for (auto c = curr_scroll; c.is_valid();) {
                            auto* p = tree_.get(c);
                            if (!p)
                                break;
                            sa_origin = sa_origin + p->layout.offset;
                            c = p->parent;
                        }
                        const auto& s = sa->scrollbar_style;
                        float thickness = s.hover_thickness + 4.0f;
                        if (sa->max_scroll.y > 0.0f) {
                            float track_h = n->layout.size.height;
                            float thumb_h =
                                (std::max)(s.min_thumb_length,
                                           (track_h / sa->content_size.height) * track_h);
                            float thumb_y = sa_origin.y + (sa->scroll_offset.y / sa->max_scroll.y) *
                                                              (track_h - thumb_h);
                            float thumb_x = sa_origin.x + n->layout.size.width - thickness;
                            core::Rect thumb_hit_rect{thumb_x, thumb_y, thickness, thumb_h};
                            if (thumb_hit_rect.contains(event.position)) {
                                scrollbar_drag_node_ = curr_scroll;
                                scrollbar_drag_is_horizontal_ = false;
                                scrollbar_drag_start_ptr_ = event.position.y;
                                scrollbar_drag_start_scroll_ = sa->scroll_offset.y;
                                sa->is_v_thumb_dragging = true;
                                sa->animated_v_thickness.tween_to(
                                    s.hover_thickness, 0.12f, animation::CubicBezier::ease_out());
                                tree_.mark_dirty(curr_scroll, core::kPaint);
                                return;
                            }
                        }
                        if (sa->max_scroll.x > 0.0f) {
                            float track_w = n->layout.size.width;
                            float thumb_w =
                                (std::max)(s.min_thumb_length,
                                           (track_w / sa->content_size.width) * track_w);
                            float thumb_x = sa_origin.x + (sa->scroll_offset.x / sa->max_scroll.x) *
                                                              (track_w - thumb_w);
                            float thumb_y = sa_origin.y + n->layout.size.height - thickness;
                            core::Rect thumb_hit_rect{thumb_x, thumb_y, thumb_w, thickness};
                            if (thumb_hit_rect.contains(event.position)) {
                                scrollbar_drag_node_ = curr_scroll;
                                scrollbar_drag_is_horizontal_ = true;
                                scrollbar_drag_start_ptr_ = event.position.x;
                                scrollbar_drag_start_scroll_ = sa->scroll_offset.x;
                                sa->is_h_thumb_dragging = true;
                                tree_.mark_dirty(curr_scroll, core::kPaint);
                                return;
                            }
                        }
                    } else if (*kind == widget::WidgetKind::VirtualList) {
                        auto* vl = reinterpret_cast<widget::VirtualListData*>(n->user_data);
                        core::Point vl_origin{0.0f, 0.0f};
                        for (auto c = curr_scroll; c.is_valid();) {
                            auto* p = tree_.get(c);
                            if (!p)
                                break;
                            vl_origin = vl_origin + p->layout.offset;
                            c = p->parent;
                        }
                        const auto& s = vl->scrollbar_style;
                        float thickness = s.hover_thickness + 4.0f;
                        if (vl->max_scroll.y > 0.0f) {
                            float total_h =
                                (vl->fixed_item_extent > 0.0f)
                                    ? (static_cast<float>(vl->item_count) * vl->fixed_item_extent)
                                    : 1.0f;
                            float track_h = n->layout.size.height;
                            float thumb_h =
                                (std::max)(s.min_thumb_length, (track_h / total_h) * track_h);
                            float thumb_y = vl_origin.y + (vl->scroll_offset.y / vl->max_scroll.y) *
                                                              (track_h - thumb_h);
                            float thumb_x = vl_origin.x + n->layout.size.width - thickness;
                            core::Rect thumb_hit_rect{thumb_x, thumb_y, thickness, thumb_h};
                            if (thumb_hit_rect.contains(event.position)) {
                                scrollbar_drag_node_ = curr_scroll;
                                scrollbar_drag_is_horizontal_ = false;
                                scrollbar_drag_start_ptr_ = event.position.y;
                                scrollbar_drag_start_scroll_ = vl->scroll_offset.y;
                                vl->is_thumb_dragging = true;
                                vl->animated_thickness.tween_to(s.hover_thickness, 0.12f,
                                                                animation::CubicBezier::ease_out());
                                tree_.mark_dirty(curr_scroll, core::kPaint);
                                return;
                            }
                        }
                    }
                }
                curr_scroll = n->parent;
            }

            bool focus_assigned = false;
            if (target.is_valid()) {
                auto* n = tree_.get(target);
                if (n && n->user_data) {
                    auto* kind = reinterpret_cast<widget::WidgetKind*>(n->user_data);
                    if (*kind == widget::WidgetKind::TextInput) {
                        focus_assigned = true;
                        set_focus(target);
                        pressed_node_ = target;
                        auto* ti = reinterpret_cast<widget::TextInputData*>(n->user_data);
                        core::Point node_origin{0.0f, 0.0f};
                        core::Handle<core::Node> cur = target;
                        while (cur.is_valid()) {
                            auto* pn = tree_.get(cur);
                            if (!pn)
                                break;
                            node_origin = node_origin + pn->layout.offset;
                            cur = pn->parent;
                        }
                        float local_x = event.position.x - (node_origin.x + ti->padding.left) +
                                        ti->scroll_offset;
                        if (ti->font) {
                            size_t cpos = find_closest_cursor_pos(ti->buffer.text(), ti->font,
                                                                  ti->font_size, local_x);
                            ti->buffer.set_cursor_pos(cpos,
                                                      (event.modifiers & platform::kModShift) != 0);
                            tree_.mark_dirty(target, core::kPaint);
                            update_ime_window_position();
                        }
                    } else if (*kind == widget::WidgetKind::Button) {
                        focus_assigned = true;
                        set_focus(target);
                        pressed_node_ = target;
                        auto* btn = reinterpret_cast<widget::ButtonData*>(n->user_data);
                        btn->state.set(widget::ButtonState::Pressed);
                        tree_.mark_dirty(target, core::kPaint);
                    } else if (*kind == widget::WidgetKind::Checkbox) {
                        focus_assigned = true;
                        set_focus(target);
                        pressed_node_ = target;
                    } else if (*kind == widget::WidgetKind::Switch) {
                        focus_assigned = true;
                        set_focus(target);
                        pressed_node_ = target;
                    } else if (*kind == widget::WidgetKind::Slider) {
                        focus_assigned = true;
                        set_focus(target);
                        pressed_node_ = target;
                        auto* sl = reinterpret_cast<widget::SliderData*>(n->user_data);
                        sl->is_dragging = true;
                        sl->thumb_radius.tween_to(10.0f, 0.12f, animation::CubicBezier::ease_out());
                        core::Point node_origin{0.0f, 0.0f};
                        for (auto c = target; c.is_valid();) {
                            auto* p = tree_.get(c);
                            if (!p)
                                break;
                            node_origin = node_origin + p->layout.offset;
                            c = p->parent;
                        }
                        sl->seek_from_pointer(event.position.x - node_origin.x);
                    } else if (*kind == widget::WidgetKind::RadioButton) {
                        focus_assigned = true;
                        set_focus(target);
                        pressed_node_ = target;
                    }
                }
            }
            if (!focus_assigned) {
                clear_focus();
            }
        }
        break;
    }

    case platform::PointerAction::Up: {
        if (scrollbar_drag_node_.is_valid()) {
            auto* n = tree_.get(scrollbar_drag_node_);
            if (n && n->user_data) {
                auto* kind = reinterpret_cast<widget::WidgetKind*>(n->user_data);
                if (*kind == widget::WidgetKind::ScrollArea) {
                    auto* sa = reinterpret_cast<widget::ScrollAreaData*>(n->user_data);
                    sa->is_v_thumb_dragging = false;
                    sa->is_h_thumb_dragging = false;
                    float target_v_th = sa->is_v_thumb_hovered ? sa->scrollbar_style.hover_thickness
                                                               : sa->scrollbar_style.thickness;
                    sa->animated_v_thickness.tween_to(target_v_th, 0.12f,
                                                      animation::CubicBezier::ease_out());
                    tree_.mark_dirty(scrollbar_drag_node_, core::kPaint);
                } else if (*kind == widget::WidgetKind::VirtualList) {
                    auto* vl = reinterpret_cast<widget::VirtualListData*>(n->user_data);
                    vl->is_thumb_dragging = false;
                    float target_vl_th = vl->is_thumb_hovered ? vl->scrollbar_style.hover_thickness
                                                              : vl->scrollbar_style.thickness;
                    vl->animated_thickness.tween_to(target_vl_th, 0.12f,
                                                    animation::CubicBezier::ease_out());
                    tree_.mark_dirty(scrollbar_drag_node_, core::kPaint);
                }
            }
            scrollbar_drag_node_ = {};
        }

        if (event.button == platform::PointerButton::Left && pressed_node_.is_valid()) {
            auto pressed = pressed_node_;
            pressed_node_ = {};

            auto* n = tree_.get(pressed);
            if (n && n->user_data) {
                auto* kind = reinterpret_cast<widget::WidgetKind*>(n->user_data);
                bool is_same = (target == pressed);
                if (*kind == widget::WidgetKind::Button) {
                    auto* btn = reinterpret_cast<widget::ButtonData*>(n->user_data);
                    auto on_click = btn->on_click;
                    void* user_data = btn->click_user_data;

                    btn->state.set(is_same ? widget::ButtonState::Hovered
                                           : widget::ButtonState::Normal);
                    tree_.mark_dirty(pressed, core::kPaint);

                    if (is_same && on_click) {
                        on_click(user_data);
                    }
                } else if (*kind == widget::WidgetKind::Checkbox) {
                    if (is_same) {
                        auto* cb = reinterpret_cast<widget::CheckboxData*>(n->user_data);
                        cb->toggle();
                    }
                } else if (*kind == widget::WidgetKind::Switch) {
                    if (is_same) {
                        auto* sw = reinterpret_cast<widget::SwitchData*>(n->user_data);
                        sw->toggle();
                    }
                } else if (*kind == widget::WidgetKind::Slider) {
                    auto* sl = reinterpret_cast<widget::SliderData*>(n->user_data);
                    sl->is_dragging = false;
                    sl->thumb_radius.tween_to(sl->is_hovered ? 10.0f : 8.0f, 0.12f,
                                              animation::CubicBezier::ease_out());
                } else if (*kind == widget::WidgetKind::RadioButton) {
                    if (is_same) {
                        auto* rb = reinterpret_cast<widget::RadioButtonData*>(n->user_data);
                        rb->select();
                    }
                }
            }
        }
        break;
    }

    case platform::PointerAction::Cancel: {
        if (scrollbar_drag_node_.is_valid()) {
            auto* n = tree_.get(scrollbar_drag_node_);
            if (n && n->user_data) {
                auto* kind = reinterpret_cast<widget::WidgetKind*>(n->user_data);
                if (*kind == widget::WidgetKind::ScrollArea) {
                    auto* sa = reinterpret_cast<widget::ScrollAreaData*>(n->user_data);
                    sa->is_v_thumb_dragging = false;
                    sa->is_h_thumb_dragging = false;
                    float target_v_th = sa->is_v_thumb_hovered ? sa->scrollbar_style.hover_thickness
                                                               : sa->scrollbar_style.thickness;
                    sa->animated_v_thickness.tween_to(target_v_th, 0.12f,
                                                      animation::CubicBezier::ease_out());
                    tree_.mark_dirty(scrollbar_drag_node_, core::kPaint);
                } else if (*kind == widget::WidgetKind::VirtualList) {
                    auto* vl = reinterpret_cast<widget::VirtualListData*>(n->user_data);
                    vl->is_thumb_dragging = false;
                    float target_vl_th = vl->is_thumb_hovered ? vl->scrollbar_style.hover_thickness
                                                              : vl->scrollbar_style.thickness;
                    vl->animated_thickness.tween_to(target_vl_th, 0.12f,
                                                    animation::CubicBezier::ease_out());
                    tree_.mark_dirty(scrollbar_drag_node_, core::kPaint);
                }
            }
            scrollbar_drag_node_ = {};
        }

        if (pressed_node_.is_valid()) {
            auto* n = tree_.get(pressed_node_);
            if (n && n->user_data) {
                auto* kind = reinterpret_cast<widget::WidgetKind*>(n->user_data);
                if (*kind == widget::WidgetKind::Button) {
                    auto* btn = reinterpret_cast<widget::ButtonData*>(n->user_data);
                    btn->state.set(widget::ButtonState::Normal);
                    tree_.mark_dirty(pressed_node_, core::kPaint);
                } else if (*kind == widget::WidgetKind::Slider) {
                    auto* sl = reinterpret_cast<widget::SliderData*>(n->user_data);
                    sl->is_dragging = false;
                    sl->thumb_radius.tween_to(8.0f, 0.12f, animation::CubicBezier::ease_out());
                }
            }
            pressed_node_ = {};
        }
        if (hovered_node_.is_valid()) {
            auto* n = tree_.get(hovered_node_);
            if (n && n->user_data) {
                auto* btn = reinterpret_cast<widget::ButtonData*>(n->user_data);
                btn->state.set(widget::ButtonState::Normal);
                tree_.mark_dirty(hovered_node_, core::kPaint);
            }
            hovered_node_ = {};
        }
        break;
    }
    }

    if (fallback_listener_) {
        fallback_listener_->on_pointer(event);
    }
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

void InputDispatcher::on_key(const platform::KeyEvent& event) {
    if (event.is_down && event.key_code == VK_TAB) {
        cycle_focus((event.modifiers & platform::kModShift) != 0);
        return;
    }

    if (focused_node_.is_valid()) {
        auto* n = tree_.get(focused_node_);
        if (n && n->user_data) {
            auto* kind = reinterpret_cast<widget::WidgetKind*>(n->user_data);
            if (*kind == widget::WidgetKind::TextInput) {
                auto* ti = reinterpret_cast<widget::TextInputData*>(n->user_data);

                if (event.character != 0 && event.character >= 32) {
                    if ((event.modifiers & platform::kModControl) == 0) {
                        ti->buffer.insert(event.character);
                        tree_.mark_dirty(focused_node_, core::kLayout | core::kPaint);
                        if (ti->on_change) {
                            ti->on_change(ti->buffer.text(), ti->change_user_data);
                        }
                        update_ime_window_position();
                        return;
                    }
                }

                if (event.is_down) {
                    bool changed = false;
                    bool caret_moved = false;
                    bool select = (event.modifiers & platform::kModShift) != 0;
                    bool ctrl = (event.modifiers & platform::kModControl) != 0;

                    switch (event.key_code) {
                    case VK_LEFT:
                        if (ctrl)
                            ti->buffer.move_word_left(select);
                        else
                            ti->buffer.move_cursor_left(select);
                        caret_moved = true;
                        break;
                    case VK_RIGHT:
                        if (ctrl)
                            ti->buffer.move_word_right(select);
                        else
                            ti->buffer.move_cursor_right(select);
                        caret_moved = true;
                        break;
                    case VK_HOME:
                        ti->buffer.move_cursor_to_start(select);
                        caret_moved = true;
                        break;
                    case VK_END:
                        ti->buffer.move_cursor_to_end(select);
                        caret_moved = true;
                        break;
                    case VK_BACK:
                        changed = ti->buffer.delete_backward();
                        break;
                    case VK_DELETE:
                        changed = ti->buffer.delete_forward();
                        break;
                    case VK_RETURN:
                        if (ti->on_submit) {
                            ti->on_submit(ti->buffer.text(), ti->submit_user_data);
                        }
                        break;
                    case 'A':
                        if (ctrl) {
                            ti->buffer.select_all();
                            caret_moved = true;
                        }
                        break;
                    case 'C':
                        if (ctrl) {
                            ti->buffer.copy_to_clipboard();
                        }
                        break;
                    case 'X':
                        if (ctrl) {
                            changed = ti->buffer.cut_to_clipboard();
                        }
                        break;
                    case 'V':
                        if (ctrl) {
                            changed = ti->buffer.paste_from_clipboard();
                        }
                        break;
                    default:
                        break;
                    }

                    if (changed) {
                        tree_.mark_dirty(focused_node_, core::kLayout | core::kPaint);
                        if (ti->on_change) {
                            ti->on_change(ti->buffer.text(), ti->change_user_data);
                        }
                        update_ime_window_position();
                    } else if (caret_moved) {
                        tree_.mark_dirty(focused_node_, core::kPaint);
                        update_ime_window_position();
                    }
                }
                return;
            } else if (*kind == widget::WidgetKind::Button) {
                if (event.is_down && (event.key_code == VK_SPACE || event.key_code == VK_RETURN)) {
                    auto* btn = reinterpret_cast<widget::ButtonData*>(n->user_data);
                    if (btn->on_click) {
                        btn->on_click(btn->click_user_data);
                    }
                    return;
                }
            } else if (*kind == widget::WidgetKind::Checkbox) {
                if (event.is_down && (event.key_code == VK_SPACE || event.key_code == VK_RETURN)) {
                    auto* cb = reinterpret_cast<widget::CheckboxData*>(n->user_data);
                    cb->toggle();
                    return;
                }
            } else if (*kind == widget::WidgetKind::Switch) {
                if (event.is_down && (event.key_code == VK_SPACE || event.key_code == VK_RETURN)) {
                    auto* sw = reinterpret_cast<widget::SwitchData*>(n->user_data);
                    sw->toggle();
                    return;
                }
            } else if (*kind == widget::WidgetKind::RadioButton) {
                if (event.is_down && (event.key_code == VK_SPACE || event.key_code == VK_RETURN)) {
                    auto* rb = reinterpret_cast<widget::RadioButtonData*>(n->user_data);
                    rb->select();
                    return;
                }
            } else if (*kind == widget::WidgetKind::Slider) {
                if (event.is_down) {
                    auto* sl = reinterpret_cast<widget::SliderData*>(n->user_data);
                    float delta =
                        (sl->step > 0.0f) ? sl->step : (sl->max_value - sl->min_value) * 0.05f;
                    if (event.key_code == VK_LEFT || event.key_code == VK_DOWN) {
                        sl->set_value(sl->value - delta);
                        return;
                    } else if (event.key_code == VK_RIGHT || event.key_code == VK_UP) {
                        sl->set_value(sl->value + delta);
                        return;
                    } else if (event.key_code == VK_HOME) {
                        sl->set_value(sl->min_value);
                        return;
                    } else if (event.key_code == VK_END) {
                        sl->set_value(sl->max_value);
                        return;
                    }
                }
            }
        }
    }

    if (fallback_listener_) {
        fallback_listener_->on_key(event);
    }
}

void InputDispatcher::on_ime_composition(std::string_view comp_str, int cursor_pos) {
    if (focused_node_.is_valid()) {
        auto* n = tree_.get(focused_node_);
        if (n && n->user_data) {
            auto* kind = reinterpret_cast<widget::WidgetKind*>(n->user_data);
            if (*kind == widget::WidgetKind::TextInput) {
                auto* ti = reinterpret_cast<widget::TextInputData*>(n->user_data);
                ti->composition_text = std::string(comp_str);
                ti->composition_cursor = cursor_pos;
                tree_.mark_dirty(focused_node_, core::kLayout | core::kPaint);
                update_ime_window_position();
            }
        }
    }
    if (fallback_listener_) {
        fallback_listener_->on_ime_composition(comp_str, cursor_pos);
    }
}

void InputDispatcher::on_ime_commit(std::string_view result_str) {
    if (focused_node_.is_valid()) {
        auto* n = tree_.get(focused_node_);
        if (n && n->user_data) {
            auto* kind = reinterpret_cast<widget::WidgetKind*>(n->user_data);
            if (*kind == widget::WidgetKind::TextInput) {
                auto* ti = reinterpret_cast<widget::TextInputData*>(n->user_data);
                ti->composition_text.clear();
                ti->buffer.insert(result_str);
                tree_.mark_dirty(focused_node_, core::kLayout | core::kPaint);
                if (ti->on_change) {
                    ti->on_change(ti->buffer.text(), ti->change_user_data);
                }
                update_ime_window_position();
            }
        }
    }
    if (fallback_listener_) {
        fallback_listener_->on_ime_commit(result_str);
    }
}

void InputDispatcher::on_close() {
    if (fallback_listener_) {
        fallback_listener_->on_close();
    }
}

void InputDispatcher::on_scroll(float delta_x, float delta_y, core::Point pointer_pos) {
    core::Handle<core::Node> hit = hit_test(pointer_pos);
    core::Handle<core::Node> curr = hit;
    while (curr.is_valid()) {
        auto* n = tree_.get(curr);
        if (!n)
            break;
        if (n->user_data) {
            auto* kind = reinterpret_cast<widget::WidgetKind*>(n->user_data);
            if (*kind == widget::WidgetKind::ScrollArea) {
                auto* sa = reinterpret_cast<widget::ScrollAreaData*>(n->user_data);
                float new_x = std::clamp(sa->target_scroll.x - delta_x, 0.0f, sa->max_scroll.x);
                float new_y = std::clamp(sa->target_scroll.y - delta_y, 0.0f, sa->max_scroll.y);
                if (new_x != sa->target_scroll.x || new_y != sa->target_scroll.y) {
                    sa->target_scroll = {new_x, new_y};
                    if (sa->smooth_scroll) {
                        sa->animated_scroll.spring_to(sa->target_scroll,
                                                      animation::SpringDesc::snappy());
                    } else {
                        sa->scroll_offset = sa->target_scroll;
                        sa->animated_scroll.set_immediate(sa->target_scroll);
                        tree_.mark_dirty(curr, core::kLayout | core::kPaint);
                    }
                }
                return;
            } else if (*kind == widget::WidgetKind::VirtualList) {
                auto* vl = reinterpret_cast<widget::VirtualListData*>(n->user_data);
                float new_y = std::clamp(vl->target_scroll_y - delta_y, 0.0f, vl->max_scroll.y);
                vl->target_scroll_y = new_y;
                if (vl->smooth_scroll) {
                    vl->animated_scroll_y.spring_to(vl->target_scroll_y,
                                                    animation::SpringDesc::snappy());
                } else {
                    vl->scroll_offset.y = vl->target_scroll_y;
                    vl->animated_scroll_y.set_immediate(vl->target_scroll_y);
                    tree_.mark_dirty(curr, core::kLayout | core::kPaint);
                }
                return;
            }
        }
        curr = n->parent;
    }

    if (fallback_listener_) {
        fallback_listener_->on_scroll(delta_x, delta_y, pointer_pos);
    }
}

} // namespace north::event
