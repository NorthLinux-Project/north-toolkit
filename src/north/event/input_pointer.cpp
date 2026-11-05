#include "north/event/input_dispatcher.hpp"

#include "north/overlay/overlay_manager.hpp"
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

void InputDispatcher::on_pointer(const platform::PointerEvent& event) {
    if (overlay_manager_ && overlay_manager_->has_active_modal()) {
        const auto* modal = overlay_manager_->top_modal();
        if (modal && !modal->computed_bounds.contains(event.position)) {
            if (event.action == platform::PointerAction::Down) {
                if (modal->config.barrier_dismissible) {
                    overlay_manager_->dismiss_top_modal();
                }
            }
            if (fallback_listener_) {
                fallback_listener_->on_pointer(event);
            }
            return;
        }
    }

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
                    core::Point node_origin = tree_.global_offset(pressed_node_);
                    float local_x =
                        event.position.x - (node_origin.x + ti->padding.left) + ti->scroll_offset;
                    if (ti->font) {
                        size_t cpos = find_closest_cursor_pos(ti->buffer.text(), ti->font,
                                                              ti->font_size, local_x);
                        ti->buffer.set_cursor_pos(cpos, true);
                        tree_.mark_dirty(pressed_node_, core::kPaint);
                        update_ime_window_position();
                    }
                } else if (*kind == widget::WidgetKind::TextArea) {
                    auto* ta = reinterpret_cast<widget::TextAreaData*>(pn->user_data);
                    if (ta->is_dragging && ta->font) {
                        core::Point node_origin = tree_.global_offset(pressed_node_);
                        float line_h = ta->font->line_height(ta->font_size);
                        float local_x = event.position.x - (node_origin.x + ta->padding.left + ta->line_numbers_width) + ta->scroll_x;
                        float local_y = event.position.y - (node_origin.y + ta->padding.top) + ta->scroll_y;
                        size_t line = (local_y < 0.0f) ? 0 : static_cast<size_t>(local_y / line_h);
                        if (line >= ta->editor.line_count()) {
                            line = ta->editor.line_count() - 1;
                        }
                        std::string_view lstr = ta->editor.line_text(line);
                        size_t c_offset = find_closest_cursor_pos(lstr, ta->font, ta->font_size, local_x);
                        size_t byte_pos = ta->editor.line_start(line) + c_offset;
                        ta->editor.set_cursor_pos(byte_pos, true);
                        tree_.mark_dirty(pressed_node_, core::kPaint);
                        update_ime_window_position();
                    }
                } else if (*kind == widget::WidgetKind::Slider) {
                    auto* sl = reinterpret_cast<widget::SliderData*>(pn->user_data);
                    if (sl->is_dragging) {
                        core::Point node_origin = tree_.global_offset(pressed_node_);
                        sl->seek_from_pointer(event.position.x - node_origin.x);
                        return;
                    }
                } else if (*kind == widget::WidgetKind::SplitView) {
                    auto* sv = reinterpret_cast<widget::SplitViewData*>(pn->user_data);
                    if (sv->sash_dragging) {
                        core::Point node_origin = tree_.global_offset(pressed_node_);
                        bool is_horiz = (sv->orientation == widget::SplitOrientation::Horizontal);
                        float ptr_pos = is_horiz ? (event.position.x - node_origin.x)
                                                 : (event.position.y - node_origin.y);
                        float total_dim = is_horiz ? pn->layout.size.width : pn->layout.size.height;
                        float avail_dim = (std::max)(1.0f, total_dim - sv->sash_thickness);

                        if (sv->sizing_mode == widget::SplitSizingMode::Ratio) {
                            float delta_ptr = ptr_pos - sv->drag_start_pointer_pos;
                            float delta_ratio = delta_ptr / avail_dim;
                            sv->set_split(std::clamp(sv->drag_start_split_value + delta_ratio, 0.0f, 1.0f));
                        } else if (sv->sizing_mode == widget::SplitSizingMode::FixedFirst) {
                            sv->set_split((std::max)(0.0f, ptr_pos));
                        } else {
                            sv->set_split((std::max)(0.0f, total_dim - ptr_pos - sv->sash_thickness));
                        }
                        return;
                    }
                } else if (*kind == widget::WidgetKind::DataTable) {
                    auto* dt = reinterpret_cast<widget::DataTableData*>(pn->user_data);
                    if (dt->is_resizing_column && dt->resizing_column < dt->columns.size()) {
                        float delta_x = event.position.x - dt->resize_start_pointer_x;
                        auto& col = dt->columns[dt->resizing_column];
                        col.width = (std::max)(col.min_width, dt->resize_start_column_w + delta_x);
                        tree_.mark_dirty(pressed_node_, core::kLayout | core::kPaint);
                        return;
                    }
                } else if (*kind == widget::WidgetKind::TabBar) {
                    auto* tb = reinterpret_cast<widget::TabBarData*>(pn->user_data);
                    if (tb->is_dragging && tb->reorderable && tb->drag_index < tb->tabs.size()) {
                        core::Point node_origin = tree_.global_offset(pressed_node_);
                        float local_x = event.position.x - node_origin.x;
                        tb->drag_current_mouse_x = local_x;
                        if (std::abs(local_x - tb->drag_start_mouse_x) > 4.0f) {
                            tb->drag_threshold_exceeded = true;
                        }
                        if (tb->drag_threshold_exceeded) {
                            float dragged_center = local_x - tb->drag_grab_offset_x +
                                                   tb->tabs[tb->drag_index].computed_width * 0.5f +
                                                   tb->scroll_offset;
                            size_t target_idx = 0;
                            for (size_t i = 0; i < tb->tabs.size(); ++i) {
                                float mid =
                                    tb->tabs[i].computed_x + tb->tabs[i].computed_width * 0.5f;
                                if (dragged_center > mid) {
                                    target_idx = i;
                                }
                            }
                            tb->drop_target_index = target_idx;
                        }
                        tree_.mark_dirty(pressed_node_, core::kPaint);
                        return;
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
                    } else if (*kind == widget::WidgetKind::TabBar) {
                        auto* tb = reinterpret_cast<widget::TabBarData*>(prev_n->user_data);
                        tb->hovered_index = SIZE_MAX;
                        tb->close_hovered_index = SIZE_MAX;
                        tb->add_button_hovered = false;
                        tb->left_scroll_hovered = false;
                        tb->right_scroll_hovered = false;
                        tree_.mark_dirty(hovered_node_, core::kPaint);
                    } else if (*kind == widget::WidgetKind::SegmentedButton) {
                        auto* sb =
                            reinterpret_cast<widget::SegmentedButtonData*>(prev_n->user_data);
                        sb->hovered_index = SIZE_MAX;
                        tree_.mark_dirty(hovered_node_, core::kPaint);
                    } else if (*kind == widget::WidgetKind::Accordion) {
                        auto* acc = reinterpret_cast<widget::AccordionData*>(prev_n->user_data);
                        acc->hovered_header = SIZE_MAX;
                        tree_.mark_dirty(hovered_node_, core::kPaint);
                    } else if (*kind == widget::WidgetKind::SplitView) {
                        auto* sv = reinterpret_cast<widget::SplitViewData*>(prev_n->user_data);
                        sv->sash_hovered = false;
                        tree_.mark_dirty(hovered_node_, core::kPaint);
                    } else if (*kind == widget::WidgetKind::TreeView) {
                        auto* tv = reinterpret_cast<widget::TreeViewData*>(prev_n->user_data);
                        tv->hovered_row = SIZE_MAX;
                        tree_.mark_dirty(hovered_node_, core::kPaint);
                    } else if (*kind == widget::WidgetKind::DataTable) {
                        auto* dt = reinterpret_cast<widget::DataTableData*>(prev_n->user_data);
                        dt->hovered_row = SIZE_MAX;
                        dt->hovered_header = SIZE_MAX;
                        dt->hovered_resizer = SIZE_MAX;
                        tree_.mark_dirty(hovered_node_, core::kPaint);
                    } else if (*kind == widget::WidgetKind::CommandPalette) {
                        auto* cp = reinterpret_cast<widget::CommandPaletteData*>(prev_n->user_data);
                        cp->hovered_index = SIZE_MAX;
                        tree_.mark_dirty(hovered_node_, core::kPaint);
                    } else if (*kind == widget::WidgetKind::Select) {
                        auto* sel = reinterpret_cast<widget::SelectData*>(prev_n->user_data);
                        sel->is_hovered = false;
                        sel->hovered_index = SIZE_MAX;
                        tree_.mark_dirty(hovered_node_, core::kPaint);
                    } else if (*kind == widget::WidgetKind::ComboBox) {
                        auto* cb = reinterpret_cast<widget::ComboBoxData*>(prev_n->user_data);
                        cb->is_hovered = false;
                        cb->hovered_index = SIZE_MAX;
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

        if (target.is_valid()) {
            auto* n = tree_.get(target);
            if (n && n->user_data) {
                auto* kind = reinterpret_cast<widget::WidgetKind*>(n->user_data);
                if (*kind == widget::WidgetKind::TabBar) {
                    auto* tb = reinterpret_cast<widget::TabBarData*>(n->user_data);
                    core::Point node_origin = tree_.global_offset(target);
                    core::Point local_pt = event.position - node_origin;
                    bool add_hov = tb->show_add_button && tb->add_button_rect.contains(local_pt);
                    bool left_hov = tb->has_overflow && tb->left_scroll_rect.contains(local_pt);
                    bool right_hov = tb->has_overflow && tb->right_scroll_rect.contains(local_pt);
                    size_t hov_idx = SIZE_MAX;
                    size_t close_hov_idx = SIZE_MAX;

                    if (!add_hov && !left_hov && !right_hov) {
                        float tab_x = local_pt.x + tb->scroll_offset;
                        for (size_t i = 0; i < tb->tabs.size(); ++i) {
                            if (tab_x >= tb->tabs[i].computed_x &&
                                tab_x < tb->tabs[i].computed_x + tb->tabs[i].computed_width) {
                                hov_idx = i;
                                if (tb->tabs[i].closable &&
                                    tb->tabs[i].close_rect.contains({tab_x, local_pt.y})) {
                                    close_hov_idx = i;
                                }
                                break;
                            }
                        }
                    }

                    if (tb->hovered_index != hov_idx || tb->close_hovered_index != close_hov_idx ||
                        tb->add_button_hovered != add_hov || tb->left_scroll_hovered != left_hov ||
                        tb->right_scroll_hovered != right_hov) {
                        tb->hovered_index = hov_idx;
                        tb->close_hovered_index = close_hov_idx;
                        tb->add_button_hovered = add_hov;
                        tb->left_scroll_hovered = left_hov;
                        tb->right_scroll_hovered = right_hov;
                        tree_.mark_dirty(target, core::kPaint);
                    }
                } else if (*kind == widget::WidgetKind::SegmentedButton) {
                    auto* sb = reinterpret_cast<widget::SegmentedButtonData*>(n->user_data);
                    core::Point node_origin = tree_.global_offset(target);
                    float local_x = event.position.x - node_origin.x;
                    size_t hov_idx = SIZE_MAX;
                    for (size_t i = 0; i < sb->segments.size(); ++i) {
                        if (local_x >= sb->segments[i].computed_x &&
                            local_x < sb->segments[i].computed_x + sb->segments[i].computed_width) {
                            hov_idx = i;
                            break;
                        }
                    }
                    if (sb->hovered_index != hov_idx) {
                        sb->hovered_index = hov_idx;
                        tree_.mark_dirty(target, core::kPaint);
                    }
                } else if (*kind == widget::WidgetKind::Accordion) {
                    auto* acc = reinterpret_cast<widget::AccordionData*>(n->user_data);
                    core::Point node_origin = tree_.global_offset(target);
                    float local_y = event.position.y - node_origin.y;
                    size_t hov_hdr = SIZE_MAX;
                    for (size_t i = 0; i < acc->items.size(); ++i) {
                        if (local_y >= acc->items[i].header_y &&
                            local_y < acc->items[i].header_y + acc->header_height) {
                            hov_hdr = i;
                            break;
                        }
                    }
                    if (acc->hovered_header != hov_hdr) {
                        acc->hovered_header = hov_hdr;
                        tree_.mark_dirty(target, core::kPaint);
                    }
                } else if (*kind == widget::WidgetKind::SplitView) {
                    auto* sv = reinterpret_cast<widget::SplitViewData*>(n->user_data);
                    core::Point node_origin = tree_.global_offset(target);
                    core::Point local_pt = event.position - node_origin;
                    bool hov = sv->sash_rect.contains(local_pt);
                    if (sv->sash_hovered != hov) {
                        sv->sash_hovered = hov;
                        tree_.mark_dirty(target, core::kPaint);
                    }
                } else if (*kind == widget::WidgetKind::TreeView) {
                    auto* tv = reinterpret_cast<widget::TreeViewData*>(n->user_data);
                    core::Point node_origin = tree_.global_offset(target);
                    float local_y = event.position.y - node_origin.y + tv->scroll_y;
                    size_t hov_row = SIZE_MAX;
                    if (local_y >= 0.0f && tv->row_height > 0.0f) {
                        size_t r = static_cast<size_t>(local_y / tv->row_height);
                        if (r < tv->visible_rows.size()) {
                            hov_row = r;
                        }
                    }
                    if (tv->hovered_row != hov_row) {
                        tv->hovered_row = hov_row;
                        tree_.mark_dirty(target, core::kPaint);
                    }
                } else if (*kind == widget::WidgetKind::DataTable) {
                    auto* dt = reinterpret_cast<widget::DataTableData*>(n->user_data);
                    core::Point node_origin = tree_.global_offset(target);
                    core::Point local_pt = event.position - node_origin;
                    size_t hov_hdr = SIZE_MAX;
                    size_t hov_resizer = SIZE_MAX;
                    size_t hov_row = SIZE_MAX;

                    if (local_pt.y < dt->header_height) {
                        for (size_t c = 0; c < dt->columns.size(); ++c) {
                            const auto& col = dt->columns[c];
                            if (col.resizable) {
                                float div_x = col.computed_x + col.computed_width;
                                if (std::abs(local_pt.x - div_x) <= 4.0f) {
                                    hov_resizer = c;
                                    break;
                                }
                            }
                            if (local_pt.x >= col.computed_x && local_pt.x < col.computed_x + col.computed_width) {
                                hov_hdr = c;
                            }
                        }
                    } else {
                        float body_y = local_pt.y - dt->header_height + dt->scroll_y;
                        if (body_y >= 0.0f && dt->row_height > 0.0f) {
                            size_t r = static_cast<size_t>(body_y / dt->row_height);
                            if (r < dt->rows.size()) {
                                hov_row = r;
                            }
                        }
                    }

                    if (dt->hovered_header != hov_hdr || dt->hovered_resizer != hov_resizer || dt->hovered_row != hov_row) {
                        dt->hovered_header = hov_hdr;
                        dt->hovered_resizer = hov_resizer;
                        dt->hovered_row = hov_row;
                        tree_.mark_dirty(target, core::kPaint);
                    }
                } else if (*kind == widget::WidgetKind::CommandPalette) {
                    auto* cp = reinterpret_cast<widget::CommandPaletteData*>(n->user_data);
                    core::Point node_origin{0.0f, 0.0f};
                    if (overlay_manager_ && cp->active_overlay_id != overlay::kInvalidOverlayId) {
                        const auto* ent = overlay_manager_->find_overlay(cp->active_overlay_id);
                        if (ent) {
                            node_origin = core::Point{ent->computed_bounds.x, ent->computed_bounds.y};
                        }
                    } else {
                        node_origin = tree_.global_offset(target);
                    }
                    core::Point local_pt{event.position.x - node_origin.x, event.position.y - node_origin.y};
                    size_t hov_idx = SIZE_MAX;
                    float list_y = local_pt.y - 44.0f + cp->scroll_y;
                    if (list_y >= 0.0f && cp->row_height > 0.0f) {
                        size_t idx = static_cast<size_t>(list_y / cp->row_height);
                        if (idx < cp->filtered_matches.size()) {
                            hov_idx = idx;
                        }
                    }
                    if (cp->hovered_index != hov_idx) {
                        cp->hovered_index = hov_idx;
                        tree_.mark_dirty(target, core::kPaint);
                    }
                } else if (*kind == widget::WidgetKind::Select) {
                    auto* sel = reinterpret_cast<widget::SelectData*>(n->user_data);
                    if (target == sel->node_handle) {
                        if (!sel->is_hovered) {
                            sel->is_hovered = true;
                            tree_.mark_dirty(target, core::kPaint);
                        }
                    } else {
                        core::Point node_origin{0.0f, 0.0f};
                        if (overlay_manager_ && sel->active_overlay_id != overlay::kInvalidOverlayId) {
                            const auto* ent = overlay_manager_->find_overlay(sel->active_overlay_id);
                            if (ent) {
                                node_origin = core::Point{ent->computed_bounds.x, ent->computed_bounds.y};
                            }
                        }
                        core::Point local_pt{event.position.x - node_origin.x, event.position.y - node_origin.y};
                        size_t hov_idx = SIZE_MAX;
                        float list_y = local_pt.y + sel->scroll_y;
                        if (list_y >= 0.0f && sel->item_height > 0.0f) {
                            size_t idx = static_cast<size_t>(list_y / sel->item_height);
                            if (idx < sel->items.size()) {
                                hov_idx = idx;
                            }
                        }
                        if (sel->hovered_index != hov_idx) {
                            sel->hovered_index = hov_idx;
                            tree_.mark_dirty(target, core::kPaint);
                        }
                    }
                } else if (*kind == widget::WidgetKind::ComboBox) {
                    auto* cb = reinterpret_cast<widget::ComboBoxData*>(n->user_data);
                    if (target == cb->node_handle) {
                        if (!cb->is_hovered) {
                            cb->is_hovered = true;
                            tree_.mark_dirty(target, core::kPaint);
                        }
                    } else {
                        core::Point node_origin{0.0f, 0.0f};
                        if (overlay_manager_ && cb->active_overlay_id != overlay::kInvalidOverlayId) {
                            const auto* ent = overlay_manager_->find_overlay(cb->active_overlay_id);
                            if (ent) {
                                node_origin = core::Point{ent->computed_bounds.x, ent->computed_bounds.y};
                            }
                        }
                        core::Point local_pt{event.position.x - node_origin.x, event.position.y - node_origin.y};
                        size_t hov_idx = SIZE_MAX;
                        float list_y = local_pt.y + cb->scroll_y;
                        if (list_y >= 0.0f && cb->item_height > 0.0f) {
                            size_t idx = static_cast<size_t>(list_y / cb->item_height);
                            if (idx < cb->filtered_indices.size()) {
                                hov_idx = idx;
                            }
                        }
                        if (cb->hovered_index != hov_idx) {
                            cb->hovered_index = hov_idx;
                            tree_.mark_dirty(target, core::kPaint);
                        }
                    }
                }
            }
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
                            core::Point sa_origin = tree_.global_offset(curr_scroll);
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
                            core::Point vl_origin = tree_.global_offset(curr_scroll);
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
                        core::Point sa_origin = tree_.global_offset(curr_scroll);
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
                        core::Point vl_origin = tree_.global_offset(curr_scroll);
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
                        core::Point node_origin = tree_.global_offset(target);
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
                    } else if (*kind == widget::WidgetKind::TextArea) {
                        focus_assigned = true;
                        set_focus(target);
                        pressed_node_ = target;
                        auto* ta = reinterpret_cast<widget::TextAreaData*>(n->user_data);
                        core::Point node_origin = tree_.global_offset(target);
                        if (ta->font) {
                            float line_h = ta->font->line_height(ta->font_size);
                            float local_x = event.position.x - (node_origin.x + ta->padding.left + ta->line_numbers_width) + ta->scroll_x;
                            float local_y = event.position.y - (node_origin.y + ta->padding.top) + ta->scroll_y;
                            size_t line = (local_y < 0.0f) ? 0 : static_cast<size_t>(local_y / line_h);
                            if (line >= ta->editor.line_count()) {
                                line = ta->editor.line_count() - 1;
                            }
                            std::string_view lstr = ta->editor.line_text(line);
                            size_t c_offset = find_closest_cursor_pos(lstr, ta->font, ta->font_size, local_x);
                            size_t byte_pos = ta->editor.line_start(line) + c_offset;
                            ta->editor.set_cursor_pos(byte_pos, (event.modifiers & platform::kModShift) != 0);
                            ta->is_dragging = true;
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
                        core::Point node_origin = tree_.global_offset(target);
                        sl->seek_from_pointer(event.position.x - node_origin.x);
                    } else if (*kind == widget::WidgetKind::RadioButton) {
                        focus_assigned = true;
                        set_focus(target);
                        pressed_node_ = target;
                    } else if (*kind == widget::WidgetKind::TabBar) {
                        focus_assigned = true;
                        set_focus(target);
                        pressed_node_ = target;
                        auto* tb = reinterpret_cast<widget::TabBarData*>(n->user_data);
                        core::Point node_origin = tree_.global_offset(target);
                        core::Point local_pt = event.position - node_origin;
                        if (tb->has_overflow && tb->left_scroll_rect.contains(local_pt)) {
                            tb->scroll_offset = (std::max)(0.0f, tb->scroll_offset - 80.0f);
                            tree_.mark_dirty(target, core::kPaint);
                        } else if (tb->has_overflow && tb->right_scroll_rect.contains(local_pt)) {
                            tb->scroll_offset =
                                (std::min)(tb->max_scroll_offset, tb->scroll_offset + 80.0f);
                            tree_.mark_dirty(target, core::kPaint);
                        } else if (!(tb->show_add_button && tb->add_button_rect.contains(local_pt))) {
                            float tab_x = local_pt.x + tb->scroll_offset;
                            for (size_t i = 0; i < tb->tabs.size(); ++i) {
                                if (tab_x >= tb->tabs[i].computed_x &&
                                    tab_x < tb->tabs[i].computed_x + tb->tabs[i].computed_width) {
                                    bool hit_close =
                                        tb->tabs[i].closable &&
                                        tb->tabs[i].close_rect.contains({tab_x, local_pt.y});
                                    if (!hit_close) {
                                        tb->select_tab(i);
                                        if (tb->reorderable) {
                                            tb->is_dragging = true;
                                            tb->drag_threshold_exceeded = false;
                                            tb->drag_index = i;
                                            tb->drop_target_index = i;
                                            tb->drag_start_mouse_x = local_pt.x;
                                            tb->drag_current_mouse_x = local_pt.x;
                                            tb->drag_grab_offset_x =
                                                local_pt.x -
                                                (tb->tabs[i].computed_x - tb->scroll_offset);
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    } else if (*kind == widget::WidgetKind::SegmentedButton) {
                        focus_assigned = true;
                        set_focus(target);
                        pressed_node_ = target;
                    } else if (*kind == widget::WidgetKind::Accordion) {
                        focus_assigned = true;
                        set_focus(target);
                        pressed_node_ = target;
                    } else if (*kind == widget::WidgetKind::SplitView) {
                        focus_assigned = true;
                        set_focus(target);
                        pressed_node_ = target;
                        auto* sv = reinterpret_cast<widget::SplitViewData*>(n->user_data);
                        core::Point node_origin = tree_.global_offset(target);
                        core::Point local_pt = event.position - node_origin;
                        if (sv->sash_rect.contains(local_pt)) {
                            uint64_t now_ms = GetTickCount64();
                            if (now_ms - sv->last_sash_click_ms < 300) {
                                sv->reset_split();
                                sv->last_sash_click_ms = 0;
                            } else {
                                sv->last_sash_click_ms = now_ms;
                                sv->sash_dragging = true;
                                bool is_horiz = (sv->orientation == widget::SplitOrientation::Horizontal);
                                sv->drag_start_pointer_pos = is_horiz ? local_pt.x : local_pt.y;
                                sv->drag_start_split_value = sv->split_value;
                                tree_.mark_dirty(target, core::kPaint);
                            }
                        }
                    } else if (*kind == widget::WidgetKind::TreeView) {
                        focus_assigned = true;
                        set_focus(target);
                        pressed_node_ = target;
                        auto* tv = reinterpret_cast<widget::TreeViewData*>(n->user_data);
                        core::Point node_origin = tree_.global_offset(target);
                        float local_y = event.position.y - node_origin.y + tv->scroll_y;
                        if (local_y >= 0.0f && tv->row_height > 0.0f) {
                            size_t r = static_cast<size_t>(local_y / tv->row_height);
                            if (r < tv->visible_rows.size() && tv->visible_rows[r].item) {
                                auto* item = tv->visible_rows[r].item;
                                float cursor_x = 8.0f + static_cast<float>(tv->visible_rows[r].depth) * tv->indent_width;
                                float local_x = event.position.x - node_origin.x;
                                if (item->expandable && local_x >= cursor_x && local_x < cursor_x + 16.0f) {
                                    tv->toggle_node(item->id);
                                } else {
                                    tv->select_node(item->id);
                                }
                            }
                        }
                    } else if (*kind == widget::WidgetKind::DataTable) {
                        focus_assigned = true;
                        set_focus(target);
                        pressed_node_ = target;
                        auto* dt = reinterpret_cast<widget::DataTableData*>(n->user_data);
                        core::Point node_origin = tree_.global_offset(target);
                        core::Point local_pt = event.position - node_origin;
                        if (local_pt.y < dt->header_height) {
                            bool hit_resizer = false;
                            for (size_t c = 0; c < dt->columns.size(); ++c) {
                                const auto& col = dt->columns[c];
                                if (col.resizable) {
                                    float div_x = col.computed_x + col.computed_width;
                                    if (std::abs(local_pt.x - div_x) <= 4.0f) {
                                        dt->is_resizing_column = true;
                                        dt->resizing_column = c;
                                        dt->resize_start_pointer_x = event.position.x;
                                        dt->resize_start_column_w = col.width;
                                        hit_resizer = true;
                                        tree_.mark_dirty(target, core::kPaint);
                                        break;
                                    }
                                }
                            }
                            if (!hit_resizer) {
                                for (size_t c = 0; c < dt->columns.size(); ++c) {
                                    const auto& col = dt->columns[c];
                                    if (col.sortable && local_pt.x >= col.computed_x &&
                                        local_pt.x < col.computed_x + col.computed_width) {
                                        dt->cycle_sort(c);
                                        break;
                                    }
                                }
                            }
                        } else {
                            float body_y = local_pt.y - dt->header_height + dt->scroll_y;
                            if (body_y >= 0.0f && dt->row_height > 0.0f) {
                                size_t r = static_cast<size_t>(body_y / dt->row_height);
                                if (r < dt->rows.size()) {
                                    dt->select_row(r);
                                }
                            }
                        }
                    } else if (*kind == widget::WidgetKind::CommandPalette) {
                        focus_assigned = true;
                        set_focus(target);
                        pressed_node_ = target;
                        auto* cp = reinterpret_cast<widget::CommandPaletteData*>(n->user_data);
                        core::Point node_origin{0.0f, 0.0f};
                        if (overlay_manager_ && cp->active_overlay_id != overlay::kInvalidOverlayId) {
                            const auto* ent = overlay_manager_->find_overlay(cp->active_overlay_id);
                            if (ent) {
                                node_origin = core::Point{ent->computed_bounds.x, ent->computed_bounds.y};
                            }
                        } else {
                            node_origin = tree_.global_offset(target);
                        }
                        core::Point local_pt{event.position.x - node_origin.x, event.position.y - node_origin.y};
                        float list_y = local_pt.y - 44.0f + cp->scroll_y;
                        if (list_y >= 0.0f && cp->row_height > 0.0f) {
                            size_t idx = static_cast<size_t>(list_y / cp->row_height);
                            if (idx < cp->filtered_matches.size()) {
                                cp->select_index(idx);
                                cp->execute_selected();
                            }
                        }
                    } else if (*kind == widget::WidgetKind::Select) {
                        focus_assigned = true;
                        auto* sel = reinterpret_cast<widget::SelectData*>(n->user_data);
                        if (target == sel->node_handle) {
                            set_focus(target);
                            pressed_node_ = target;
                            sel->toggle_dropdown();
                        } else {
                            core::Point node_origin{0.0f, 0.0f};
                            if (overlay_manager_ && sel->active_overlay_id != overlay::kInvalidOverlayId) {
                                const auto* ent = overlay_manager_->find_overlay(sel->active_overlay_id);
                                if (ent) {
                                    node_origin = core::Point{ent->computed_bounds.x, ent->computed_bounds.y};
                                }
                            }
                            core::Point local_pt{event.position.x - node_origin.x, event.position.y - node_origin.y};
                            float list_y = local_pt.y + sel->scroll_y;
                            if (list_y >= 0.0f && sel->item_height > 0.0f) {
                                size_t idx = static_cast<size_t>(list_y / sel->item_height);
                                if (idx < sel->items.size()) {
                                    sel->select_by_index(idx);
                                }
                            }
                        }
                    } else if (*kind == widget::WidgetKind::ComboBox) {
                        focus_assigned = true;
                        auto* cb = reinterpret_cast<widget::ComboBoxData*>(n->user_data);
                        if (target == cb->node_handle) {
                            set_focus(target);
                            pressed_node_ = target;
                            core::Point node_origin = tree_.global_offset(target);
                            core::Point local_pt{event.position.x - node_origin.x, event.position.y - node_origin.y};
                            if (local_pt.x >= n->layout.size.width - 28.0f) {
                                cb->toggle_dropdown();
                            } else {
                                if (!cb->is_open) {
                                    cb->open_dropdown();
                                }
                            }
                        } else {
                            core::Point node_origin{0.0f, 0.0f};
                            if (overlay_manager_ && cb->active_overlay_id != overlay::kInvalidOverlayId) {
                                const auto* ent = overlay_manager_->find_overlay(cb->active_overlay_id);
                                if (ent) {
                                    node_origin = core::Point{ent->computed_bounds.x, ent->computed_bounds.y};
                                }
                            }
                            core::Point local_pt{event.position.x - node_origin.x, event.position.y - node_origin.y};
                            float list_y = local_pt.y + cb->scroll_y;
                            if (list_y >= 0.0f && cb->item_height > 0.0f) {
                                size_t idx = static_cast<size_t>(list_y / cb->item_height);
                                if (idx < cb->filtered_indices.size()) {
                                    cb->select_by_index(idx);
                                }
                            }
                        }
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
                } else if (*kind == widget::WidgetKind::TextArea) {
                    auto* ta = reinterpret_cast<widget::TextAreaData*>(n->user_data);
                    ta->is_dragging = false;
                } else if (*kind == widget::WidgetKind::TabBar) {
                    auto* tb = reinterpret_cast<widget::TabBarData*>(n->user_data);
                    core::Point node_origin = tree_.global_offset(pressed);
                    core::Point local_pt = event.position - node_origin;
                    if (tb->is_dragging) {
                        bool was_reordering = tb->drag_threshold_exceeded;
                        size_t from_idx = tb->drag_index;
                        size_t to_idx = tb->drop_target_index;
                        tb->is_dragging = false;
                        tb->drag_threshold_exceeded = false;
                        tb->drag_index = SIZE_MAX;
                        tb->drop_target_index = SIZE_MAX;
                        if (was_reordering && from_idx < tb->tabs.size() &&
                            to_idx < tb->tabs.size() && from_idx != to_idx) {
                            tb->reorder_tab(from_idx, to_idx);
                        } else {
                            tree_.mark_dirty(pressed, core::kPaint);
                        }
                    } else if (is_same) {
                        if (tb->show_add_button && tb->add_button_rect.contains(local_pt)) {
                            if (tb->on_add) {
                                tb->on_add(tb->add_user_data);
                            }
                        } else {
                            float tab_x = local_pt.x + tb->scroll_offset;
                            for (size_t i = 0; i < tb->tabs.size(); ++i) {
                                if (tb->tabs[i].closable &&
                                    tb->tabs[i].close_rect.contains({tab_x, local_pt.y})) {
                                    tb->close_tab(i);
                                    break;
                                }
                            }
                        }
                    }
                } else if (*kind == widget::WidgetKind::SegmentedButton) {
                    if (is_same) {
                        auto* sb = reinterpret_cast<widget::SegmentedButtonData*>(n->user_data);
                        core::Point node_origin = tree_.global_offset(pressed);
                        float local_x = event.position.x - node_origin.x;
                        for (size_t i = 0; i < sb->segments.size(); ++i) {
                            if (local_x >= sb->segments[i].computed_x &&
                                local_x <
                                    sb->segments[i].computed_x + sb->segments[i].computed_width) {
                                sb->toggle_segment(i);
                                break;
                            }
                        }
                    }
                } else if (*kind == widget::WidgetKind::Accordion) {
                    if (is_same) {
                        auto* acc = reinterpret_cast<widget::AccordionData*>(n->user_data);
                        core::Point node_origin = tree_.global_offset(pressed);
                        float local_y = event.position.y - node_origin.y;
                        for (size_t i = 0; i < acc->items.size(); ++i) {
                            if (local_y >= acc->items[i].header_y &&
                                local_y < acc->items[i].header_y + acc->header_height) {
                                acc->toggle_item(i);
                                break;
                            }
                        }
                    }
                } else if (*kind == widget::WidgetKind::SplitView) {
                    auto* sv = reinterpret_cast<widget::SplitViewData*>(n->user_data);
                    if (sv->sash_dragging) {
                        sv->sash_dragging = false;
                        tree_.mark_dirty(pressed, core::kPaint);
                    }
                } else if (*kind == widget::WidgetKind::DataTable) {
                    auto* dt = reinterpret_cast<widget::DataTableData*>(n->user_data);
                    if (dt->is_resizing_column) {
                        dt->is_resizing_column = false;
                        dt->resizing_column = SIZE_MAX;
                        tree_.mark_dirty(pressed, core::kLayout | core::kPaint);
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
                } else if (*kind == widget::WidgetKind::TextArea) {
                    auto* ta = reinterpret_cast<widget::TextAreaData*>(n->user_data);
                    ta->is_dragging = false;
                } else if (*kind == widget::WidgetKind::TabBar) {
                    auto* tb = reinterpret_cast<widget::TabBarData*>(n->user_data);
                    tb->is_dragging = false;
                    tb->drag_threshold_exceeded = false;
                    tb->drag_index = SIZE_MAX;
                    tb->drop_target_index = SIZE_MAX;
                    tree_.mark_dirty(pressed_node_, core::kPaint);
                }
            }
            pressed_node_ = {};
        }
        if (hovered_node_.is_valid()) {
            auto* n = tree_.get(hovered_node_);
            if (n && n->user_data) {
                auto* kind = reinterpret_cast<widget::WidgetKind*>(n->user_data);
                if (*kind == widget::WidgetKind::Button) {
                    auto* btn = reinterpret_cast<widget::ButtonData*>(n->user_data);
                    btn->state.set(widget::ButtonState::Normal);
                    tree_.mark_dirty(hovered_node_, core::kPaint);
                } else if (*kind == widget::WidgetKind::TabBar) {
                    auto* tb = reinterpret_cast<widget::TabBarData*>(n->user_data);
                    tb->hovered_index = SIZE_MAX;
                    tb->close_hovered_index = SIZE_MAX;
                    tb->add_button_hovered = false;
                    tb->left_scroll_hovered = false;
                    tb->right_scroll_hovered = false;
                    tree_.mark_dirty(hovered_node_, core::kPaint);
                } else if (*kind == widget::WidgetKind::SegmentedButton) {
                    auto* sb = reinterpret_cast<widget::SegmentedButtonData*>(n->user_data);
                    sb->hovered_index = SIZE_MAX;
                    tree_.mark_dirty(hovered_node_, core::kPaint);
                } else if (*kind == widget::WidgetKind::Accordion) {
                    auto* acc = reinterpret_cast<widget::AccordionData*>(n->user_data);
                    acc->hovered_header = SIZE_MAX;
                    tree_.mark_dirty(hovered_node_, core::kPaint);
                }
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

} // namespace north::event
