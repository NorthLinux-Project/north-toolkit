#include "north/widget/data_table.hpp"

#include "north/render/display_list.hpp"
#include "north/text/font_manager.hpp"
#include "north/text/shaper.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <numeric>

namespace north::widget {

namespace {

core::Size data_table_layout_fn(core::NodeTree&, core::Handle<core::Node>,
                                const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<DataTableData*>(user_data);
    if (!data) {
        return constraints.constrain({0.0f, 0.0f});
    }

    float w = (data->width > 0.0f) ? data->width : (constraints.max_width < 100000.0f ? constraints.max_width : 500.0f);
    float h = (data->height > 0.0f) ? data->height : (constraints.max_height < 100000.0f ? constraints.max_height : 350.0f);
    core::Size resolved = constraints.constrain({w, h});

    float fixed_w = 0.0f;
    float total_flex = 0.0f;
    for (const auto& col : data->columns) {
        if (col.flex > 0.0f) {
            total_flex += col.flex;
        } else {
            fixed_w += col.width;
        }
    }

    float remaining_w = (std::max)(0.0f, resolved.width - fixed_w);
    float cur_x = 0.0f;
    for (auto& col : data->columns) {
        col.computed_x = cur_x;
        if (col.flex > 0.0f && total_flex > 0.0f) {
            col.computed_width = (std::max)(col.min_width, (col.flex / total_flex) * remaining_w);
        } else {
            col.computed_width = (std::max)(col.min_width, col.width);
        }
        cur_x += col.computed_width;
    }

    float content_area_h = (std::max)(0.0f, resolved.height - data->header_height);
    float total_rows_h = static_cast<float>(data->rows.size()) * data->row_height;
    data->max_scroll_y = (std::max)(0.0f, total_rows_h - content_area_h);
    data->scroll_y = std::clamp(data->scroll_y, 0.0f, data->max_scroll_y);

    return resolved;
}

void data_table_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                         void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    auto* data = reinterpret_cast<DataTableData*>(user_data);
    auto* node = tree.get(handle);
    if (!dl || !data || !node) {
        return;
    }

    core::Rect bounds{origin.x, origin.y, node->layout.size.width, node->layout.size.height};
    dl->push_quad(bounds, core::CornerRadii::uniform(0.0f), data->background_color,
                  data->border_color, core::Edges::uniform(1.0f));

    if (data->columns.empty() || bounds.height <= 0.0f) {
        return;
    }

    float font_h = data->font ? data->font->line_height(data->font_size) : (data->font_size * 1.4f);

    float body_y = bounds.y + data->header_height;
    float body_h = (std::max)(0.0f, bounds.height - data->header_height);
    core::Rect body_clip{bounds.x, body_y, bounds.width, body_h};

