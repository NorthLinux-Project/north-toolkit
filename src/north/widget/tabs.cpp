#include "north/widget/tabs.hpp"

#include "north/render/display_list.hpp"
#include "north/render/svg_path.hpp"
#include "north/text/font_manager.hpp"
#include "north/text/paragraph.hpp"
#include "north/text/shaper.hpp"

#include <algorithm>
#include <cmath>

namespace north::widget {

namespace {

void paint_cross_icon(render::DisplayList& dl, core::Rect rect, core::Color color) {
    float cx = rect.x + rect.width * 0.5f;
    float cy = rect.y + rect.height * 0.5f;
    float half = 3.5f;

    render::Path cross;
    cross.move_to(cx - half, cy - half);
    cross.line_to(cx + half, cy + half);
    cross.move_to(cx + half, cy - half);
    cross.line_to(cx - half, cy + half);

    render::StrokeStyle stroke{.width = 1.5f, .cap = render::LineCap::Round};
    dl.push_path_stroke(cross, color, stroke);
}

void paint_plus_icon(render::DisplayList& dl, core::Rect rect, core::Color color) {
    float cx = rect.x + rect.width * 0.5f;
    float cy = rect.y + rect.height * 0.5f;
    float half = 5.0f;

    render::Path plus;
    plus.move_to(cx - half, cy);
    plus.line_to(cx + half, cy);
    plus.move_to(cx, cy - half);
    plus.line_to(cx, cy + half);

    render::StrokeStyle stroke{.width = 1.6f, .cap = render::LineCap::Round};
    dl.push_path_stroke(plus, color, stroke);
}

void paint_chevron_horizontal(render::DisplayList& dl, core::Rect rect, bool left,
                              core::Color color) {
    float cx = rect.x + rect.width * 0.5f;
    float cy = rect.y + rect.height * 0.5f;
    float dx = left ? -2.5f : 2.5f;

    render::Path chev;
    chev.move_to(cx - dx, cy - 4.5f);
    chev.line_to(cx + dx, cy);
    chev.line_to(cx - dx, cy + 4.5f);

    render::StrokeStyle stroke{
        .width = 1.6f, .cap = render::LineCap::Round, .join = render::LineJoin::Round};
    dl.push_path_stroke(chev, color, stroke);
}

core::Size tab_bar_layout_fn(core::NodeTree&, core::Handle<core::Node>,
                             const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<TabBarData*>(user_data);
    if (!data) {
        return constraints.constrain({0.0f, 40.0f});
    }

    float cur_x = 0.0f;
    for (auto& tab : data->tabs) {
        float text_w = 0.0f;
        if (data->font && !tab.label.empty()) {
            text_w = text::Shaper::shape_text(tab.label, data->font, data->font_size).total_advance;
        } else {
            text_w = static_cast<float>(tab.label.size()) * (data->font_size * 0.55f);
        }

        float extra_w = data->tab_padding.horizontal();
        if (!tab.icon_path.empty()) {
            extra_w += 20.0f;
        }
        if (tab.show_badge) {
            extra_w += tab.badge_text.empty() ? 14.0f : 24.0f;
        }
        if (tab.closable) {
            extra_w += 22.0f;
        }

        float tab_w = std::clamp(text_w + extra_w, data->tab_min_width, data->tab_max_width);
        tab.computed_x = cur_x;
        tab.computed_width = tab_w;

        if (tab.closable) {
            float close_sz = 18.0f;
            float close_x = cur_x + tab_w - data->tab_padding.right - close_sz + 4.0f;
            float close_y = (data->bar_height - close_sz) * 0.5f;
            tab.close_rect = core::Rect{close_x, close_y, close_sz, close_sz};
        } else {
            tab.close_rect = core::Rect{0.0f, 0.0f, 0.0f, 0.0f};
        }

        cur_x += tab_w;
    }

    data->total_tabs_width = cur_x;

    float add_btn_w = data->show_add_button ? 34.0f : 0.0f;
    float desired_w = (constraints.max_width < 100000.0f)
                          ? constraints.max_width
                          : (data->total_tabs_width + add_btn_w);
    core::Size resolved = constraints.constrain({desired_w, data->bar_height});

    float available_for_tabs = resolved.width - add_btn_w;
    if (data->total_tabs_width > available_for_tabs && resolved.width > 80.0f) {
        data->has_overflow = true;
        float scroll_btns_w = 56.0f;
        float strip_w = (std::max)(0.0f, available_for_tabs - scroll_btns_w);
        data->max_scroll_offset = (std::max)(0.0f, data->total_tabs_width - strip_w);
        data->scroll_offset = std::clamp(data->scroll_offset, 0.0f, data->max_scroll_offset);

        float right_edge = resolved.width - add_btn_w;
        data->left_scroll_rect = core::Rect{right_edge - 56.0f, 4.0f, 26.0f, resolved.height - 8.0f};
        data->right_scroll_rect = core::Rect{right_edge - 28.0f, 4.0f, 26.0f, resolved.height - 8.0f};
    } else {
        data->has_overflow = false;
        data->max_scroll_offset = 0.0f;
        data->scroll_offset = 0.0f;
        data->left_scroll_rect = {};
        data->right_scroll_rect = {};
    }

    if (data->show_add_button) {
        float add_x = data->has_overflow
                          ? (resolved.width - add_btn_w)
                          : (std::min)(data->total_tabs_width + 4.0f, resolved.width - add_btn_w);
        data->add_button_rect = core::Rect{add_x, 6.0f, 28.0f, resolved.height - 12.0f};
    } else {
        data->add_button_rect = {};
    }

    if (!data->tabs.empty()) {
        if (data->active_index >= data->tabs.size()) {
            data->active_index = data->tabs.size() - 1;
        }
        const auto& active_tab = data->tabs[data->active_index];
        if (!data->indicator_initialized) {
            data->indicator_x.set_immediate(active_tab.computed_x);
            data->indicator_width.set_immediate(active_tab.computed_width);
            data->indicator_initialized = true;
        } else {
            if (std::abs(data->indicator_x.target() - active_tab.computed_x) > 0.5f) {
                data->indicator_x.tween_to(active_tab.computed_x, 0.16f,
                                           animation::CubicBezier::ease_out());
            }
            if (std::abs(data->indicator_width.target() - active_tab.computed_width) > 0.5f) {
                data->indicator_width.tween_to(active_tab.computed_width, 0.16f,
                                               animation::CubicBezier::ease_out());
            }
        }
    }

    return resolved;
}

void paint_single_tab(render::DisplayList& dl, const TabBarData& data, const TabItem& tab,
                      size_t idx, float visual_x, core::Point origin, float bar_h) {
    bool is_active = (idx == data.active_index);
    bool is_hovered = (idx == data.hovered_index);

    core::Rect tab_rect{origin.x + visual_x, origin.y + 4.0f, tab.computed_width - 2.0f,
                        bar_h - 6.0f};

    if (is_active) {
        dl.push_quad(tab_rect, core::CornerRadii{6.0f, 6.0f, 0.0f, 0.0f}, data.active_tab_bg,
                     data.border_color, core::Edges{1.0f, 1.0f, 0.0f, 1.0f});
    } else if (is_hovered) {
        dl.push_quad(tab_rect, core::CornerRadii{6.0f, 6.0f, 0.0f, 0.0f}, data.hovered_tab_bg,
                     core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
    }

    core::Color content_color = is_active ? data.active_text_color : data.inactive_text_color;
    float cursor_x = origin.x + visual_x + data.tab_padding.left;

    if (!tab.icon_path.empty()) {
        core::Rect b = tab.icon_path.bounds();
        float icon_sz = 14.0f;
        float sx = (b.width > 1e-4f) ? (icon_sz / b.width) : 1.0f;
        float sy = (b.height > 1e-4f) ? (icon_sz / b.height) : 1.0f;
        render::Path p = tab.icon_path;
        p.translate({-b.x, -b.y});
        p.scale(sx, sy);
        p.translate({cursor_x, origin.y + (bar_h - icon_sz) * 0.5f});
        dl.push_path_fill(p, content_color);
        cursor_x += icon_sz + 6.0f;
    }

    if (data.font && !tab.label.empty()) {
        float line_h = data.font->line_height(data.font_size);
        float text_y = origin.y + std::floor((bar_h - line_h) * 0.5f);
        float max_label_w = (std::max)(
            10.0f, (origin.x + visual_x + tab.computed_width) - cursor_x -
                       (tab.closable ? 24.0f : 8.0f) - (tab.show_badge ? 14.0f : 0.0f));

        text::ParagraphBuilder pb(text::TextAlign::Left);
        pb.add_text(tab.label, text::TextStyle{.font = data.font,
                                               .font_size = data.font_size,
                                               .color = content_color});
        auto para = pb.build();
        para->layout(layout::BoxConstraints::loose({max_label_w, line_h}));
        if (dl.glyph_atlas()) {
            para->paint(dl, *dl.glyph_atlas(), {cursor_x, text_y}, content_color);
        }
        cursor_x += para->size().width + 6.0f;
    }

    if (tab.show_badge) {
        float badge_r = 4.0f;
        float bx = (tab.closable)
                       ? (origin.x + visual_x + tab.close_rect.x - tab.computed_x - 12.0f)
                       : (origin.x + visual_x + tab.computed_width - data.tab_padding.right - 8.0f);
        float by = origin.y + (bar_h - badge_r * 2.0f) * 0.5f;
        core::Rect badge_rect{bx, by, badge_r * 2.0f, badge_r * 2.0f};
        dl.push_quad(badge_rect, core::CornerRadii::uniform(badge_r), tab.badge_color,
                     core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
    }

    if (tab.closable) {
        core::Rect c_rect{origin.x + visual_x + (tab.close_rect.x - tab.computed_x),
                          origin.y + tab.close_rect.y, tab.close_rect.width, tab.close_rect.height};
        if (idx == data.close_hovered_index) {
            dl.push_quad(c_rect, core::CornerRadii::uniform(4.0f), data.close_hover_bg,
                         core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        }
        paint_cross_icon(dl, c_rect, content_color);
    }
}

void tab_bar_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                      void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    auto* data = reinterpret_cast<TabBarData*>(user_data);
    auto* node = tree.get(handle);
    if (!dl || !data || !node) {
        return;
    }

    core::Rect bounds{origin.x, origin.y, node->layout.size.width, node->layout.size.height};
    dl->push_quad(bounds, core::CornerRadii::uniform(0.0f), data->background_color,
                  data->border_color, core::Edges{0.0f, 0.0f, 1.0f, 0.0f});

    float strip_w = bounds.width;
    if (data->show_add_button) {
        strip_w -= 34.0f;
    }
    if (data->has_overflow) {
        strip_w -= 56.0f;
    }
    strip_w = (std::max)(0.0f, strip_w);

    core::Rect strip_clip{origin.x, origin.y, strip_w, bounds.height};
    dl->push_clip(strip_clip);

    float drag_w = (data->is_dragging && data->drag_index < data->tabs.size())
                       ? data->tabs[data->drag_index].computed_width
                       : 0.0f;

    for (size_t i = 0; i < data->tabs.size(); ++i) {
        if (data->is_dragging && data->drag_threshold_exceeded && i == data->drag_index) {
            continue;
        }

        float visual_x = data->tabs[i].computed_x - data->scroll_offset;
        if (data->is_dragging && data->drag_threshold_exceeded &&
            data->drop_target_index < data->tabs.size()) {
            if (data->drag_index < data->drop_target_index && i > data->drag_index &&
                i <= data->drop_target_index) {
                visual_x -= drag_w;
            } else if (data->drag_index > data->drop_target_index && i < data->drag_index &&
                       i >= data->drop_target_index) {
                visual_x += drag_w;
            }
        }

        paint_single_tab(*dl, *data, data->tabs[i], i, visual_x, origin, bounds.height);
    }

    if (!data->tabs.empty() && data->active_index < data->tabs.size()) {
        float ind_x = data->indicator_x.get() - data->scroll_offset;
        float ind_w = data->indicator_width.get();
        if (data->is_dragging && data->drag_threshold_exceeded &&
            data->drag_index == data->active_index) {
            ind_x = data->drag_current_mouse_x - data->drag_grab_offset_x;
        }
        core::Rect ind_rect{origin.x + ind_x + 4.0f, origin.y + bounds.height - 3.0f,
                            (std::max)(8.0f, ind_w - 8.0f), 3.0f};
        dl->push_quad(ind_rect, core::CornerRadii{2.0f, 2.0f, 0.0f, 0.0f}, data->indicator_color,
                      core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
    }

    if (data->is_dragging && data->drag_threshold_exceeded &&
        data->drag_index < data->tabs.size()) {
        float dragged_x = data->drag_current_mouse_x - data->drag_grab_offset_x;
        core::Rect shadow_rect{origin.x + dragged_x, origin.y + 2.0f,
                               data->tabs[data->drag_index].computed_width - 2.0f,
                               bounds.height - 4.0f};
        dl->push_shadow(shadow_rect, core::CornerRadii{6.0f, 6.0f, 0.0f, 0.0f},
                        core::Color{0.0f, 0.0f, 0.0f, 0.45f}, 8.0f, 0.0f, {0.0f, 2.0f});
        paint_single_tab(*dl, *data, data->tabs[data->drag_index], data->drag_index, dragged_x,
                         origin, bounds.height);
    }

    dl->pop_clip();

    if (data->has_overflow) {
        core::Rect l_rect{origin.x + data->left_scroll_rect.x, origin.y + data->left_scroll_rect.y,
                          data->left_scroll_rect.width, data->left_scroll_rect.height};
        core::Rect r_rect{origin.x + data->right_scroll_rect.x,
                          origin.y + data->right_scroll_rect.y, data->right_scroll_rect.width,
                          data->right_scroll_rect.height};

        if (data->left_scroll_hovered) {
            dl->push_quad(l_rect, core::CornerRadii::uniform(4.0f), data->hovered_tab_bg,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        }
        if (data->right_scroll_hovered) {
            dl->push_quad(r_rect, core::CornerRadii::uniform(4.0f), data->hovered_tab_bg,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        }

        paint_chevron_horizontal(*dl, l_rect, true, data->active_text_color);
        paint_chevron_horizontal(*dl, r_rect, false, data->active_text_color);
    }

    if (data->show_add_button) {
        core::Rect add_rect{origin.x + data->add_button_rect.x, origin.y + data->add_button_rect.y,
                            data->add_button_rect.width, data->add_button_rect.height};
        if (data->add_button_hovered) {
            dl->push_quad(add_rect, core::CornerRadii::uniform(6.0f), data->hovered_tab_bg,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        }
        paint_plus_icon(*dl, add_rect, data->active_text_color);
    }
}

void tab_bar_destroy_fn(void* user_data) {
    delete reinterpret_cast<TabBarData*>(user_data);
}

core::Size tabs_view_layout_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                               const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<TabsViewData*>(user_data);
    auto* node = tree.get(handle);
    if (!data || !node) {
        return constraints.constrain({0.0f, 0.0f});
    }

    float target_w = (data->width > 0.0f) ? data->width : constraints.max_width;
    float target_h = (data->height > 0.0f) ? data->height : constraints.max_height;
    if (target_w >= 100000.0f) {
        target_w = 600.0f;
    }
    if (target_h >= 100000.0f) {
        target_h = 400.0f;
    }

    core::Size resolved = constraints.constrain({target_w, target_h});

    float bar_h = 40.0f;
    if (data->bar_node.is_valid()) {
        auto* b_node = tree.get(data->bar_node);
        if (b_node) {
            core::Size b_sz = tree.layout_subtree(
                data->bar_node,
                layout::BoxConstraints::tight({resolved.width, data->bar_data ? data->bar_data->bar_height : 40.0f}));
            b_node->layout.offset = {0.0f, 0.0f};
            bar_h = b_sz.height;
        }
    }

    float content_h = (std::max)(0.0f, resolved.height - bar_h);
    layout::BoxConstraints content_constraints =
        layout::BoxConstraints::tight({resolved.width, content_h});

    if (data->bar_data) {
        for (size_t i = 0; i < data->bar_data->tabs.size(); ++i) {
            auto ch = data->bar_data->tabs[i].content_node;
            if (!ch.is_valid()) {
                continue;
            }
            auto* ch_node = tree.get(ch);
            if (!ch_node) {
                continue;
            }
            if (i == data->bar_data->active_index) {
                tree.layout_subtree(ch, content_constraints);
                ch_node->layout.offset = {0.0f, bar_h};
            } else {
                ch_node->layout.size = {0.0f, 0.0f};
                ch_node->layout.offset = {0.0f, bar_h};
            }
        }
    }

    return resolved;
}

void tabs_view_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                        void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    auto* data = reinterpret_cast<TabsViewData*>(user_data);
    auto* node = tree.get(handle);
    if (!dl || !data || !node) {
        return;
    }

    float bar_h = data->bar_data ? data->bar_data->bar_height : 40.0f;
    core::Rect content_bounds{origin.x, origin.y + bar_h, node->layout.size.width,
                              (std::max)(0.0f, node->layout.size.height - bar_h)};

    dl->push_quad(content_bounds, core::CornerRadii::uniform(0.0f), data->content_bg,
                  data->border_color, core::Edges{0.0f, 1.0f, 1.0f, 1.0f});

    if (data->bar_node.is_valid()) {
        tree.paint_subtree(data->bar_node, origin, stream);
    }

    if (data->bar_data && data->bar_data->active_index < data->bar_data->tabs.size()) {
        auto active_content = data->bar_data->tabs[data->bar_data->active_index].content_node;
        if (active_content.is_valid()) {
            dl->push_clip(content_bounds);
            tree.paint_subtree(active_content, origin, stream);
            dl->pop_clip();
        }
    }
}

void tabs_view_destroy_fn(void* user_data) {
    delete reinterpret_cast<TabsViewData*>(user_data);
}

} // namespace

void TabBarData::select_tab(size_t index) {
    if (index >= tabs.size()) {
        return;
    }
    bool changed = (active_index != index);
    active_index = index;

    const auto& active_tab = tabs[active_index];
    indicator_x.tween_to(active_tab.computed_x, 0.16f, animation::CubicBezier::ease_out());
    indicator_width.tween_to(active_tab.computed_width, 0.16f, animation::CubicBezier::ease_out());

    if (has_overflow) {
        if (active_tab.computed_x < scroll_offset) {
            scroll_offset = active_tab.computed_x;
        } else if (active_tab.computed_x + active_tab.computed_width >
                   scroll_offset + (total_tabs_width - max_scroll_offset)) {
            float visible_w = total_tabs_width - max_scroll_offset;
            scroll_offset = (std::min)(max_scroll_offset,
                                       active_tab.computed_x + active_tab.computed_width - visible_w);
        }
    }

    if (parent_tabs_view) {
        parent_tabs_view->sync_active_content();
    }

    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kLayout | core::kPaint);
    }

    if (changed && on_select) {
        on_select(active_index, select_user_data);
    }
}

bool TabBarData::close_tab(size_t index) {
    if (index >= tabs.size() || !tabs[index].closable) {
        return false;
    }

    if (on_close) {
        on_close(index, close_user_data);
    }

    if (tabs[index].content_node.is_valid() && tree) {
        tree->destroy_node(tabs[index].content_node);
    }

    tabs.erase(tabs.begin() + static_cast<std::ptrdiff_t>(index));

    if (tabs.empty()) {
        active_index = 0;
    } else if (active_index >= tabs.size()) {
        active_index = tabs.size() - 1;
    } else if (index < active_index) {
        --active_index;
    }

    hovered_index = SIZE_MAX;
    close_hovered_index = SIZE_MAX;

    if (parent_tabs_view) {
        parent_tabs_view->sync_active_content();
    }

    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kLayout | core::kPaint);
    }

