#include "north/event/input_dispatcher.hpp"

#include "north/widget/widget_data.hpp"

#include <algorithm>

namespace north::event {

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
            } else if (*kind == widget::WidgetKind::TreeView) {
                auto* tv = reinterpret_cast<widget::TreeViewData*>(n->user_data);
                float new_y = std::clamp(tv->scroll_y - delta_y, 0.0f, tv->max_scroll_y);
                if (new_y != tv->scroll_y) {
                    tv->scroll_y = new_y;
                    tree_.mark_dirty(curr, core::kPaint);
                }
                return;
            } else if (*kind == widget::WidgetKind::DataTable) {
                auto* dt = reinterpret_cast<widget::DataTableData*>(n->user_data);
                float new_y = std::clamp(dt->scroll_y - delta_y, 0.0f, dt->max_scroll_y);
                if (new_y != dt->scroll_y) {
                    dt->scroll_y = new_y;
                    tree_.mark_dirty(curr, core::kPaint);
                }
                return;
            } else if (*kind == widget::WidgetKind::CommandPalette) {
                auto* cp = reinterpret_cast<widget::CommandPaletteData*>(n->user_data);
                float new_y = std::clamp(cp->scroll_y - delta_y, 0.0f, cp->max_scroll_y);
                if (new_y != cp->scroll_y) {
                    cp->scroll_y = new_y;
                    tree_.mark_dirty(curr, core::kPaint);
                }
                return;
            } else if (*kind == widget::WidgetKind::Select) {
                auto* sel = reinterpret_cast<widget::SelectData*>(n->user_data);
                float new_y = std::clamp(sel->scroll_y - delta_y, 0.0f, sel->max_scroll_y);
                if (new_y != sel->scroll_y) {
                    sel->scroll_y = new_y;
                    tree_.mark_dirty(curr, core::kPaint);
                }
                return;
            } else if (*kind == widget::WidgetKind::ComboBox) {
                auto* cb = reinterpret_cast<widget::ComboBoxData*>(n->user_data);
                float new_y = std::clamp(cb->scroll_y - delta_y, 0.0f, cb->max_scroll_y);
                if (new_y != cb->scroll_y) {
                    cb->scroll_y = new_y;
                    tree_.mark_dirty(curr, core::kPaint);
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
