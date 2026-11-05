#include "north/widget/command_palette.hpp"

#include "north/render/display_list.hpp"
#include "north/render/svg_path.hpp"
#include "north/text/font_manager.hpp"
#include "north/text/fuzzy_matcher.hpp"
#include "north/text/paragraph.hpp"

#include <algorithm>

namespace north::widget {

namespace {

constexpr float kInputHeight = 44.0f;
constexpr std::string_view kSearchIconSvg =
    "M216 208l-40.8-40.8a72 72 0 1 0-11.3 11.3L204.7 219.3a8 8 0 0 0 11.3-11.3ZM40 104a56 56 0 1 1 56 56a56 56 0 0 1-56-56Z";

core::Size command_palette_layout_fn(core::NodeTree&, core::Handle<core::Node>,
                                     const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<CommandPaletteData*>(user_data);
    if (!data) {
        return constraints.constrain({0.0f, 0.0f});
    }

    float width = (std::min)(data->width, constraints.max_width);
    float list_items_h = static_cast<float>(data->filtered_matches.size()) * data->row_height;
    float total_h = kInputHeight + list_items_h;
    if (total_h > data->max_height) {
        total_h = data->max_height;
    }

    float body_h = (std::max)(0.0f, total_h - kInputHeight);
    data->max_scroll_y = (std::max)(0.0f, list_items_h - body_h);
    if (data->scroll_y > data->max_scroll_y) {
        data->scroll_y = data->max_scroll_y;
    }

    return constraints.constrain({width, total_h});
}

void command_palette_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                              void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    auto* data = reinterpret_cast<CommandPaletteData*>(user_data);
    auto* node = tree.get(handle);
    if (!dl || !data || !node) {
        return;
    }

    core::Rect bounds{origin.x, origin.y, node->layout.size.width, node->layout.size.height};

    // Dialog container card with rounded corners, shadow, and border
    dl->push_shadow(bounds, core::CornerRadii::uniform(10.0f),
                    core::Color{0.0f, 0.0f, 0.0f, 0.35f}, 16.0f, 0.0f, {0.0f, 6.0f});
    dl->push_quad(bounds, core::CornerRadii::uniform(10.0f), data->background_color,
                  data->border_color, core::Edges::uniform(1.0f));

    // 1. Search Bar at top
    core::Rect input_rect{bounds.x + 8.0f, bounds.y + 6.0f, bounds.width - 16.0f, kInputHeight - 12.0f};
    dl->push_quad(input_rect, core::CornerRadii::uniform(6.0f), data->input_bg,
                  core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));

    // Search Icon
    render::Path search_icon;
    if (render::parse_svg_path(kSearchIconSvg, search_icon)) {
        core::Rect icon_target{input_rect.x + 10.0f, input_rect.y + (input_rect.height - 16.0f) * 0.5f, 16.0f, 16.0f};
        core::Rect b = search_icon.bounds();
        if (b.width > 0.0f && b.height > 0.0f) {
            float sx = icon_target.width / b.width;
            float sy = icon_target.height / b.height;
            render::Path scaled = search_icon;
            scaled.translate(core::Point{-b.x, -b.y});
            scaled.scale(sx, sy);
            scaled.translate(core::Point{icon_target.x, icon_target.y});
            dl->push_path_fill(scaled, data->secondary_text_color);
        }
    }

    // Search query or placeholder text
    float text_x = input_rect.x + 34.0f;
    float font_h = data->font_size * 1.3f;
    float text_y = input_rect.y + (input_rect.height - font_h) * 0.5f;