    return true;
}

bool TabBarData::reorder_tab(size_t from_index, size_t to_index) {
    if (!reorderable || from_index >= tabs.size() || to_index >= tabs.size() ||
        from_index == to_index) {
        return false;
    }

    TabItem moved = std::move(tabs[from_index]);
    tabs.erase(tabs.begin() + static_cast<std::ptrdiff_t>(from_index));
    tabs.insert(tabs.begin() + static_cast<std::ptrdiff_t>(to_index), std::move(moved));

    if (active_index == from_index) {
        active_index = to_index;
    } else if (from_index < active_index && to_index >= active_index) {
        --active_index;
    } else if (from_index > active_index && to_index <= active_index) {
        ++active_index;
    }

    if (parent_tabs_view) {
        parent_tabs_view->sync_active_content();
    }

    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kLayout | core::kPaint);
    }

    if (on_reorder) {
        on_reorder(from_index, to_index, reorder_user_data);
    }

    return true;
}

void TabBarData::apply_theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    background_color = c.surface;
    active_tab_bg = c.surface_variant;
    hovered_tab_bg = core::Color{c.on_surface.r, c.on_surface.g, c.on_surface.b, 0.08f};
    active_text_color = c.on_surface;
    inactive_text_color = c.on_surface_variant;
    indicator_color = c.primary;
    border_color = c.outline_variant;

    if (on_theme) {
        on_theme(theme, *this);
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

void TabsViewData::sync_active_content() {
    if (!bar_data || !tree || !node_handle.is_valid()) {
        return;
    }

    if (retention_policy == TabRetentionPolicy::Lazy) {
        for (size_t i = 0; i < bar_data->tabs.size(); ++i) {
            auto& tab = bar_data->tabs[i];
            if (i == bar_data->active_index) {
                if (!tab.content_node.is_valid() && tab.build_fn) {
                    tab.content_node = tab.build_fn(*tree, i, tab.user_data);
                    if (tab.content_node.is_valid()) {
                        tree->append_child(node_handle, tab.content_node);
                    }
                }
            } else {
                if (tab.content_node.is_valid() && tab.build_fn) {
                    tree->destroy_node(tab.content_node);
                    tab.content_node = {};
                }
            }
        }
    }

    tree->mark_dirty(node_handle, core::kLayout | core::kPaint);
}

void TabsViewData::apply_theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    content_bg = c.surface_variant;
    border_color = c.outline_variant;
    if (bar_data) {
        bar_data->apply_theme(theme);
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

TabBarBuilder::TabBarBuilder(core::NodeTree& tree) : tree_(tree) {}

TabBarBuilder& TabBarBuilder::tab(std::string_view label, bool closable, bool show_badge,
                                  std::string_view icon_svg) {
    TabItem item;
    item.label = std::string(label);
    item.closable = closable;
    item.show_badge = show_badge;
    if (!icon_svg.empty()) {
        (void)render::parse_svg_path(icon_svg, item.icon_path);
    }
    data_.tabs.push_back(std::move(item));
    return *this;
}

TabBarBuilder& TabBarBuilder::tab_with_badge(std::string_view label, std::string_view badge_text,
                                             core::Color badge_color, bool closable,
                                             std::string_view icon_svg) {
    TabItem item;
    item.label = std::string(label);
    item.closable = closable;
    item.show_badge = true;
    item.badge_text = std::string(badge_text);
    item.badge_color = badge_color;
    if (!icon_svg.empty()) {
        (void)render::parse_svg_path(icon_svg, item.icon_path);
    }
    data_.tabs.push_back(std::move(item));
    return *this;
}

TabBarBuilder& TabBarBuilder::active_tab(size_t index) {
    data_.active_index = index;
    return *this;
}

TabBarBuilder& TabBarBuilder::reorderable(bool enabled) {
    data_.reorderable = enabled;
    return *this;
}

TabBarBuilder& TabBarBuilder::add_button(bool show) {
    data_.show_add_button = show;
    return *this;
}

TabBarBuilder& TabBarBuilder::font(std::shared_ptr<text::Font> font) {
    data_.font = std::move(font);
    return *this;
}

TabBarBuilder& TabBarBuilder::font_size(float size) {
    data_.font_size = size;
    return *this;
}

TabBarBuilder& TabBarBuilder::bar_height(float height) {
    data_.bar_height = height;
    return *this;
}

TabBarBuilder& TabBarBuilder::tab_width_bounds(float min_w, float max_w) {
    data_.tab_min_width = min_w;
    data_.tab_max_width = max_w;
    return *this;
}

TabBarBuilder& TabBarBuilder::theme(const style::Theme& theme) {
    data_.apply_theme(theme);
    return *this;
}

TabBarBuilder& TabBarBuilder::theme_signal(core::Signal<style::Theme>* sig, TabBarThemeFn fn) {
    data_.theme_signal = sig;
    data_.on_theme = fn;
    if (sig) {
        data_.apply_theme(sig->get());
    }
    return *this;
}

TabBarBuilder& TabBarBuilder::on_select(TabSelectCallbackFn fn, void* user_data) {
    data_.on_select = fn;
    data_.select_user_data = user_data;
    return *this;
}

TabBarBuilder& TabBarBuilder::on_close(TabCloseCallbackFn fn, void* user_data) {
    data_.on_close = fn;
    data_.close_user_data = user_data;
    return *this;
}

TabBarBuilder& TabBarBuilder::on_add(ClickCallbackFn fn, void* user_data) {
    data_.on_add = fn;
    data_.add_user_data = user_data;
    return *this;
}

TabBarBuilder& TabBarBuilder::on_reorder(TabReorderCallbackFn fn, void* user_data) {
    data_.on_reorder = fn;
    data_.reorder_user_data = user_data;
    return *this;
}

core::Handle<core::Node> TabBarBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node) {
        return {};
    }

    if (!data_.font) {
        data_.font = text::FontManager::instance().load_system_font();
    }

    auto* heap_data = new TabBarData(std::move(data_));
    heap_data->tree = &tree_;
    heap_data->node_handle = handle;

    heap_data->indicator_x.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* tb = static_cast<TabBarData*>(ctx);
            if (tb->tree && tb->node_handle.is_valid()) {
                tb->tree->mark_dirty(tb->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    heap_data->indicator_width.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* tb = static_cast<TabBarData*>(ctx);
            if (tb->tree && tb->node_handle.is_valid()) {
                tb->tree->mark_dirty(tb->node_handle, flags);
            }
        },
        heap_data, core::kPaint);

    if (heap_data->theme_signal) {
        heap_data->theme_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* tb = static_cast<TabBarData*>(ctx);
                if (tb->theme_signal) {
                    tb->apply_theme(tb->theme_signal->get());
                }
            },
            heap_data, core::kPaint);
    }

    node->user_data = heap_data;
    node->layout_fn = &tab_bar_layout_fn;
    node->paint_fn = &tab_bar_paint_fn;
    node->destroy_fn = &tab_bar_destroy_fn;

    return handle;
}

