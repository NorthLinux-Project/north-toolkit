#include "north/widget/tree_view.hpp"

#include "north/render/display_list.hpp"
#include "north/render/svg_path.hpp"
#include "north/text/font_manager.hpp"
#include "north/text/shaper.hpp"

#include <algorithm>
#include <cmath>

namespace north::widget {

namespace {

void collect_visible_rows(TreeNodeItem& item, uint32_t parent_id, uint16_t depth,
                          std::vector<VisibleTreeRow>& rows) {
    rows.push_back(VisibleTreeRow{&item, parent_id, depth});
    if (item.expandable && item.expanded) {
        for (auto& child : item.children) {
            collect_visible_rows(child, item.id, depth + 1, rows);
        }
    }
}

TreeNodeItem* find_node_recursive(TreeNodeItem& item, uint32_t id) {
    if (item.id == id) {
        return &item;
    }
    for (auto& child : item.children) {
        auto* found = find_node_recursive(child, id);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

core::Size tree_view_layout_fn(core::NodeTree&, core::Handle<core::Node>,
                               const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<TreeViewData*>(user_data);
    if (!data) {
        return constraints.constrain({0.0f, 0.0f});
    }

    float w = (data->width > 0.0f) ? data->width : (constraints.max_width < 100000.0f ? constraints.max_width : 260.0f);
    float h = (data->height > 0.0f) ? data->height : (constraints.max_height < 100000.0f ? constraints.max_height : 400.0f);
    core::Size resolved = constraints.constrain({w, h});

    float total_content_h = static_cast<float>(data->visible_rows.size()) * data->row_height;
    data->max_scroll_y = (std::max)(0.0f, total_content_h - resolved.height);
    data->scroll_y = std::clamp(data->scroll_y, 0.0f, data->max_scroll_y);

    return resolved;
}

void tree_view_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                        void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    auto* data = reinterpret_cast<TreeViewData*>(user_data);
    auto* node = tree.get(handle);
    if (!dl || !data || !node) {
        return;
    }

    core::Rect bounds{origin.x, origin.y, node->layout.size.width, node->layout.size.height};
    dl->push_quad(bounds, core::CornerRadii::uniform(0.0f), data->background_color,
                  data->border_color, core::Edges::uniform(1.0f));

    if (data->visible_rows.empty() || bounds.height <= 0.0f) {
        return;
    }

    dl->push_clip(bounds);

    size_t start_row = static_cast<size_t>(std::max(0.0f, std::floor(data->scroll_y / data->row_height)));
    size_t visible_count = static_cast<size_t>(std::ceil(bounds.height / data->row_height)) + 1;
    size_t end_row = (std::min)(data->visible_rows.size(), start_row + visible_count);

    float font_h = data->font ? data->font->line_height(data->font_size) : (data->font_size * 1.4f);

    for (size_t i = start_row; i < end_row; ++i) {
        const auto& row = data->visible_rows[i];
        if (!row.item) {
            continue;
        }

        float row_y = bounds.y + static_cast<float>(i) * data->row_height - data->scroll_y;
        core::Rect row_rect{bounds.x, row_y, bounds.width, data->row_height};

        bool is_selected = (row.item->id == data->selected_id);
        bool is_hovered = (i == data->hovered_row);

        if (is_selected) {
            dl->push_quad(row_rect, core::CornerRadii::uniform(4.0f), data->selected_row_bg,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        } else if (is_hovered) {
            dl->push_quad(row_rect, core::CornerRadii::uniform(4.0f), data->hover_row_bg,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        }

        float cursor_x = bounds.x + 8.0f;

        if (data->show_indent_guides && row.depth > 0) {
            for (uint16_t d = 0; d < row.depth; ++d) {
                float guide_x = cursor_x + static_cast<float>(d) * data->indent_width + 8.0f;
                core::Rect guide_rect{guide_x, row_y, 1.0f, data->row_height};
                dl->push_quad(guide_rect, core::CornerRadii::uniform(0.0f), data->indent_guide_color,
                              core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
            }
        }

        cursor_x += static_cast<float>(row.depth) * data->indent_width;

        if (row.item->expandable) {
            float chev_sz = 10.0f;
            float chev_x = cursor_x + 2.0f;
            float chev_y = row_y + (data->row_height - chev_sz) * 0.5f;

            render::Path chev;
            if (row.item->expanded) {
                chev.move_to({chev_x + 1.0f, chev_y + 3.0f});
                chev.line_to({chev_x + chev_sz * 0.5f, chev_y + chev_sz - 2.0f});
                chev.line_to({chev_x + chev_sz - 1.0f, chev_y + 3.0f});
            } else {
                chev.move_to({chev_x + 3.0f, chev_y + 1.0f});
                chev.line_to({chev_x + chev_sz - 2.0f, chev_y + chev_sz * 0.5f});
                chev.line_to({chev_x + 3.0f, chev_y + chev_sz - 1.0f});
            }
            dl->push_path_stroke(chev, data->chevron_color, render::StrokeStyle{1.5f});
        }

        cursor_x += 16.0f;

        if (!row.item->icon_path.empty()) {
            core::Rect b = row.item->icon_path.bounds();
            float icon_sz = 14.0f;
            float sx = (b.width > 1e-4f) ? (icon_sz / b.width) : 1.0f;
            float sy = (b.height > 1e-4f) ? (icon_sz / b.height) : 1.0f;
            render::Path p = row.item->icon_path;
            p.translate({-b.x, -b.y});
            p.scale(sx, sy);
            p.translate({cursor_x, row_y + (data->row_height - icon_sz) * 0.5f});
            dl->push_path_fill(p, data->icon_color);
            cursor_x += icon_sz + 6.0f;
        }

        if (data->font && !row.item->label.empty()) {
            float text_y = row_y + (data->row_height - font_h) * 0.5f;
            text::ParagraphBuilder pb(text::TextAlign::Left);
            pb.add_text(row.item->label, text::TextStyle{.font = data->font,
                                                         .font_size = data->font_size,
                                                         .color = data->text_color});
            auto para = pb.build();
            para->layout(layout::BoxConstraints::loose({(std::max)(10.0f, bounds.x + bounds.width - cursor_x - 12.0f), font_h}));
            if (dl->glyph_atlas()) {
                para->paint(*dl, *dl->glyph_atlas(), {cursor_x, text_y}, data->text_color);
            }
        }
    }

    if (data->max_scroll_y > 0.0f && bounds.height > 20.0f) {
        float total_h = static_cast<float>(data->visible_rows.size()) * data->row_height;
        float thumb_h = (std::max)(16.0f, (bounds.height / total_h) * bounds.height);
        float track_h = bounds.height - thumb_h;
        float thumb_y = bounds.y + (data->scroll_y / data->max_scroll_y) * track_h;
        core::Rect thumb_rect{bounds.x + bounds.width - 6.0f, thumb_y, 4.0f, thumb_h};
        dl->push_quad(thumb_rect, core::CornerRadii::uniform(2.0f), data->scrollbar_thumb_color,
                      core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
    }

    dl->pop_clip();
}

void tree_view_destroy_fn(void* user_data) {
    delete reinterpret_cast<TreeViewData*>(user_data);
}

} // namespace

void TreeViewData::rebuild_visible_rows() {
    visible_rows.clear();
    for (auto& root : roots) {
        collect_visible_rows(root, 0, 0, visible_rows);
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kLayout | core::kPaint);
    }
}

TreeNodeItem* TreeViewData::find_node(uint32_t id) {
    for (auto& root : roots) {
        auto* found = find_node_recursive(root, id);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

void TreeViewData::select_node(uint32_t id) {
    selected_id = id;
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
    if (on_select) {
        on_select(selected_id, select_user_data);
    }
}

void TreeViewData::toggle_node(uint32_t id) {
    auto* item = find_node(id);
    if (!item || !item->expandable) {
        return;
    }
    item->expanded = !item->expanded;
    rebuild_visible_rows();
    if (on_toggle) {
        on_toggle(id, item->expanded, toggle_user_data);
    }
}

void TreeViewData::scroll_into_view(size_t row_idx, float viewport_h) {
    if (row_idx >= visible_rows.size() || viewport_h <= 0.0f) {
        return;
    }
    float item_top = static_cast<float>(row_idx) * row_height;
    float item_bottom = item_top + row_height;

    if (item_top < scroll_y) {
        scroll_y = item_top;
    } else if (item_bottom > scroll_y + viewport_h) {
        scroll_y = item_bottom - viewport_h;
    }
    scroll_y = std::clamp(scroll_y, 0.0f, max_scroll_y);
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

void TreeViewData::apply_theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    background_color = c.surface;
    text_color = c.on_surface;
    hover_row_bg = core::Color{c.on_surface.r, c.on_surface.g, c.on_surface.b, 0.08f};
    selected_row_bg = c.surface_variant;
    indent_guide_color = core::Color{c.outline_variant.r, c.outline_variant.g, c.outline_variant.b, 0.40f};
    chevron_color = c.on_surface_variant;
    icon_color = c.primary;
    border_color = c.outline_variant;
    scrollbar_thumb_color = core::Color{c.outline.r, c.outline.g, c.outline.b, 0.60f};

    if (on_theme) {
        on_theme(theme, *this);
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

TreeViewBuilder::TreeViewBuilder(core::NodeTree& tree) : tree_(tree) {}

TreeViewBuilder& TreeViewBuilder::roots(std::vector<TreeNodeItem> roots) {
    data_.roots = std::move(roots);
    return *this;
}

TreeViewBuilder& TreeViewBuilder::root_item(TreeNodeItem item) {
    data_.roots.push_back(std::move(item));
    return *this;
}

TreeViewBuilder& TreeViewBuilder::selected_id(uint32_t id) {
    data_.selected_id = id;
    return *this;
}

TreeViewBuilder& TreeViewBuilder::font(std::shared_ptr<text::Font> font) {
    data_.font = std::move(font);
    return *this;
}

TreeViewBuilder& TreeViewBuilder::font_size(float size) {
    data_.font_size = size;
    return *this;
}

TreeViewBuilder& TreeViewBuilder::row_height(float height) {
    data_.row_height = height;
    return *this;
}

TreeViewBuilder& TreeViewBuilder::indent_width(float width) {
    data_.indent_width = width;
    return *this;
}

TreeViewBuilder& TreeViewBuilder::indent_guides(bool show) {
    data_.show_indent_guides = show;
    return *this;
}

TreeViewBuilder& TreeViewBuilder::width(float width) {
    data_.width = width;
    return *this;
}

TreeViewBuilder& TreeViewBuilder::height(float height) {
    data_.height = height;
    return *this;
}

TreeViewBuilder& TreeViewBuilder::theme(const style::Theme& theme) {
    data_.apply_theme(theme);
    return *this;
}

TreeViewBuilder& TreeViewBuilder::theme_signal(core::Signal<style::Theme>* sig, TreeViewThemeFn fn) {
    data_.theme_signal = sig;
    data_.on_theme = fn;
    if (sig) {
        data_.apply_theme(sig->get());
    }
    return *this;
}

TreeViewBuilder& TreeViewBuilder::on_select(TreeSelectCallbackFn fn, void* user_data) {
    data_.on_select = fn;
    data_.select_user_data = user_data;
    return *this;
}

TreeViewBuilder& TreeViewBuilder::on_toggle(TreeToggleCallbackFn fn, void* user_data) {
    data_.on_toggle = fn;
    data_.toggle_user_data = user_data;
    return *this;
}

TreeViewBuilder& TreeViewBuilder::on_activate(TreeActivateCallbackFn fn, void* user_data) {
    data_.on_activate = fn;
    data_.activate_user_data = user_data;
    return *this;
}

core::Handle<core::Node> TreeViewBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node) {
        return {};
    }

    if (!data_.font) {
        data_.font = text::FontManager::instance().load_system_font();
    }

    auto* heap_data = new TreeViewData(std::move(data_));
    heap_data->tree = &tree_;
    heap_data->node_handle = handle;
    heap_data->rebuild_visible_rows();

    if (heap_data->theme_signal) {
        heap_data->theme_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* tv = static_cast<TreeViewData*>(ctx);
                if (tv && tv->theme_signal) {
                    tv->apply_theme(tv->theme_signal->get());
                }
            },
            heap_data, core::kPaint);
    }

    node->user_data = heap_data;
    node->layout_fn = &tree_view_layout_fn;
    node->paint_fn = &tree_view_paint_fn;
    node->destroy_fn = &tree_view_destroy_fn;

    return handle;
}

} // namespace north::widget