    if (body_h > 0.0f && !data->rows.empty()) {
        dl->push_clip(body_clip);

        size_t start_row = static_cast<size_t>(std::max(0.0f, std::floor(data->scroll_y / data->row_height)));
        size_t visible_count = static_cast<size_t>(std::ceil(body_h / data->row_height)) + 1;
        size_t end_row = (std::min)(data->rows.size(), start_row + visible_count);

        for (size_t r = start_row; r < end_row; ++r) {
            size_t row_idx = (r < data->sorted_indices.size()) ? data->sorted_indices[r] : r;
            const auto& row = data->rows[row_idx];

            float r_y = body_y + static_cast<float>(r) * data->row_height - data->scroll_y;
            core::Rect row_rect{bounds.x, r_y, bounds.width, data->row_height};

            bool is_selected = (r == data->selected_row);
            bool is_hovered = (r == data->hovered_row);

            if (is_selected) {
                dl->push_quad(row_rect, core::CornerRadii::uniform(0.0f), data->selected_row_bg,
                              core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
            } else if (is_hovered) {
                dl->push_quad(row_rect, core::CornerRadii::uniform(0.0f), data->hover_row_bg,
                              core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
            } else if (data->alternating_rows && (r % 2 == 1)) {
                dl->push_quad(row_rect, core::CornerRadii::uniform(0.0f), data->row_alt_bg,
                              core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
            }

            for (size_t c = 0; c < data->columns.size() && c < row.cells.size(); ++c) {
                const auto& col = data->columns[c];
                core::Rect cell_rect{bounds.x + col.computed_x, r_y, col.computed_width, data->row_height};

                if (data->font && !row.cells[c].empty()) {
                    float text_y = r_y + (data->row_height - font_h) * 0.5f;
                    text::ParagraphBuilder pb(col.align);
                    pb.add_text(row.cells[c], text::TextStyle{.font = data->font,
                                                              .font_size = data->font_size,
                                                              .color = data->text_color});
                    auto para = pb.build();
                    para->layout(layout::BoxConstraints::loose({(std::max)(0.0f, col.computed_width - 16.0f), font_h}));

                    float text_x = cell_rect.x + 8.0f;
                    if (col.align == text::TextAlign::Right) {
                        text_x = cell_rect.x + cell_rect.width - 8.0f - para->size().width;
                    } else if (col.align == text::TextAlign::Center) {
                        text_x = cell_rect.x + (cell_rect.width - para->size().width) * 0.5f;
                    }

                    if (dl->glyph_atlas()) {
                        para->paint(*dl, *dl->glyph_atlas(), {text_x, text_y}, data->text_color);
                    }
                }
            }
        }

        if (data->max_scroll_y > 0.0f && body_h > 20.0f) {
            float total_h = static_cast<float>(data->rows.size()) * data->row_height;
            float thumb_h = (std::max)(16.0f, (body_h / total_h) * body_h);
            float track_h = body_h - thumb_h;
            float thumb_y = body_y + (data->scroll_y / data->max_scroll_y) * track_h;
            core::Rect thumb_rect{bounds.x + bounds.width - 6.0f, thumb_y, 4.0f, thumb_h};
            dl->push_quad(thumb_rect, core::CornerRadii::uniform(2.0f), data->scrollbar_thumb_color,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        }

        dl->pop_clip();
    }

    core::Rect header_rect{bounds.x, bounds.y, bounds.width, data->header_height};
    dl->push_quad(header_rect, core::CornerRadii::uniform(0.0f), data->header_bg,
                  data->border_color, core::Edges{0.0f, 0.0f, 1.0f, 0.0f});

    for (size_t c = 0; c < data->columns.size(); ++c) {
        const auto& col = data->columns[c];
        core::Rect col_hdr{bounds.x + col.computed_x, bounds.y, col.computed_width, data->header_height};

        if (c == data->hovered_header && col.sortable) {
            dl->push_quad(col_hdr, core::CornerRadii::uniform(0.0f), data->header_hover_bg,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        }

        if (data->font) {
            float text_y = bounds.y + (data->header_height - font_h) * 0.5f;
            float avail_hdr_w = (std::max)(0.0f, col.computed_width - (col.sortable ? 26.0f : 16.0f));

            if (!col.title.empty()) {
                text::ParagraphBuilder pb(text::TextAlign::Center);
                pb.add_text(col.title, text::TextStyle{.font = data->font,
                                                       .font_size = data->font_size,
                                                       .color = data->header_text_color});
                auto para = pb.build();
                para->layout(layout::BoxConstraints::loose({avail_hdr_w, font_h}));

                float text_x = col_hdr.x + (col_hdr.width - para->size().width) * 0.5f;
                if (col.sortable && text_x + para->size().width > col_hdr.x + col_hdr.width - 18.0f) {
                    text_x = col_hdr.x + col_hdr.width - 18.0f - para->size().width;
                }
                if (text_x < col_hdr.x + 8.0f) {
                    text_x = col_hdr.x + 8.0f;
                }

                if (dl->glyph_atlas()) {
                    para->paint(*dl, *dl->glyph_atlas(), {text_x, text_y}, data->header_text_color);
                }
            }

            if (data->sort_column == c && data->sort_direction != SortDirection::None && col.computed_width >= 24.0f) {
                float icon_x = col_hdr.x + col_hdr.width - 16.0f;
                float icon_y = bounds.y + (data->header_height - 8.0f) * 0.5f;
                render::Path arrow;
                if (data->sort_direction == SortDirection::Ascending) {
                    arrow.move_to({icon_x, icon_y + 6.0f});
                    arrow.line_to({icon_x + 4.0f, icon_y + 1.0f});
                    arrow.line_to({icon_x + 8.0f, icon_y + 6.0f});
                } else {
                    arrow.move_to({icon_x, icon_y + 2.0f});
                    arrow.line_to({icon_x + 4.0f, icon_y + 7.0f});
                    arrow.line_to({icon_x + 8.0f, icon_y + 2.0f});
                }
                dl->push_path_stroke(arrow, data->sort_icon_color, render::StrokeStyle{1.5f});
            }
        }

        if (col.resizable) {
            float div_x = col_hdr.x + col_hdr.width - 1.0f;
            bool is_active_resizer = (c == data->hovered_resizer || (data->is_resizing_column && c == data->resizing_column));
            core::Color div_color = is_active_resizer ? data->resizer_hover_color : data->border_color;
            float div_thick = is_active_resizer ? 2.0f : 1.0f;
            core::Rect div_rect{div_x - (is_active_resizer ? 1.0f : 0.0f), bounds.y + 4.0f, div_thick, data->header_height - 8.0f};
            dl->push_quad(div_rect, core::CornerRadii::uniform(0.0f), div_color,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        }
    }
}

void data_table_destroy_fn(void* user_data) {
    delete reinterpret_cast<DataTableData*>(user_data);
}

} // namespace

void DataTableData::sort_by_column(size_t col_idx, SortDirection dir) {
    if (col_idx >= columns.size() || !columns[col_idx].sortable) {
        return;
    }

    sort_column = col_idx;
    sort_direction = dir;

    if (sorted_indices.size() != rows.size()) {
        sorted_indices.resize(rows.size());
        std::iota(sorted_indices.begin(), sorted_indices.end(), 0);
    }

    if (sort_direction == SortDirection::None) {
        std::iota(sorted_indices.begin(), sorted_indices.end(), 0);
    } else {
        bool asc = (sort_direction == SortDirection::Ascending);
        std::sort(sorted_indices.begin(), sorted_indices.end(), [&](size_t a, size_t b) {
            const auto& str_a = (col_idx < rows[a].cells.size()) ? rows[a].cells[col_idx] : "";
            const auto& str_b = (col_idx < rows[b].cells.size()) ? rows[b].cells[col_idx] : "";

            double num_a = 0.0;
            double num_b = 0.0;
            auto [p1, ec1] = std::from_chars(str_a.data(), str_a.data() + str_a.size(), num_a);
            auto [p2, ec2] = std::from_chars(str_b.data(), str_b.data() + str_b.size(), num_b);

            if (ec1 == std::errc() && ec2 == std::errc() && p1 == str_a.data() + str_a.size() && p2 == str_b.data() + str_b.size()) {
                return asc ? (num_a < num_b) : (num_a > num_b);
            }
            return asc ? (str_a < str_b) : (str_a > str_b);
        });
    }

    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
    if (on_sort) {
        on_sort(sort_column, sort_direction, sort_user_data);
    }
}

void DataTableData::cycle_sort(size_t col_idx) {
    if (col_idx >= columns.size() || !columns[col_idx].sortable) {
        return;
    }
    if (sort_column != col_idx) {
        sort_by_column(col_idx, SortDirection::Ascending);
    } else if (sort_direction == SortDirection::Ascending) {
        sort_by_column(col_idx, SortDirection::Descending);
    } else if (sort_direction == SortDirection::Descending) {
        sort_by_column(col_idx, SortDirection::None);
    } else {
        sort_by_column(col_idx, SortDirection::Ascending);
    }
}

void DataTableData::select_row(size_t row_idx) {
    selected_row = row_idx;
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
    if (on_select && selected_row < sorted_indices.size()) {
        size_t actual_idx = sorted_indices[selected_row];
        on_select(selected_row, rows[actual_idx].id, select_user_data);
    }
}

void DataTableData::scroll_into_view(size_t row_idx, float viewport_h) {
    if (row_idx >= rows.size() || viewport_h <= 0.0f) {
        return;
    }
    float body_h = (std::max)(0.0f, viewport_h - header_height);
    float item_top = static_cast<float>(row_idx) * row_height;
    float item_bottom = item_top + row_height;

    if (item_top < scroll_y) {
        scroll_y = item_top;
    } else if (item_bottom > scroll_y + body_h) {
        scroll_y = item_bottom - body_h;
    }
    scroll_y = std::clamp(scroll_y, 0.0f, max_scroll_y);
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

void DataTableData::apply_theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    background_color = c.surface;
    header_bg = c.surface_variant;
    header_hover_bg = core::Color{c.on_surface.r, c.on_surface.g, c.on_surface.b, 0.08f};
    row_alt_bg = core::Color{c.surface.r, c.surface.g, c.surface.b, 0.70f};
    hover_row_bg = core::Color{c.on_surface.r, c.on_surface.g, c.on_surface.b, 0.06f};
    selected_row_bg = c.surface_variant;
    text_color = c.on_surface;
    header_text_color = c.on_surface;
    border_color = c.outline_variant;
    resizer_hover_color = c.primary;
    sort_icon_color = c.on_surface_variant;
    scrollbar_thumb_color = core::Color{c.outline.r, c.outline.g, c.outline.b, 0.60f};

    if (on_theme) {
        on_theme(theme, *this);
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

DataTableBuilder::DataTableBuilder(core::NodeTree& tree) : tree_(tree) {}

DataTableBuilder& DataTableBuilder::column(std::string_view title, float width,
                                           text::TextAlign align, bool sortable, bool resizable) {
    TableColumn col;
    col.title = std::string(title);
    col.width = width;
    col.flex = 0.0f;
    col.align = align;
    col.sortable = sortable;
    col.resizable = resizable;
    data_.columns.push_back(std::move(col));
    return *this;
}

DataTableBuilder& DataTableBuilder::flex_column(std::string_view title, float flex,
                                                float min_width, text::TextAlign align,
                                                bool sortable) {
    TableColumn col;
    col.title = std::string(title);
    col.width = min_width;
    col.flex = flex;
    col.min_width = min_width;
    col.align = align;
    col.sortable = sortable;
    col.resizable = false;
    data_.columns.push_back(std::move(col));
    return *this;
}

DataTableBuilder& DataTableBuilder::row(uint32_t id, std::vector<std::string> cells, void* user_data) {
    data_.rows.push_back(TableRow{id, std::move(cells), user_data});
    return *this;
}

DataTableBuilder& DataTableBuilder::rows(std::vector<TableRow> rows) {
    data_.rows = std::move(rows);
    return *this;
}

DataTableBuilder& DataTableBuilder::header_height(float height) {
    data_.header_height = height;
    return *this;
}

DataTableBuilder& DataTableBuilder::row_height(float height) {
    data_.row_height = height;
    return *this;
}

DataTableBuilder& DataTableBuilder::alternating_rows(bool enabled) {
    data_.alternating_rows = enabled;
    return *this;
}

DataTableBuilder& DataTableBuilder::font(std::shared_ptr<text::Font> font) {
    data_.font = std::move(font);
    return *this;
}

DataTableBuilder& DataTableBuilder::font_size(float size) {
    data_.font_size = size;
    return *this;
}

DataTableBuilder& DataTableBuilder::width(float width) {
    data_.width = width;
    return *this;
}

DataTableBuilder& DataTableBuilder::height(float height) {
    data_.height = height;
    return *this;
}

DataTableBuilder& DataTableBuilder::theme(const style::Theme& theme) {
    data_.apply_theme(theme);
    return *this;
}

DataTableBuilder& DataTableBuilder::theme_signal(core::Signal<style::Theme>* sig, DataTableThemeFn fn) {
    data_.theme_signal = sig;
    data_.on_theme = fn;
    if (sig) {
        data_.apply_theme(sig->get());
    }
    return *this;
}

DataTableBuilder& DataTableBuilder::sort_icon_color(core::Color color) {
    data_.sort_icon_color = color;
    return *this;
}

DataTableBuilder& DataTableBuilder::on_select(TableSelectCallbackFn fn, void* user_data) {
    data_.on_select = fn;
    data_.select_user_data = user_data;
    return *this;
}

DataTableBuilder& DataTableBuilder::on_sort(TableSortCallbackFn fn, void* user_data) {
    data_.on_sort = fn;
    data_.sort_user_data = user_data;
    return *this;
}

DataTableBuilder& DataTableBuilder::on_activate(TableActivateCallbackFn fn, void* user_data) {
    data_.on_activate = fn;
    data_.activate_user_data = user_data;
    return *this;
}

core::Handle<core::Node> DataTableBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node) {
        return {};
    }

    if (!data_.font) {
        data_.font = text::FontManager::instance().load_system_font();
    }

    data_.sorted_indices.resize(data_.rows.size());
    std::iota(data_.sorted_indices.begin(), data_.sorted_indices.end(), 0);

    auto* heap_data = new DataTableData(std::move(data_));
    heap_data->tree = &tree_;
    heap_data->node_handle = handle;

    if (heap_data->theme_signal) {
        heap_data->theme_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* dt = static_cast<DataTableData*>(ctx);
                if (dt && dt->theme_signal) {
                    dt->apply_theme(dt->theme_signal->get());
                }
            },
            heap_data, core::kPaint);
    }

    node->user_data = heap_data;
    node->layout_fn = &data_table_layout_fn;
    node->paint_fn = &data_table_paint_fn;
    node->destroy_fn = &data_table_destroy_fn;

    return handle;
}

} // namespace north::widget