TabsViewBuilder::TabsViewBuilder(core::NodeTree& tree) : tree_(tree) {}

TabsViewBuilder& TabsViewBuilder::tab(std::string_view label, core::Handle<core::Node> content_node,
                                      bool closable, bool show_badge, std::string_view icon_svg) {
    TabItem item;
    item.label = std::string(label);
    item.content_node = content_node;
    item.closable = closable;
    item.show_badge = show_badge;
    if (!icon_svg.empty()) {
        (void)render::parse_svg_path(icon_svg, item.icon_path);
    }
    bar_data_.tabs.push_back(std::move(item));
    return *this;
}

TabsViewBuilder& TabsViewBuilder::lazy_tab(std::string_view label, TabContentFactoryFn build_fn,
                                           void* user_data, bool closable, bool show_badge,
                                           std::string_view icon_svg) {
    TabItem item;
    item.label = std::string(label);
    item.build_fn = build_fn;
    item.user_data = user_data;
    item.closable = closable;
    item.show_badge = show_badge;
    if (!icon_svg.empty()) {
        (void)render::parse_svg_path(icon_svg, item.icon_path);
    }
    bar_data_.tabs.push_back(std::move(item));
    return *this;
}

TabsViewBuilder& TabsViewBuilder::retention_policy(TabRetentionPolicy policy) {
    view_data_.retention_policy = policy;
    return *this;
}