    if (data->font) {
        if (!data->query.empty()) {
            text::ParagraphBuilder pb;
            pb.add_text(data->query, text::TextStyle{.font = data->font,
                                                     .font_size = data->font_size,
                                                     .color = data->text_color});
            auto para = pb.build();
            para->layout(layout::BoxConstraints::loose({input_rect.width - 44.0f, font_h}));
            if (dl->glyph_atlas()) {
                para->paint(*dl, *dl->glyph_atlas(), {text_x, text_y}, data->text_color);
            }

            // Caret indicator
            float caret_x = text_x + para->size().width + 1.0f;
            core::Rect caret_rect{caret_x, text_y, 1.5f, font_h};
            dl->push_quad(caret_rect, core::CornerRadii::uniform(0.0f), data->highlight_color,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        } else {
            text::ParagraphBuilder pb;
            pb.add_text(data->placeholder, text::TextStyle{.font = data->font,
                                                           .font_size = data->font_size,
                                                           .color = data->secondary_text_color});
            auto para = pb.build();
            para->layout(layout::BoxConstraints::loose({input_rect.width - 44.0f, font_h}));
            if (dl->glyph_atlas()) {
                para->paint(*dl, *dl->glyph_atlas(), {text_x, text_y}, data->secondary_text_color);
            }

            // Caret indicator at start
            core::Rect caret_rect{text_x, text_y, 1.5f, font_h};
            dl->push_quad(caret_rect, core::CornerRadii::uniform(0.0f), data->highlight_color,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        }
    }

    // Divider line between search bar and results
    float div_y = bounds.y + kInputHeight;
    dl->push_quad(core::Rect{bounds.x, div_y, bounds.width, 1.0f},
                  core::CornerRadii::uniform(0.0f), data->border_color,
                  core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));

    // 2. Results list
    core::Rect list_rect{bounds.x, div_y + 1.0f, bounds.width, bounds.height - kInputHeight - 1.0f};
    if (list_rect.height <= 0.0f || data->filtered_matches.empty()) {
        return;
    }

    dl->push_clip(list_rect);

    size_t start_row = static_cast<size_t>((std::max)(0.0f, data->scroll_y / data->row_height));
    size_t visible_count = static_cast<size_t>(list_rect.height / data->row_height) + 2;
    size_t end_row = (std::min)(data->filtered_matches.size(), start_row + visible_count);

    for (size_t i = start_row; i < end_row; ++i) {
        const auto& match = data->filtered_matches[i];
        if (!match.item) {
            continue;
        }

        float row_y = list_rect.y + static_cast<float>(i) * data->row_height - data->scroll_y;
        core::Rect row_rect{bounds.x + 6.0f, row_y + 2.0f, bounds.width - 12.0f, data->row_height - 4.0f};

        bool is_selected = (i == data->selected_index);
        bool is_hovered = (i == data->hovered_index);

        if (is_selected) {
            dl->push_quad(row_rect, core::CornerRadii::uniform(6.0f), data->selected_row_bg,
                          data->highlight_color, core::Edges{0.0f, 0.0f, 0.0f, 3.0f});
        } else if (is_hovered) {
            dl->push_quad(row_rect, core::CornerRadii::uniform(6.0f), data->hover_row_bg,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        }

        float cursor_x = row_rect.x + 12.0f;

        // Item icon if available
        if (!match.item->icon_path.empty()) {
            core::Rect icon_box{cursor_x, row_rect.y + (row_rect.height - 16.0f) * 0.5f, 16.0f, 16.0f};
            core::Rect b = match.item->icon_path.bounds();
            if (b.width > 0.0f && b.height > 0.0f) {
                float sx = icon_box.width / b.width;
                float sy = icon_box.height / b.height;
                render::Path scaled = match.item->icon_path;
                scaled.translate(core::Point{-b.x, -b.y});
                scaled.scale(sx, sy);
                scaled.translate(core::Point{icon_box.x, icon_box.y});
                dl->push_path_stroke(scaled, is_selected ? data->highlight_color : data->secondary_text_color,
                                     render::StrokeStyle{1.5f});
            }
            cursor_x += 24.0f;
        }

        // Category tag if available
        if (!match.item->category.empty() && data->font) {
            text::ParagraphBuilder cpb;
            cpb.add_text(match.item->category, text::TextStyle{.font = data->font,
                                                               .font_size = data->font_size * 0.85f,
                                                               .color = data->category_header_color});
            auto cpara = cpb.build();
            cpara->layout(layout::BoxConstraints::loose({100.0f, font_h}));
            float cat_y = row_rect.y + (row_rect.height - cpara->size().height) * 0.5f;
            if (dl->glyph_atlas()) {
                cpara->paint(*dl, *dl->glyph_atlas(), {cursor_x, cat_y}, data->category_header_color);
            }
            cursor_x += cpara->size().width + 10.0f;

            // Small dot divider
            core::Rect dot_rect{cursor_x, row_rect.y + row_rect.height * 0.5f - 1.5f, 3.0f, 3.0f};
            dl->push_quad(dot_rect, core::CornerRadii::uniform(1.5f), data->category_header_color,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
            cursor_x += 10.0f;
        }

        // Item Title with highlights
        if (data->font && !match.item->title.empty()) {
            float item_text_y = row_rect.y + (row_rect.height - font_h) * 0.5f;
            float max_title_w = row_rect.width - (cursor_x - row_rect.x) - 90.0f;

            text::ParagraphBuilder pb;
            if (match.matched_indices.empty()) {
                pb.add_text(match.item->title, text::TextStyle{.font = data->font,
                                                               .font_size = data->font_size,
                                                               .color = is_selected ? data->text_color : data->secondary_text_color});
            } else {
                size_t last_idx = 0;
                for (uint32_t midx : match.matched_indices) {
                    if (midx > last_idx) {
                        std::string_view regular = std::string_view(match.item->title).substr(last_idx, midx - last_idx);
                        pb.add_text(regular, text::TextStyle{.font = data->font,
                                                             .font_size = data->font_size,
                                                             .color = is_selected ? data->text_color : data->secondary_text_color});
                    }
                    std::string_view hl = std::string_view(match.item->title).substr(midx, 1);
                    pb.add_text(hl, text::TextStyle{.font = data->font,
                                                    .font_size = data->font_size,
                                                    .color = data->highlight_color});
                    last_idx = midx + 1;
                }
                if (last_idx < match.item->title.size()) {
                    std::string_view tail = std::string_view(match.item->title).substr(last_idx);
                    pb.add_text(tail, text::TextStyle{.font = data->font,
                                                      .font_size = data->font_size,
                                                      .color = is_selected ? data->text_color : data->secondary_text_color});
                }
            }

            auto para = pb.build();
            para->layout(layout::BoxConstraints::loose({(std::max)(0.0f, max_title_w), font_h}));
            if (dl->glyph_atlas()) {
                para->paint(*dl, *dl->glyph_atlas(), {cursor_x, item_text_y}, data->text_color);
            }
        }

        // Shortcut hint badge on right side
        if (!match.item->shortcut_hint.empty() && data->font) {
            text::ParagraphBuilder spb;
            spb.add_text(match.item->shortcut_hint, text::TextStyle{.font = data->font,
                                                                    .font_size = data->font_size * 0.85f,
                                                                    .color = data->shortcut_badge_text});
            auto spara = spb.build();
            spara->layout(layout::BoxConstraints::loose({120.0f, font_h}));

            float badge_pad_h = 6.0f;
            float badge_w = spara->size().width + badge_pad_h * 2.0f;
            float badge_h = spara->size().height + 4.0f;
            float badge_x = row_rect.x + row_rect.width - badge_w - 8.0f;
            float badge_y = row_rect.y + (row_rect.height - badge_h) * 0.5f;

            core::Rect badge_rect{badge_x, badge_y, badge_w, badge_h};
            dl->push_quad(badge_rect, core::CornerRadii::uniform(4.0f), data->shortcut_badge_bg,
                          data->border_color, core::Edges::uniform(1.0f));

            if (dl->glyph_atlas()) {
                spara->paint(*dl, *dl->glyph_atlas(), {badge_x + badge_pad_h, badge_y + 2.0f},
                             data->shortcut_badge_text);
            }
        }
    }

    // Scrollbar if needed
    if (data->max_scroll_y > 0.0f && list_rect.height > 20.0f) {
        float track_h = list_rect.height - 8.0f;
        float thumb_h = (std::max)(16.0f, track_h * (list_rect.height / (list_rect.height + data->max_scroll_y)));
        float thumb_y = list_rect.y + 4.0f + (data->scroll_y / data->max_scroll_y) * (track_h - thumb_h);
        core::Rect thumb_rect{bounds.x + bounds.width - 6.0f, thumb_y, 3.0f, thumb_h};
        dl->push_quad(thumb_rect, core::CornerRadii::uniform(1.5f), data->secondary_text_color,
                      core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
    }

    dl->pop_clip();
}

void command_palette_destroy_fn(void* user_data) {
    delete reinterpret_cast<CommandPaletteData*>(user_data);
}

} // namespace

void CommandPaletteData::refilter() {
    filtered_matches.clear();
    std::string_view match_query = query;
    if (active_mode_prefix != '\0' && !match_query.empty() && match_query.front() == active_mode_prefix) {
        match_query = match_query.substr(1);
    }

    for (size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        if (match_query.empty()) {
            filtered_matches.push_back(PaletteItemMatch{
                .original_index = i,
                .item = &item,
                .score = 0,
                .matched_indices = {}
            });
        } else {
            auto res = text::fuzzy_match(match_query, item.title);
            if (res.matched) {
                filtered_matches.push_back(PaletteItemMatch{
                    .original_index = i,
                    .item = &item,
                    .score = res.score,
                    .matched_indices = std::move(res.matched_indices)
                });
            }
        }
    }

    if (!match_query.empty()) {
        std::sort(filtered_matches.begin(), filtered_matches.end(),
                  [](const PaletteItemMatch& a, const PaletteItemMatch& b) {
                      return a.score > b.score;
                  });
    }

    if (selected_index >= filtered_matches.size()) {
        selected_index = 0;
    }
    scroll_y = 0.0f;

    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kLayout | core::kPaint);
    }
}

void CommandPaletteData::select_next() {
    if (filtered_matches.empty()) {
        return;
    }
    selected_index = (selected_index + 1 < filtered_matches.size()) ? selected_index + 1 : 0;
    scroll_into_view(selected_index, max_height - kInputHeight);
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

void CommandPaletteData::select_prev() {
    if (filtered_matches.empty()) {
        return;
    }
    selected_index = (selected_index > 0) ? selected_index - 1 : filtered_matches.size() - 1;
    scroll_into_view(selected_index, max_height - kInputHeight);
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

void CommandPaletteData::select_index(size_t index) {
    if (index < filtered_matches.size()) {
        selected_index = index;
        scroll_into_view(selected_index, max_height - kInputHeight);
        if (tree && node_handle.is_valid()) {
            tree->mark_dirty(node_handle, core::kPaint);
        }
    }
}

void CommandPaletteData::execute_selected() {
    auto* mgr = overlay_manager;
    auto oid = active_overlay_id;
    active_overlay_id = overlay::kInvalidOverlayId;
    auto dismiss_fn = on_dismiss;
    void* dismiss_ud = dismiss_user_data;

    PaletteActionFn action_fn = on_action;
    void* action_ud = action_user_data;
    uint32_t act_id = 0;
    void* act_user_data = nullptr;
    bool has_item_action = false;
    PaletteActionFn item_action_fn = nullptr;

    if (selected_index < filtered_matches.size()) {
        const auto* item = filtered_matches[selected_index].item;
        if (item) {
            act_id = item->id;
            act_user_data = item->user_data;
            if (item->on_action) {
                has_item_action = true;
                item_action_fn = item->on_action;
            }
        }
    }

    if (mgr && oid != overlay::kInvalidOverlayId) {
        mgr->hide_overlay(oid);
    }

    if (dismiss_fn) {
        dismiss_fn(dismiss_ud);
    }

    if (has_item_action && item_action_fn) {
        item_action_fn(act_id, act_user_data);
    } else if (action_fn && act_id != 0) {
        action_fn(act_id, action_ud);
    }
}

void CommandPaletteData::set_query(std::string_view new_query) {
    query = std::string(new_query);
    refilter();
}

void CommandPaletteData::scroll_into_view(size_t idx, float viewport_h) {
    float top_y = static_cast<float>(idx) * row_height;
    float bot_y = top_y + row_height;
    if (top_y < scroll_y) {
        scroll_y = top_y;
    } else if (bot_y > scroll_y + viewport_h) {
        scroll_y = bot_y - viewport_h;
    }
}

void CommandPaletteData::apply_theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    background_color = c.surface_variant;
    input_bg = c.surface;
    border_color = c.outline_variant;
    text_color = c.on_surface;
    secondary_text_color = c.on_surface_variant;
    highlight_color = c.primary;
    selected_row_bg = core::Color{c.primary.r, c.primary.g, c.primary.b, 0.16f};
    hover_row_bg = core::Color{c.on_surface.r, c.on_surface.g, c.on_surface.b, 0.06f};
    category_header_color = c.on_surface_variant;
    shortcut_badge_bg = core::Color{c.on_surface.r, c.on_surface.g, c.on_surface.b, 0.08f};
    shortcut_badge_text = c.on_surface_variant;

    if (on_theme) {
        on_theme(theme, *this);
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

CommandPaletteBuilder::CommandPaletteBuilder(core::NodeTree& tree) : tree_(tree) {}

CommandPaletteBuilder& CommandPaletteBuilder::item(uint32_t id, std::string_view title,
                                                  std::string_view category,
                                                  std::string_view shortcut_hint,
                                                  render::Path icon,
                                                  PaletteActionFn on_action,
                                                  void* user_data) {
    PaletteItem it;
    it.id = id;
    it.title = std::string(title);
    it.category = std::string(category);
    it.shortcut_hint = std::string(shortcut_hint);
    it.icon_path = std::move(icon);
    it.on_action = on_action;
    it.user_data = user_data;
    data_.items.push_back(std::move(it));
    return *this;
}

CommandPaletteBuilder& CommandPaletteBuilder::items(std::vector<PaletteItem> items) {
    data_.items = std::move(items);
    return *this;
}

CommandPaletteBuilder& CommandPaletteBuilder::placeholder(std::string_view placeholder) {
    data_.placeholder = std::string(placeholder);
    return *this;
}

CommandPaletteBuilder& CommandPaletteBuilder::mode_prefix(char prefix) {
    data_.active_mode_prefix = prefix;
    return *this;
}

CommandPaletteBuilder& CommandPaletteBuilder::width(float width) {
    data_.width = width;
    return *this;
}

CommandPaletteBuilder& CommandPaletteBuilder::max_height(float height) {
    data_.max_height = height;
    return *this;
}

CommandPaletteBuilder& CommandPaletteBuilder::row_height(float height) {
    data_.row_height = height;
    return *this;
}

CommandPaletteBuilder& CommandPaletteBuilder::font(std::shared_ptr<text::Font> font) {
    data_.font = std::move(font);
    return *this;
}

CommandPaletteBuilder& CommandPaletteBuilder::font_size(float size) {
    data_.font_size = size;
    return *this;
}

CommandPaletteBuilder& CommandPaletteBuilder::theme(const style::Theme& theme) {
    data_.apply_theme(theme);
    return *this;
}

CommandPaletteBuilder& CommandPaletteBuilder::theme_signal(core::Signal<style::Theme>* sig,
                                                          CommandPaletteThemeFn fn) {
    data_.theme_signal = sig;
    data_.on_theme = fn;
    if (sig) {
        data_.apply_theme(sig->get());
    }
    return *this;
}

CommandPaletteBuilder& CommandPaletteBuilder::on_action(PaletteActionFn fn, void* user_data) {
    data_.on_action = fn;
    data_.action_user_data = user_data;
    return *this;
}

CommandPaletteBuilder& CommandPaletteBuilder::on_dismiss(PaletteDismissFn fn, void* user_data) {
    data_.on_dismiss = fn;
    data_.dismiss_user_data = user_data;
    return *this;
}

CommandPaletteBuilder& CommandPaletteBuilder::overlay_manager(overlay::OverlayManager& manager) {
    data_.overlay_manager = &manager;
    return *this;
}

core::Handle<core::Node> CommandPaletteBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node) {
        return {};
    }

    if (!data_.font) {
        data_.font = text::FontManager::instance().load_system_font();
    }

    data_.refilter();

    auto* heap_data = new CommandPaletteData(std::move(data_));
    heap_data->tree = &tree_;
    heap_data->node_handle = handle;

    if (heap_data->theme_signal) {
        heap_data->theme_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* cp = static_cast<CommandPaletteData*>(ctx);
                if (cp && cp->theme_signal) {
                    cp->apply_theme(cp->theme_signal->get());
                }
            },
            heap_data, core::kPaint);
    }

    node->user_data = heap_data;
    node->layout_fn = &command_palette_layout_fn;
    node->paint_fn = &command_palette_paint_fn;
    node->destroy_fn = &command_palette_destroy_fn;

    return handle;
}

} // namespace north::widget
