#include "north/event/input_dispatcher.hpp"

#include "north/overlay/overlay_manager.hpp"
#include "north/widget/widget_data.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>

#include <windows.h>

namespace north::event {

void InputDispatcher::on_key(const platform::KeyEvent& event) {
    if (event.is_down) {
        for (const auto& acc : accelerators_) {
            if (acc.shortcut.key_code == event.key_code &&
                acc.shortcut.modifiers == event.modifiers) {
                if (acc.callback) {
                    acc.callback(acc.user_data);
                    return;
                }
            }
        }
    }

    if (event.is_down && event.key_code == VK_ESCAPE) {
        if (overlay_manager_ && overlay_manager_->has_active_overlays()) {
            overlay_manager_->dismiss_top_modal();
            return;
        }
    }

    if (event.is_down && (event.modifiers & platform::kModControl) != 0 &&
        (event.key_code == VK_TAB || event.key_code == 'W')) {
        auto find_tab_bar = [&]() -> widget::TabBarData* {
            for (auto cur = focused_node_; cur.is_valid();) {
                auto* pn = tree_.get(cur);
                if (!pn)
                    break;
                if (pn->user_data) {
                    auto* k = reinterpret_cast<widget::WidgetKind*>(pn->user_data);
                    if (*k == widget::WidgetKind::TabBar) {
                        return reinterpret_cast<widget::TabBarData*>(pn->user_data);
                    }
                    if (*k == widget::WidgetKind::TabsView) {
                        auto* tv = reinterpret_cast<widget::TabsViewData*>(pn->user_data);
                        return tv->bar_data;
                    }
                }
                cur = pn->parent;
            }
            widget::TabBarData* found = nullptr;
            auto search = [&](auto& self, core::Handle<core::Node> h) -> void {
                if (found)
                    return;
                auto* pn = tree_.get(h);
                if (!pn)
                    return;
                if (pn->user_data) {
                    auto* k = reinterpret_cast<widget::WidgetKind*>(pn->user_data);
                    if (*k == widget::WidgetKind::TabBar) {
                        found = reinterpret_cast<widget::TabBarData*>(pn->user_data);
                        return;
                    }
                }
                for (auto ch : pn->children) {
                    self(self, ch);
                }
            };
            if (tree_.root().is_valid()) {
                search(search, tree_.root());
            }
            return found;
        };

        if (auto* tb = find_tab_bar()) {
            if (event.key_code == VK_TAB && !tb->tabs.empty()) {
                bool reverse = (event.modifiers & platform::kModShift) != 0;
                size_t next = reverse ? ((tb->active_index == 0) ? (tb->tabs.size() - 1)
                                                                 : (tb->active_index - 1))
                                      : ((tb->active_index + 1) % tb->tabs.size());
                tb->select_tab(next);
                return;
            }
            if (event.key_code == 'W' && !tb->tabs.empty()) {
                tb->close_tab(tb->active_index);
                return;
            }
        }
    }

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
            } else if (*kind == widget::WidgetKind::TextArea) {
                auto* ta = reinterpret_cast<widget::TextAreaData*>(n->user_data);

                if (event.character != 0 && event.character >= 32) {
                    if ((event.modifiers & platform::kModControl) == 0) {
                        ta->editor.insert(event.character);
                        tree_.mark_dirty(focused_node_, core::kLayout | core::kPaint);
                        if (ta->on_change) {
                            ta->on_change(ta->editor.text(), ta->change_user_data);
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
                            ta->editor.move_word_left(select);
                        else
                            ta->editor.move_cursor_left(select);
                        caret_moved = true;
                        break;
                    case VK_RIGHT:
                        if (ctrl)
                            ta->editor.move_word_right(select);
                        else
                            ta->editor.move_cursor_right(select);
                        caret_moved = true;
                        break;
                    case VK_UP:
                        ta->editor.move_cursor_up(select);
                        caret_moved = true;
                        break;
                    case VK_DOWN:
                        ta->editor.move_cursor_down(select);
                        caret_moved = true;
                        break;
                    case VK_HOME:
                        if (ctrl)
                            ta->editor.move_to_document_start(select);
                        else
                            ta->editor.move_to_line_start(select);
                        caret_moved = true;
                        break;
                    case VK_END:
                        if (ctrl)
                            ta->editor.move_to_document_end(select);
                        else
                            ta->editor.move_to_line_end(select);
                        caret_moved = true;
                        break;
                    case VK_PRIOR:
                        for (int i = 0; i < 5; ++i) {
                            ta->editor.move_cursor_up(select);
                        }
                        caret_moved = true;
                        break;
                    case VK_NEXT:
                        for (int i = 0; i < 5; ++i) {
                            ta->editor.move_cursor_down(select);
                        }
                        caret_moved = true;
                        break;
                    case VK_BACK:
                        changed = ta->editor.delete_backward();
                        break;
                    case VK_DELETE:
                        changed = ta->editor.delete_forward();
                        break;
                    case VK_RETURN:
                        changed = ta->editor.insert_newline();
                        break;
                    case VK_TAB:
                        changed = ta->editor.insert("    ");
                        break;
                    case 'A':
                        if (ctrl) {
                            ta->editor.select_all();
                            caret_moved = true;
                        }
                        break;
                    case 'C':
                        if (ctrl) {
                            ta->editor.copy_to_clipboard();
                        }
                        break;
                    case 'X':
                        if (ctrl) {
                            changed = ta->editor.cut_to_clipboard();
                        }
                        break;
                    case 'V':
                        if (ctrl) {
                            changed = ta->editor.paste_from_clipboard();
                        }
                        break;
                    case 'Z':
                        if (ctrl) {
                            if (event.modifiers & platform::kModShift) {
                                changed = ta->editor.redo();
                            } else {
                                changed = ta->editor.undo();
                            }
                        }
                        break;
                    case 'Y':
                        if (ctrl) {
                            changed = ta->editor.redo();
                        }
                        break;
                    default:
                        break;
                    }

                    if (changed) {
                        tree_.mark_dirty(focused_node_, core::kLayout | core::kPaint);
                        if (ta->on_change) {
                            ta->on_change(ta->editor.text(), ta->change_user_data);
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
            } else if (*kind == widget::WidgetKind::TabBar) {
                if (event.is_down) {
                    auto* tb = reinterpret_cast<widget::TabBarData*>(n->user_data);
                    if (!tb->tabs.empty()) {
                        if (event.key_code == VK_LEFT) {
                            size_t prev = (tb->active_index == 0) ? (tb->tabs.size() - 1)
                                                                  : (tb->active_index - 1);
                            tb->select_tab(prev);
                            return;
                        } else if (event.key_code == VK_RIGHT) {
                            size_t next = (tb->active_index + 1) % tb->tabs.size();
                            tb->select_tab(next);
                            return;
                        } else if (event.key_code == VK_HOME) {
                            tb->select_tab(0);
                            return;
                        } else if (event.key_code == VK_END) {
                            tb->select_tab(tb->tabs.size() - 1);
                            return;
                        } else if (event.key_code == VK_DELETE) {
                            tb->close_tab(tb->active_index);
                            return;
                        }
                    }
                }
            } else if (*kind == widget::WidgetKind::SegmentedButton) {
                if (event.is_down) {
                    auto* sb = reinterpret_cast<widget::SegmentedButtonData*>(n->user_data);
                    if (!sb->segments.empty()) {
                        if (event.key_code == VK_LEFT) {
                            for (size_t step = 1; step <= sb->segments.size(); ++step) {
                                size_t cand = (sb->focused_index + sb->segments.size() - step) %
                                              sb->segments.size();
                                if (!sb->segments[cand].disabled) {
                                    sb->focused_index = cand;
                                    if (!sb->multi_select) {
                                        sb->toggle_segment(cand);
                                    } else {
                                        tree_.mark_dirty(focused_node_, core::kPaint);
                                    }
                                    break;
                                }
                            }
                            return;
                        } else if (event.key_code == VK_RIGHT) {
                            for (size_t step = 1; step <= sb->segments.size(); ++step) {
                                size_t cand = (sb->focused_index + step) % sb->segments.size();
                                if (!sb->segments[cand].disabled) {
                                    sb->focused_index = cand;
                                    if (!sb->multi_select) {
                                        sb->toggle_segment(cand);
                                    } else {
                                        tree_.mark_dirty(focused_node_, core::kPaint);
                                    }
                                    break;
                                }
                            }
                            return;
                        } else if (event.key_code == VK_SPACE || event.key_code == VK_RETURN) {
                            sb->toggle_segment(sb->focused_index);
                            return;
                        }
                    }
                }
            } else if (*kind == widget::WidgetKind::Accordion) {
                if (event.is_down) {
                    auto* acc = reinterpret_cast<widget::AccordionData*>(n->user_data);
                    if (!acc->items.empty()) {
                        if (event.key_code == VK_UP) {
                            acc->focused_header = (acc->focused_header == 0)
                                                      ? (acc->items.size() - 1)
                                                      : (acc->focused_header - 1);
                            tree_.mark_dirty(focused_node_, core::kPaint);
                            return;
                        } else if (event.key_code == VK_DOWN) {
                            acc->focused_header = (acc->focused_header + 1) % acc->items.size();
                            tree_.mark_dirty(focused_node_, core::kPaint);
                            return;
                        } else if (event.key_code == VK_SPACE || event.key_code == VK_RETURN) {
                            acc->toggle_item(acc->focused_header);
                            return;
                        }
                    }
                }
            } else if (*kind == widget::WidgetKind::TreeView) {
                if (event.is_down) {
                    auto* tv = reinterpret_cast<widget::TreeViewData*>(n->user_data);
                    if (!tv->visible_rows.empty()) {
                        size_t cur_row = SIZE_MAX;
                        for (size_t i = 0; i < tv->visible_rows.size(); ++i) {
                            if (tv->visible_rows[i].item && tv->visible_rows[i].item->id == tv->selected_id) {
                                cur_row = i;
                                break;
                            }
                        }
                        if (cur_row == SIZE_MAX) {
                            cur_row = 0;
                        }

                        if (event.key_code == VK_UP) {
                            size_t next_r = (cur_row == 0) ? 0 : (cur_row - 1);
                            tv->select_node(tv->visible_rows[next_r].item->id);
                            tv->scroll_into_view(next_r, n->layout.size.height);
                            return;
                        } else if (event.key_code == VK_DOWN) {
                            size_t next_r = (std::min)(tv->visible_rows.size() - 1, cur_row + 1);
                            tv->select_node(tv->visible_rows[next_r].item->id);
                            tv->scroll_into_view(next_r, n->layout.size.height);
                            return;
                        } else if (event.key_code == VK_RIGHT) {
                            auto* item = tv->visible_rows[cur_row].item;
                            if (item && item->expandable) {
                                if (!item->expanded) {
                                    tv->toggle_node(item->id);
                                } else if (cur_row + 1 < tv->visible_rows.size()) {
                                    tv->select_node(tv->visible_rows[cur_row + 1].item->id);
                                    tv->scroll_into_view(cur_row + 1, n->layout.size.height);
                                }
                            }
                            return;
                        } else if (event.key_code == VK_LEFT) {
                            auto* item = tv->visible_rows[cur_row].item;
                            if (item && item->expandable && item->expanded) {
                                tv->toggle_node(item->id);
                            } else if (tv->visible_rows[cur_row].parent_id != 0) {
                                tv->select_node(tv->visible_rows[cur_row].parent_id);
                                for (size_t i = 0; i < tv->visible_rows.size(); ++i) {
                                    if (tv->visible_rows[i].item && tv->visible_rows[i].item->id == tv->visible_rows[cur_row].parent_id) {
                                        tv->scroll_into_view(i, n->layout.size.height);
                                        break;
                                    }
                                }
                            }
                            return;
                        } else if (event.key_code == VK_HOME) {
                            tv->select_node(tv->visible_rows[0].item->id);
                            tv->scroll_into_view(0, n->layout.size.height);
                            return;
                        } else if (event.key_code == VK_END) {
                            size_t last_r = tv->visible_rows.size() - 1;
                            tv->select_node(tv->visible_rows[last_r].item->id);
                            tv->scroll_into_view(last_r, n->layout.size.height);
                            return;
                        } else if (event.key_code == VK_SPACE) {
                            auto* item = tv->visible_rows[cur_row].item;
                            if (item && item->expandable) {
                                tv->toggle_node(item->id);
                            }
                            return;
                        } else if (event.key_code == VK_RETURN) {
                            if (tv->on_activate) {
                                tv->on_activate(tv->selected_id, tv->activate_user_data);
                            }
                            return;
                        }
                    }
                }
            } else if (*kind == widget::WidgetKind::DataTable) {
                if (event.is_down) {
                    auto* dt = reinterpret_cast<widget::DataTableData*>(n->user_data);
                    if (!dt->rows.empty()) {
                        size_t cur_r = (dt->selected_row < dt->rows.size()) ? dt->selected_row : 0;
                        if (event.key_code == VK_UP) {
                            size_t next_r = (cur_r == 0) ? 0 : (cur_r - 1);
                            dt->select_row(next_r);
                            dt->scroll_into_view(next_r, n->layout.size.height);
                            return;
                        } else if (event.key_code == VK_DOWN) {
                            size_t next_r = (std::min)(dt->rows.size() - 1, cur_r + 1);
                            dt->select_row(next_r);
                            dt->scroll_into_view(next_r, n->layout.size.height);
                            return;
                        } else if (event.key_code == VK_PRIOR) {
                            size_t page = static_cast<size_t>((std::max)(1.0f, (n->layout.size.height - dt->header_height) / dt->row_height));
                            size_t next_r = (cur_r > page) ? (cur_r - page) : 0;
                            dt->select_row(next_r);
                            dt->scroll_into_view(next_r, n->layout.size.height);
                            return;
                        } else if (event.key_code == VK_NEXT) {
                            size_t page = static_cast<size_t>((std::max)(1.0f, (n->layout.size.height - dt->header_height) / dt->row_height));
                            size_t next_r = (std::min)(dt->rows.size() - 1, cur_r + page);
                            dt->select_row(next_r);
                            dt->scroll_into_view(next_r, n->layout.size.height);
                            return;
                        } else if (event.key_code == VK_HOME) {
                            dt->select_row(0);
                            dt->scroll_into_view(0, n->layout.size.height);
                            return;
                        } else if (event.key_code == VK_END) {
                            size_t last_r = dt->rows.size() - 1;
                            dt->select_row(last_r);
                            dt->scroll_into_view(last_r, n->layout.size.height);
                            return;
                        } else if (event.key_code == VK_RETURN) {
                            if (dt->on_activate && dt->selected_row < dt->sorted_indices.size()) {
                                size_t act_idx = dt->sorted_indices[dt->selected_row];
                                dt->on_activate(dt->selected_row, dt->rows[act_idx].id, dt->activate_user_data);
                            }
                            return;
                        }
                    }
                }
            } else if (*kind == widget::WidgetKind::CommandPalette) {
                if (event.is_down) {
                    auto* cp = reinterpret_cast<widget::CommandPaletteData*>(n->user_data);
                    if (event.key_code == VK_UP) {
                        cp->select_prev();
                        return;
                    } else if (event.key_code == VK_DOWN) {
                        cp->select_next();
                        return;
                    } else if (event.key_code == VK_PRIOR) {
                        for (int k = 0; k < 5; ++k) {
                            cp->select_prev();
                        }
                        return;
                    } else if (event.key_code == VK_NEXT) {
                        for (int k = 0; k < 5; ++k) {
                            cp->select_next();
                        }
                        return;
                    } else if (event.key_code == VK_RETURN) {
                        cp->execute_selected();
                        return;
                    } else if (event.key_code == VK_ESCAPE) {
                        if (cp->on_dismiss) {
                            cp->on_dismiss(cp->dismiss_user_data);
                        }
                        if (overlay_manager_ && cp->active_overlay_id != overlay::kInvalidOverlayId) {
                            overlay_manager_->hide_overlay(cp->active_overlay_id);
                            cp->active_overlay_id = overlay::kInvalidOverlayId;
                        }
                        return;
                    } else if (event.key_code == VK_BACK) {
                        if (!cp->query.empty()) {
                            cp->query.pop_back();
                            cp->refilter();
                        }
                        return;
                    } else if (event.character >= 32 && event.character < 127) {
                        cp->query.push_back(static_cast<char>(event.character));
                        cp->refilter();
                        return;
                    }
                }
            } else if (*kind == widget::WidgetKind::Select) {
                if (event.is_down) {
                    auto* sel = reinterpret_cast<widget::SelectData*>(n->user_data);
                    if (event.key_code == VK_SPACE || event.key_code == VK_RETURN) {
                        sel->toggle_dropdown();
                        return;
                    } else if (event.key_code == VK_ESCAPE) {
                        sel->close_dropdown();
                        return;
                    } else if (event.key_code == VK_UP) {
                        if (sel->selected_index > 0) {
                            sel->select_by_index(sel->selected_index - 1);
                        }
                        return;
                    } else if (event.key_code == VK_DOWN) {
                        if (sel->selected_index + 1 < sel->items.size()) {
                            sel->select_by_index(sel->selected_index + 1);
                        }
                        return;
                    }
                }
            } else if (*kind == widget::WidgetKind::ComboBox) {
                if (event.is_down) {
                    auto* cb = reinterpret_cast<widget::ComboBoxData*>(n->user_data);
                    if (event.key_code == VK_ESCAPE) {
                        cb->close_dropdown();
                        return;
                    } else if (event.key_code == VK_UP) {
                        if (cb->is_open && cb->dropdown_selected_index > 0) {
                            cb->dropdown_selected_index--;
                            tree_.mark_dirty(focused_node_, core::kPaint);
                        }
                        return;
                    } else if (event.key_code == VK_DOWN) {
                        if (!cb->is_open) {
                            cb->open_dropdown();
                        } else if (cb->dropdown_selected_index + 1 < cb->filtered_indices.size()) {
                            cb->dropdown_selected_index++;
                            tree_.mark_dirty(focused_node_, core::kPaint);
                        }
                        return;
                    } else if (event.key_code == VK_RETURN) {
                        if (cb->is_open) {
                            cb->select_by_index(cb->dropdown_selected_index);
                        }
                        return;
                    } else if (event.key_code == VK_BACK) {
                        if (!cb->text.empty()) {
                            cb->text.pop_back();
                            cb->cursor_pos = cb->text.size();
                            cb->refilter();
                            if (cb->on_text_change) {
                                cb->on_text_change(cb->text, cb->text_user_data);
                            }
                            tree_.mark_dirty(focused_node_, core::kPaint);
                        }
                        return;
                    } else if (event.character >= 32 && event.character < 127) {
                        cb->text.push_back(static_cast<char>(event.character));
                        cb->cursor_pos = cb->text.size();
                        cb->refilter();
                        if (!cb->is_open) {
                            cb->open_dropdown();
                        }
                        if (cb->on_text_change) {
                            cb->on_text_change(cb->text, cb->text_user_data);
                        }
                        tree_.mark_dirty(focused_node_, core::kPaint);
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
            } else if (*kind == widget::WidgetKind::TextArea) {
                auto* ta = reinterpret_cast<widget::TextAreaData*>(n->user_data);
                ta->composition_text = std::string(comp_str);
                ta->composition_cursor = cursor_pos;
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
            } else if (*kind == widget::WidgetKind::TextArea) {
                auto* ta = reinterpret_cast<widget::TextAreaData*>(n->user_data);
                ta->composition_text.clear();
                ta->editor.insert(result_str);
                tree_.mark_dirty(focused_node_, core::kLayout | core::kPaint);
                if (ta->on_change) {
                    ta->on_change(ta->editor.text(), ta->change_user_data);
                }
                update_ime_window_position();
            }
        }
    }
    if (fallback_listener_) {
        fallback_listener_->on_ime_commit(result_str);
    }
}

} // namespace north::event
