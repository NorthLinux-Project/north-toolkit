#pragma once

#include "north/core/tree.hpp"
#include "north/style/theme.hpp"
#include "north/widget/widget_data.hpp"

namespace north::widget {

class DataTableBuilder {
public:
    explicit DataTableBuilder(core::NodeTree& tree);

    DataTableBuilder& column(std::string_view title, float width = 120.0f,
                             text::TextAlign align = text::TextAlign::Left,
                             bool sortable = true, bool resizable = true);
    DataTableBuilder& flex_column(std::string_view title, float flex = 1.0f,
                                  float min_width = 48.0f,
                                  text::TextAlign align = text::TextAlign::Left,
                                  bool sortable = true);
    DataTableBuilder& row(uint32_t id, std::vector<std::string> cells, void* user_data = nullptr);
    DataTableBuilder& rows(std::vector<TableRow> rows);
    DataTableBuilder& header_height(float height);
    DataTableBuilder& row_height(float height);
    DataTableBuilder& alternating_rows(bool enabled);
    DataTableBuilder& font(std::shared_ptr<text::Font> font);
    DataTableBuilder& font_size(float size);
    DataTableBuilder& width(float width);
    DataTableBuilder& height(float height);
    DataTableBuilder& theme(const style::Theme& theme);
    DataTableBuilder& theme_signal(core::Signal<style::Theme>* sig, DataTableThemeFn fn = nullptr);
    DataTableBuilder& sort_icon_color(core::Color color);
    DataTableBuilder& on_select(TableSelectCallbackFn fn, void* user_data = nullptr);
    DataTableBuilder& on_sort(TableSortCallbackFn fn, void* user_data = nullptr);
    DataTableBuilder& on_activate(TableActivateCallbackFn fn, void* user_data = nullptr);

    [[nodiscard]] core::Handle<core::Node> build();

private:
    core::NodeTree& tree_;
    DataTableData data_;
};

} // namespace north::widget
