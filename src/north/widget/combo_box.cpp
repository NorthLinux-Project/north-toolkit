#include "north/widget/combo_box.hpp"

#include "north/render/display_list.hpp"
#include "north/text/font_manager.hpp"
#include "north/text/fuzzy_matcher.hpp"
#include "north/text/paragraph.hpp"

#include <algorithm>

namespace north::widget {

namespace {

core::Size combo_box_layout_fn(core::NodeTree&, core::Handle<core::Node>,
                               const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<ComboBoxData*>(user_data);
    if (!data) {
        return constraints.constrain({0.0f, 0.0f});
    }
    return constraints.constrain({data->width, data->height});
}

void combo_box_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                        void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    auto* data = reinterpret_cast<ComboBoxData*>(user_data);
    auto* node = tree.get(handle);
    if (!dl || !data || !node) {
        return;
    }

    core::Rect bounds{origin.x, origin.y, node->layout.size.width, node->layout.size.height};

    core::Color bg = data->is_hovered ? data->hover_bg : data->background_color;
    core::Color border = data->is_focused ? data->focus_border_color : data->border_color;
    float border_w = data->is_focused ? 1.5f : 1.0f;

    dl->push_quad(bounds, core::CornerRadii::uniform(6.0f), bg, border, core::Edges::uniform(border_w));

    float font_h = data->font_size * 1.3f;
    float text_y = bounds.y + (bounds.height - font_h) * 0.5f;
    float cursor_x = bounds.x + 10.0f;