TabsViewBuilder& TabsViewBuilder::active_tab(size_t index) {
    bar_data_.active_index = index;
    return *this;
}

TabsViewBuilder& TabsViewBuilder::reorderable(bool enabled) {
    bar_data_.reorderable = enabled;
    return *this;
}

TabsViewBuilder& TabsViewBuilder::add_button(bool show) {
    bar_data_.show_add_button = show;
    return *this;
}

TabsViewBuilder& TabsViewBuilder::font(std::shared_ptr<text::Font> font) {
    bar_data_.font = std::move(font);
    return *this;
}

TabsViewBuilder& TabsViewBuilder::font_size(float size) {
    bar_data_.font_size = size;
    return *this;
}

TabsViewBuilder& TabsViewBuilder::bar_height(float height) {
    bar_data_.bar_height = height;
    return *this;
}

TabsViewBuilder& TabsViewBuilder::width(float width) {
    view_data_.width = width;
    return *this;
}

TabsViewBuilder& TabsViewBuilder::height(float height) {
    view_data_.height = height;
    return *this;
}

TabsViewBuilder& TabsViewBuilder::theme(const style::Theme& theme) {
    bar_data_.apply_theme(theme);
    view_data_.apply_theme(theme);
    return *this;
}

TabsViewBuilder& TabsViewBuilder::theme_signal(core::Signal<style::Theme>* sig) {
    view_data_.theme_signal = sig;
    if (sig) {
        theme(sig->get());
    }
    return *this;
}