    if (data->font) {
        if (!data->text.empty()) {
            text::ParagraphBuilder pb;
            pb.add_text(data->text, text::TextStyle{.font = data->font,
                                                    .font_size = data->font_size,
                                                    .color = data->text_color});
            auto para = pb.build();
            float avail_w = (std::max)(0.0f, bounds.width - 38.0f);
            para->layout(layout::BoxConstraints::loose({avail_w, font_h}));
            if (dl->glyph_atlas()) {
                para->paint(*dl, *dl->glyph_atlas(), {cursor_x, text_y}, data->text_color);
            }

            if (data->is_focused) {
                float caret_x = cursor_x + para->size().width + 1.0f;
                core::Rect caret_rect{caret_x, text_y, 1.5f, font_h};
                dl->push_quad(caret_rect, core::CornerRadii::uniform(0.0f), data->focus_border_color,
                              core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
            }
        } else {
            text::ParagraphBuilder pb;
            pb.add_text(data->placeholder, text::TextStyle{.font = data->font,
                                                           .font_size = data->font_size,
                                                           .color = data->secondary_text_color});
            auto para = pb.build();
            float avail_w = (std::max)(0.0f, bounds.width - 38.0f);
            para->layout(layout::BoxConstraints::loose({avail_w, font_h}));
            if (dl->glyph_atlas()) {
                para->paint(*dl, *dl->glyph_atlas(), {cursor_x, text_y}, data->secondary_text_color);
            }

            if (data->is_focused) {
                core::Rect caret_rect{cursor_x, text_y, 1.5f, font_h};
                dl->push_quad(caret_rect, core::CornerRadii::uniform(0.0f), data->focus_border_color,
                              core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
            }
        }
    }

    // Right-aligned Chevron Button
    float chev_x = bounds.x + bounds.width - 18.0f;
    float chev_y = bounds.y + (bounds.height - 8.0f) * 0.5f;
    render::Path chevron;
    if (data->is_open) {
        chevron.move_to({chev_x, chev_y + 6.0f});
        chevron.line_to({chev_x + 4.0f, chev_y + 1.0f});
        chevron.line_to({chev_x + 8.0f, chev_y + 6.0f});
    } else {
        chevron.move_to({chev_x, chev_y + 2.0f});
        chevron.line_to({chev_x + 4.0f, chev_y + 7.0f});
        chevron.line_to({chev_x + 8.0f, chev_y + 2.0f});
    }
    dl->push_path_stroke(chevron, data->chevron_color, render::StrokeStyle{1.5f});
}

void combo_box_destroy_fn(void* user_data) {
    auto* data = reinterpret_cast<ComboBoxData*>(user_data);
    if (data) {
        if (data->overlay_manager && data->active_overlay_id != overlay::kInvalidOverlayId) {
            overlay::OverlayId id_to_hide = data->active_overlay_id;
            data->active_overlay_id = overlay::kInvalidOverlayId;
            data->overlay_manager->hide_overlay(id_to_hide);
        }
        if (data->tree && data->popup_node.is_valid()) {
            data->tree->destroy_node(data->popup_node);
            data->popup_node = {};
        }
        delete data;
    }
}

core::Size combo_box_dropdown_layout_fn(core::NodeTree&, core::Handle<core::Node>,
                                        const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<ComboBoxData*>(user_data);
    if (!data) {
        return constraints.constrain({0.0f, 0.0f});
    }

    float total_h = static_cast<float>(data->filtered_indices.size()) * data->item_height;
    if (total_h > data->dropdown_max_height) {
        total_h = data->dropdown_max_height;
    }
    data->max_scroll_y = (std::max)(0.0f, static_cast<float>(data->filtered_indices.size()) * data->item_height - total_h);

    return constraints.constrain({data->width, total_h});
}

void combo_box_dropdown_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                                 void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    auto* data = reinterpret_cast<ComboBoxData*>(user_data);
    auto* node = tree.get(handle);
    if (!dl || !data || !node) {
        return;
    }

    core::Rect bounds{origin.x, origin.y, node->layout.size.width, node->layout.size.height};

    dl->push_shadow(bounds, core::CornerRadii::uniform(6.0f),
                    core::Color{0.0f, 0.0f, 0.0f, 0.30f}, 12.0f, 0.0f, {0.0f, 4.0f});
    dl->push_quad(bounds, core::CornerRadii::uniform(6.0f), data->dropdown_bg,
                  data->border_color, core::Edges::uniform(1.0f));

    dl->push_clip(bounds);

    float font_h = data->font_size * 1.3f;
    size_t start_row = static_cast<size_t>((std::max)(0.0f, data->scroll_y / data->item_height));
    size_t visible_count = static_cast<size_t>(bounds.height / data->item_height) + 2;
    size_t end_row = (std::min)(data->filtered_indices.size(), start_row + visible_count);

    for (size_t i = start_row; i < end_row; ++i) {
        size_t actual_idx = data->filtered_indices[i];
        if (actual_idx >= data->items.size()) {
            continue;
        }
        const auto& item = data->items[actual_idx];
        float row_y = bounds.y + static_cast<float>(i) * data->item_height - data->scroll_y;
        core::Rect row_rect{bounds.x + 2.0f, row_y + 1.0f, bounds.width - 4.0f, data->item_height - 2.0f};

        bool is_selected = (i == data->dropdown_selected_index);
        bool is_hovered = (i == data->hovered_index);

        if (is_selected) {
            dl->push_quad(row_rect, core::CornerRadii::uniform(4.0f), data->item_selected_bg,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        } else if (is_hovered) {
            dl->push_quad(row_rect, core::CornerRadii::uniform(4.0f), data->item_hover_bg,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        }

        float cursor_x = row_rect.x + 8.0f;

        if (!item.icon_path.empty()) {
            core::Rect icon_box{cursor_x, row_rect.y + (row_rect.height - 14.0f) * 0.5f, 14.0f, 14.0f};
            core::Rect b = item.icon_path.bounds();
            if (b.width > 0.0f && b.height > 0.0f) {
                float sx = icon_box.width / b.width;
                float sy = icon_box.height / b.height;
                render::Path scaled = item.icon_path;
                scaled.translate(core::Point{-b.x, -b.y});
                scaled.scale(sx, sy);
                scaled.translate(core::Point{icon_box.x, icon_box.y});
                dl->push_path_stroke(scaled, data->text_color, render::StrokeStyle{1.2f});
            }
            cursor_x += 20.0f;
        }

        if (data->font && !item.label.empty()) {
            float item_text_y = row_rect.y + (row_rect.height - font_h) * 0.5f;
            text::ParagraphBuilder pb;
            pb.add_text(item.label, text::TextStyle{.font = data->font,
                                                    .font_size = data->font_size,
                                                    .color = item.disabled ? data->secondary_text_color : data->text_color});
            auto para = pb.build();
            para->layout(layout::BoxConstraints::loose({(std::max)(0.0f, row_rect.width - (cursor_x - row_rect.x) - 8.0f), font_h}));
            if (dl->glyph_atlas()) {
                para->paint(*dl, *dl->glyph_atlas(), {cursor_x, item_text_y}, data->text_color);
            }
        }
    }

    if (data->max_scroll_y > 0.0f && bounds.height > 20.0f) {
        float track_h = bounds.height - 8.0f;
        float thumb_h = (std::max)(16.0f, track_h * (bounds.height / (bounds.height + data->max_scroll_y)));
        float thumb_y = bounds.y + 4.0f + (data->scroll_y / data->max_scroll_y) * (track_h - thumb_h);
        core::Rect thumb_rect{bounds.x + bounds.width - 5.0f, thumb_y, 3.0f, thumb_h};
        dl->push_quad(thumb_rect, core::CornerRadii::uniform(1.5f), data->secondary_text_color,
                      core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
    }

    dl->pop_clip();
}

} // namespace

void ComboBoxData::refilter() {
    filtered_indices.clear();
    for (size_t i = 0; i < items.size(); ++i) {
        if (text.empty()) {
            filtered_indices.push_back(i);
        } else {
            auto res = text::fuzzy_match(text, items[i].label);
            if (res.matched) {
                filtered_indices.push_back(i);
            }
        }
    }
    if (dropdown_selected_index >= filtered_indices.size()) {
        dropdown_selected_index = 0;
    }
    scroll_y = 0.0f;
}

void ComboBoxData::open_dropdown() {
    if (is_open) {
        return;
    }
    refilter();

    if (overlay_manager && tree) {
        popup_node = tree->create_node();
        auto* pnode = tree->get(popup_node);
        if (pnode) {
            pnode->user_data = this;
            pnode->layout_fn = &combo_box_dropdown_layout_fn;
            pnode->paint_fn = &combo_box_dropdown_paint_fn;

            overlay::OverlayConfig cfg;
            cfg.placement = overlay::Placement::BottomStart;
            cfg.offset = 4.0f;
            cfg.flip_on_collision = true;
            cfg.clamp_to_viewport = true;
            cfg.modal = true;
            cfg.barrier_dismissible = true;
            cfg.barrier_color = core::Color{0.0f, 0.0f, 0.0f, 0.0f};
            cfg.on_dismiss = [](void* ud) {
                static_cast<ComboBoxData*>(ud)->close_dropdown();
            };
            cfg.dismiss_user_data = this;

            active_overlay_id = overlay_manager->show_overlay(popup_node, cfg, node_handle);
            is_open = true;
            if (tree && node_handle.is_valid()) {
                tree->mark_dirty(node_handle, core::kPaint);
            }
        }
    } else {
        is_open = true;
        if (tree && node_handle.is_valid()) {
            tree->mark_dirty(node_handle, core::kPaint);
        }
    }
}

void ComboBoxData::close_dropdown() {
    if (!is_open) {
        return;
    }
    is_open = false;
    if (overlay_manager && active_overlay_id != overlay::kInvalidOverlayId) {
        overlay::OverlayId id_to_hide = active_overlay_id;
        active_overlay_id = overlay::kInvalidOverlayId;
        overlay_manager->hide_overlay(id_to_hide);
    }
    if (tree && popup_node.is_valid()) {
        tree->destroy_node(popup_node);
        popup_node = {};
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

void ComboBoxData::toggle_dropdown() {
    if (is_open) {
        close_dropdown();
    } else {
        open_dropdown();
    }
}

void ComboBoxData::select_by_index(size_t index) {
    if (index < filtered_indices.size()) {
        size_t actual_idx = filtered_indices[index];
        if (actual_idx < items.size()) {
            if (items[actual_idx].disabled) {
                return;
            }
            selected_id = items[actual_idx].id;
            text = items[actual_idx].label;
            cursor_pos = text.size();
            close_dropdown();

            auto sel_cb = on_select;
            void* sel_ud = select_user_data;
            auto txt_cb = on_text_change;
            void* txt_ud = text_user_data;
            uint32_t sid = selected_id;
            std::string item_text = text;

            if (sel_cb) {
                sel_cb(sid, item_text, sel_ud);
            }
            if (txt_cb) {
                txt_cb(item_text, txt_ud);
            }
        }
    }
}

void ComboBoxData::set_text(std::string_view new_text) {
    text = std::string(new_text);
    cursor_pos = text.size();
    refilter();
    if (on_text_change) {
        on_text_change(text, text_user_data);
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

void ComboBoxData::apply_theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    background_color = c.surface;
    hover_bg = c.surface_variant;
    border_color = c.outline_variant;
    focus_border_color = c.primary;
    text_color = c.on_surface;
    secondary_text_color = c.on_surface_variant;
    chevron_color = c.on_surface_variant;
    dropdown_bg = c.surface_variant;
    item_hover_bg = core::Color{c.on_surface.r, c.on_surface.g, c.on_surface.b, 0.08f};
    item_selected_bg = core::Color{c.primary.r, c.primary.g, c.primary.b, 0.16f};

    if (on_theme) {
        on_theme(theme, *this);
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

ComboBoxBuilder::ComboBoxBuilder(core::NodeTree& tree) : tree_(tree) {}

ComboBoxBuilder& ComboBoxBuilder::item(uint32_t id, std::string_view label,
                                       std::string_view secondary_label,
                                       render::Path icon,
                                       bool disabled,
                                       void* user_data) {
    SelectItem it;
    it.id = id;
    it.label = std::string(label);
    it.secondary_label = std::string(secondary_label);
    it.icon_path = std::move(icon);
    it.disabled = disabled;
    it.user_data = user_data;
    data_.items.push_back(std::move(it));
    return *this;
}

ComboBoxBuilder& ComboBoxBuilder::items(std::vector<SelectItem> items) {
    data_.items = std::move(items);
    return *this;
}

ComboBoxBuilder& ComboBoxBuilder::text(std::string_view text) {
    data_.text = std::string(text);
    data_.cursor_pos = data_.text.size();
    return *this;
}

ComboBoxBuilder& ComboBoxBuilder::placeholder(std::string_view placeholder) {
    data_.placeholder = std::string(placeholder);
    return *this;
}

ComboBoxBuilder& ComboBoxBuilder::width(float width) {
    data_.width = width;
    return *this;
}

ComboBoxBuilder& ComboBoxBuilder::height(float height) {
    data_.height = height;
    return *this;
}

ComboBoxBuilder& ComboBoxBuilder::dropdown_max_height(float height) {
    data_.dropdown_max_height = height;
    return *this;
}

ComboBoxBuilder& ComboBoxBuilder::font(std::shared_ptr<text::Font> font) {
    data_.font = std::move(font);
    return *this;
}

ComboBoxBuilder& ComboBoxBuilder::font_size(float size) {
    data_.font_size = size;
    return *this;
}

ComboBoxBuilder& ComboBoxBuilder::theme(const style::Theme& theme) {
    data_.apply_theme(theme);
    return *this;
}

ComboBoxBuilder& ComboBoxBuilder::theme_signal(core::Signal<style::Theme>* sig, ComboBoxThemeFn fn) {
    data_.theme_signal = sig;
    data_.on_theme = fn;
    if (sig) {
        data_.apply_theme(sig->get());
    }
    return *this;
}

ComboBoxBuilder& ComboBoxBuilder::on_select(ComboBoxSelectCallbackFn fn, void* user_data) {
    data_.on_select = fn;
    data_.select_user_data = user_data;
    return *this;
}

ComboBoxBuilder& ComboBoxBuilder::on_text_change(ComboBoxTextCallbackFn fn, void* user_data) {
    data_.on_text_change = fn;
    data_.text_user_data = user_data;
    return *this;
}

ComboBoxBuilder& ComboBoxBuilder::overlay_manager(overlay::OverlayManager& manager) {
    data_.overlay_manager = &manager;
    return *this;
}

core::Handle<core::Node> ComboBoxBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node) {
        return {};
    }

    if (!data_.font) {
        data_.font = text::FontManager::instance().load_system_font();
    }

    data_.refilter();

    auto* heap_data = new ComboBoxData(std::move(data_));
    heap_data->tree = &tree_;
    heap_data->node_handle = handle;

    if (heap_data->theme_signal) {
        heap_data->theme_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* cb = static_cast<ComboBoxData*>(ctx);
                if (cb && cb->theme_signal) {
                    cb->apply_theme(cb->theme_signal->get());
                }
            },
            heap_data, core::kPaint);
    }

    node->user_data = heap_data;
    node->layout_fn = &combo_box_layout_fn;
    node->paint_fn = &combo_box_paint_fn;
    node->destroy_fn = &combo_box_destroy_fn;

    return handle;
}

} // namespace north::widget