TabsViewBuilder& TabsViewBuilder::on_select(TabSelectCallbackFn fn, void* user_data) {
    bar_data_.on_select = fn;
    bar_data_.select_user_data = user_data;
    return *this;
}

TabsViewBuilder& TabsViewBuilder::on_close(TabCloseCallbackFn fn, void* user_data) {
    bar_data_.on_close = fn;
    bar_data_.close_user_data = user_data;
    return *this;
}

TabsViewBuilder& TabsViewBuilder::on_add(ClickCallbackFn fn, void* user_data) {
    bar_data_.on_add = fn;
    bar_data_.add_user_data = user_data;
    return *this;
}

TabsViewBuilder& TabsViewBuilder::on_reorder(TabReorderCallbackFn fn, void* user_data) {
    bar_data_.on_reorder = fn;
    bar_data_.reorder_user_data = user_data;
    return *this;
}

core::Handle<core::Node> TabsViewBuilder::build() {
    auto view_handle = tree_.create_node();
    auto* view_node = tree_.get(view_handle);
    if (!view_node) {
        return {};
    }

    auto bar_handle = tree_.create_node();
    auto* bar_node = tree_.get(bar_handle);
    if (!bar_node) {
        tree_.destroy_node(view_handle);
        return {};
    }

    if (!bar_data_.font) {
        bar_data_.font = text::FontManager::instance().load_system_font();
    }

    auto* heap_bar = new TabBarData(std::move(bar_data_));
    auto* heap_view = new TabsViewData(std::move(view_data_));

    heap_bar->tree = &tree_;
    heap_bar->node_handle = bar_handle;
    heap_bar->parent_tabs_view = heap_view;

    heap_bar->indicator_x.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* tb = static_cast<TabBarData*>(ctx);
            if (tb->tree && tb->node_handle.is_valid()) {
                tb->tree->mark_dirty(tb->node_handle, flags);
            }
        },
        heap_bar, core::kPaint);

    heap_bar->indicator_width.bind_dirty(
        [](void* ctx, uint32_t flags) {
            auto* tb = static_cast<TabBarData*>(ctx);
            if (tb->tree && tb->node_handle.is_valid()) {
                tb->tree->mark_dirty(tb->node_handle, flags);
            }
        },
        heap_bar, core::kPaint);

    bar_node->user_data = heap_bar;
    bar_node->layout_fn = &tab_bar_layout_fn;
    bar_node->paint_fn = &tab_bar_paint_fn;
    bar_node->destroy_fn = &tab_bar_destroy_fn;

    tree_.append_child(view_handle, bar_handle);

    heap_view->tree = &tree_;
    heap_view->node_handle = view_handle;
    heap_view->bar_node = bar_handle;
    heap_view->bar_data = heap_bar;

    for (size_t i = 0; i < heap_bar->tabs.size(); ++i) {
        auto& tab = heap_bar->tabs[i];
        if (tab.content_node.is_valid()) {
            tree_.append_child(view_handle, tab.content_node);
        } else if (tab.build_fn &&
                   (heap_view->retention_policy == TabRetentionPolicy::KeepAlive ||
                    i == heap_bar->active_index)) {
            tab.content_node = tab.build_fn(tree_, i, tab.user_data);
            if (tab.content_node.is_valid()) {
                tree_.append_child(view_handle, tab.content_node);
            }
        }
    }

    if (heap_view->theme_signal) {
        heap_view->theme_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* tv = static_cast<TabsViewData*>(ctx);
                if (tv->theme_signal) {
                    tv->apply_theme(tv->theme_signal->get());
                }
            },
            heap_view, core::kPaint);
    }

    view_node->user_data = heap_view;
    view_node->layout_fn = &tabs_view_layout_fn;
    view_node->paint_fn = &tabs_view_paint_fn;
    view_node->destroy_fn = &tabs_view_destroy_fn;
    view_node->custom_child_paint = true;

    return view_handle;
}

} // namespace north::widget
