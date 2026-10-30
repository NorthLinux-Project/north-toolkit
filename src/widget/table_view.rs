//! Multi-column data grid widget with column re-sizing, header sorting, and virtualized row scrolling.

use std::any::Any;

use super::icon::IconName;
use super::tree::Widget;
use super::virtual_list::{ItemState, VirtualList};
use crate::event::{CursorIcon, EventResponse, KeyEvent, MouseButton, PointerEvent};
use crate::layout::{Dimension, FlexStyle, LayoutRect, Overflow};
use crate::render::vector::{StrokeStyle, VectorAtlas, VectorPath, VectorRasterizer};
use crate::render::{CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// Sort order direction for column sorting.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SortDirection {
    /// Ascending sort order (A to Z, 0 to 9).
    Ascending,
    /// Descending sort order (Z to A, 9 to 0).
    Descending,
}

/// Column configuration schema for the `TableView` widget.
#[derive(Debug, Clone, PartialEq)]
pub struct TableColumn {
    /// Unique column identifier key.
    pub id: String,
    /// Header label text.
    pub header: String,
    /// Current column width in logical points.
    pub width: f32,
    /// Minimum allowed column width when resizing.
    pub min_width: f32,
    /// Whether user can click the column header to sort rows.
    pub sortable: bool,
}

impl TableColumn {
    /// Creates a new table column with specified id, header, and initial width.
    pub fn new<I: Into<String>, H: Into<String>>(id: I, header: H, width: f32) -> Self {
        Self {
            id: id.into(),
            header: header.into(),
            width: width.max(30.0),
            min_width: 40.0,
            sortable: true,
        }
    }

    /// Sets the minimum allowed column width.
    pub fn with_min_width(mut self, min: f32) -> Self {
        self.min_width = min;
        self
    }

    /// Configures whether this column supports sorting.
    pub fn with_sortable(mut self, sortable: bool) -> Self {
        self.sortable = sortable;
        self
    }
}

/// Type alias for custom table cell rendering closures.
pub type TableCellRenderer =
    Box<dyn Fn(usize, usize, LayoutRect, ItemState, &Theme, &TextShaper, &mut VectorAtlas, f32) -> Vec<RenderPrimitive>>;

/// Virtualized multi-column table data grid widget.
pub struct TableView {
    /// Column definitions and widths.
    pub columns: Vec<TableColumn>,
    /// Underlying virtual list managing vertical row virtualization and scroll physics.
    pub virtual_list: VirtualList,
    /// Height of the table header in points.
    pub header_height: f32,
    /// Currently active sort column index and direction.
    pub sort_column: Option<(usize, SortDirection)>,
    resizing_column: Option<(usize, f32, f32)>, // (col index, start_x, start_w)
    hovered_separator: Option<usize>,
    /// Optional custom cell renderer closure.
    pub cell_renderer: Option<TableCellRenderer>,
    /// Row data backing storage (rows of column cell strings).
    pub rows: Vec<Vec<String>>,
    /// Callback invoked when a column sort order is toggled.
    pub on_sort: Option<Box<dyn FnMut(usize, SortDirection)>>,
}

impl TableView {
    /// Default table header height in logical points.
    pub const DEFAULT_HEADER_HEIGHT: f32 = 32.0;

    /// Creates a new table view with the specified columns and row count.
    pub fn new(columns: Vec<TableColumn>, row_count: usize) -> Self {
        Self {
            columns,
            virtual_list: VirtualList::new(row_count),
            header_height: Self::DEFAULT_HEADER_HEIGHT,
            sort_column: None,
            resizing_column: None,
            hovered_separator: None,
            cell_renderer: None,
            rows: Vec::new(),
            on_sort: None,
        }
    }

    /// Constructs a table view pre-populated with row string vectors.
    pub fn with_rows(columns: Vec<TableColumn>, rows: Vec<Vec<String>>) -> Self {
        let count = rows.len();
        let mut table = Self::new(columns, count);
        table.rows = rows;
        table
    }

    /// Attaches a custom cell rendering closure.
    pub fn with_cell_renderer<F>(mut self, renderer: F) -> Self
    where
        F: Fn(usize, usize, LayoutRect, ItemState, &Theme, &TextShaper, &mut VectorAtlas, f32) -> Vec<RenderPrimitive>
            + 'static,
    {
        self.cell_renderer = Some(Box::new(renderer));
        self
    }

    /// Attaches a column sort callback handler.
    pub fn on_sort<F: FnMut(usize, SortDirection) + 'static>(mut self, cb: F) -> Self {
        self.on_sort = Some(Box::new(cb));
        self
    }

    /// Computes cumulative horizontal start x coordinates for each column header.
    pub fn column_x_offsets(&self, start_x: f32) -> Vec<f32> {
        let mut offsets = Vec::with_capacity(self.columns.len() + 1);
        let mut curr = start_x;
        for col in &self.columns {
            offsets.push(curr);
            curr += col.width;
        }
        offsets.push(curr);
        offsets
    }

    /// Resolves which column separator boundary is within hover tolerance of `(x, y)`.
    pub fn separator_at(&self, rect: LayoutRect, x: f32, y: f32) -> Option<usize> {
        if y < rect.y || y > rect.y + self.header_height {
            return None;
        }

        let offsets = self.column_x_offsets(rect.x);
        for i in 1..offsets.len() {
            let sep_x = offsets[i];
            if (x - sep_x).abs() <= 4.0 {
                return Some(i - 1);
            }
        }
        None
    }

    /// Resolves which column header contains coordinate `(x, y)`.
    pub fn column_at(&self, rect: LayoutRect, x: f32, y: f32) -> Option<usize> {
        if y < rect.y || y > rect.y + self.header_height {
            return None;
        }

        let offsets = self.column_x_offsets(rect.x);
        for i in 0..self.columns.len() {
            if x >= offsets[i] && x < offsets[i + 1] {
                return Some(i);
            }
        }
        None
    }

    /// Sorts stored string rows according to column index and direction with numeric prefix detection.
    pub fn sort_by_column(&mut self, col_idx: usize, dir: SortDirection) {
        if col_idx >= self.columns.len() {
            return;
        }

        self.rows.sort_by(|row_a, row_b| {
            let val_a = row_a.get(col_idx).map(|s| s.as_str()).unwrap_or("");
            let val_b = row_b.get(col_idx).map(|s| s.as_str()).unwrap_or("");

            let parse_num = |s: &str| -> Option<f64> {
                let s = s.trim();
                if let Ok(n) = s.parse::<f64>() {
                    return Some(n);
                }
                let prefix: String = s
                    .chars()
                    .take_while(|c| c.is_ascii_digit() || *c == '.' || *c == '-')
                    .collect();
                prefix.parse::<f64>().ok()
            };

            let cmp = match (parse_num(val_a), parse_num(val_b)) {
                (Some(na), Some(nb)) => na.partial_cmp(&nb).unwrap_or(std::cmp::Ordering::Equal),
                _ => val_a.cmp(val_b),
            };

            match dir {
                SortDirection::Ascending => cmp,
                SortDirection::Descending => cmp.reverse(),
            }
        });
    }
}

impl Widget for TableView {
    fn layout_style(&self) -> FlexStyle {
        FlexStyle {
            overflow: Overflow::Scroll,
            width: Dimension::Percent(100.0),
            height: Dimension::Percent(100.0),
            ..Default::default()
        }
    }

    fn is_focusable(&self) -> bool {
        true
    }

    fn cursor_icon_at(&self, rel_x: f32, rel_y: f32) -> CursorIcon {
        if rel_y <= self.header_height {
            let offsets = self.column_x_offsets(0.0);
            for i in 1..offsets.len() {
                if (rel_x - offsets[i]).abs() <= 4.0 {
                    return CursorIcon::ColResize;
                }
            }
        }
        CursorIcon::Default
    }

    fn tick_animation(&mut self, delta_ms: f32) -> bool {
        self.virtual_list.tick_animation(delta_ms)
    }

    fn on_pointer_event(&mut self, event: &PointerEvent, rect: LayoutRect) -> EventResponse {
        let (px, py) = event.position();

        match *event {
            PointerEvent::Move { x, y } => {
                if let Some((col_idx, start_x, start_w)) = self.resizing_column {
                    let delta = x - start_x;
                    let min_w = self.columns[col_idx].min_width;
                    self.columns[col_idx].width = (start_w + delta).max(min_w);
                    return EventResponse::Handled;
                }

                let new_sep = self.separator_at(rect, x, y);
                if new_sep != self.hovered_separator {
                    self.hovered_separator = new_sep;
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Down { button, .. } => {
                if button == MouseButton::Left {
                    if let Some(sep_idx) = self.separator_at(rect, px, py) {
                        self.resizing_column =
                            Some((sep_idx, px, self.columns[sep_idx].width));
                        return EventResponse::Handled;
                    } else if let Some(col_idx) = self.column_at(rect, px, py) {
                        if self.columns[col_idx].sortable {
                            let new_dir = match self.sort_column {
                                Some((c, SortDirection::Ascending)) if c == col_idx => {
                                    SortDirection::Descending
                                }
                                _ => SortDirection::Ascending,
                            };
                            self.sort_column = Some((col_idx, new_dir));
                            self.sort_by_column(col_idx, new_dir);
                            if let Some(cb) = &mut self.on_sort {
                                cb(col_idx, new_dir);
                            }
                            return EventResponse::Handled;
                        }
                    }
                }
            }
            PointerEvent::Up { button, .. } => {
                if button == MouseButton::Left && self.resizing_column.is_some() {
                    self.resizing_column = None;
                    return EventResponse::Handled;
                }
            }
            _ => {}
        }

        // Forward content row events to virtual_list
        let list_rect = LayoutRect::new(
            rect.x,
            rect.y + self.header_height,
            rect.width,
            (rect.height - self.header_height).max(0.0),
        );
        self.virtual_list.on_pointer_event(event, list_rect)
    }

    fn on_key_event(&mut self, event: &KeyEvent) -> EventResponse {
        self.virtual_list.on_key_event(event)
    }

    fn paint(
        &self,
        rect: LayoutRect,
        theme: &Theme,
        atlas: &mut GlyphAtlas,
        rasterizer: &GlyphRasterizer,
        shaper: &TextShaper,
        vector_atlas: &mut VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let mut prims = Vec::new();

        // 1. Header row background
        let header_bg = [0.11, 0.11, 0.11, 1.0];
        prims.push(RenderPrimitive::solid_rect(
            [
                rect.x * scale,
                rect.y * scale,
                rect.width * scale,
                self.header_height * scale,
            ],
            header_bg,
            CornerRadii::all(0.0),
        ));

        // Bottom divider below header
        let divider_color = [0.22, 0.22, 0.22, 1.0];
        prims.push(RenderPrimitive::solid_rect(
            [
                rect.x * scale,
                (rect.y + self.header_height - 1.0) * scale,
                rect.width * scale,
                1.0 * scale,
            ],
            divider_color,
            CornerRadii::all(0.0),
        ));

        let offsets = self.column_x_offsets(rect.x);
        let center_header_y = rect.y + self.header_height * 0.5;

        // Render column headers
        for (i, col) in self.columns.iter().enumerate() {
            let col_x = offsets[i];

            // Column separator vertical line
            if i > 0 {
                prims.push(RenderPrimitive::solid_rect(
                    [
                        col_x * scale,
                        (rect.y + 4.0) * scale,
                        1.0 * scale,
                        (self.header_height - 8.0) * scale,
                    ],
                    [0.20, 0.20, 0.20, 1.0],
                    CornerRadii::all(0.0),
                ));
            }

            // Header title
            let font_size = 12.0;
            let shaped = shaper.shape(&col.header, font_size);
            let text_x = col_x + 10.0;
            let text_baseline = center_header_y + font_size * 0.35;
            let text_color = [0.75, 0.75, 0.75, 1.0];

            crate::text::emit_text_glyphs(
                &shaped,
                text_x,
                text_baseline,
                font_size,
                text_color,
                scale,
                atlas,
                rasterizer,
                &mut prims,
            );

            // Sort indicator arrow
            if let Some((sorted_idx, dir)) = self.sort_column {
                if sorted_idx == i {
                    let sort_icon = match dir {
                        SortDirection::Ascending => IconName::ChevronUp,
                        SortDirection::Descending => IconName::ChevronDown,
                    };
                    let icon_size = 12.0;
                    let icon_x = text_x + shaped.width + 6.0;
                    let icon_y = center_header_y - icon_size * 0.5;
                    let phys_size = ((icon_size * scale).round() as u32).max(1);
                    let stroke_w = (1.5 * (phys_size as f32 / 24.0)).max(1.0);

                    let uv = vector_atlas.get_or_insert(sort_icon as u64, phys_size, phys_size, || {
                        let path = VectorPath::parse_svg(sort_icon.svg_path())
                            .with_view_box(0.0, 0.0, 24.0, 24.0);
                        VectorRasterizer::rasterize(
                            &path,
                            phys_size,
                            phys_size,
                            None,
                            Some(StrokeStyle {
                                color: [1.0, 1.0, 1.0, 1.0],
                                width: stroke_w,
                                ..Default::default()
                            }),
                        )
                    });

                    if let Some(uv_rect) = uv {
                        prims.push(RenderPrimitive::Rect {
                            rect: [
                                icon_x * scale,
                                icon_y * scale,
                                icon_size * scale,
                                icon_size * scale,
                            ],
                            color: [0.26, 0.52, 0.96, 1.0],
                            radii: CornerRadii::all(0.0),
                            border: None,
                            uv_rect,
                            use_texture: true,
                        });
                    }
                }
            }
        }

        // 2. Render Virtualized Content Rows
        let list_rect = LayoutRect::new(
            rect.x,
            rect.y + self.header_height,
            rect.width,
            (rect.height - self.header_height).max(0.0),
        );

        // Viewport scissor clipping push
        prims.push(RenderPrimitive::ClipPush {
            rect: [
                list_rect.x * scale,
                list_rect.y * scale,
                list_rect.width * scale,
                list_rect.height * scale,
            ],
        });

        let (start_idx, end_idx) = self.virtual_list.visible_range(list_rect.height);
        let has_scrollbar = self.virtual_list.total_height() > list_rect.height;
        let content_w = if has_scrollbar {
            list_rect.width - 12.0
        } else {
            list_rect.width
        };

        for row_idx in start_idx..end_idx {
            let item_y = self.virtual_list.item_y_offset(row_idx) - self.virtual_list.scroll_y;
            let item_h = self.virtual_list.item_height_at(row_idx);
            let row_rect = LayoutRect::new(list_rect.x, list_rect.y + item_y, content_w, item_h);

            let is_selected = self.virtual_list.selected_indices.contains(&row_idx);
            let is_hovered = self.virtual_list.hovered_index == Some(row_idx);
            let is_focused = self.virtual_list.focused_index == Some(row_idx);

            let state = ItemState {
                index: row_idx,
                is_selected,
                is_hovered,
                is_focused,
                rect: row_rect,
            };

            // Row background highlight
            let bg_color = if is_selected {
                [0.20, 0.45, 0.90, 0.28]
            } else if is_hovered {
                [1.0, 1.0, 1.0, 0.04]
            } else if row_idx % 2 == 1 {
                [1.0, 1.0, 1.0, 0.015]
            } else {
                [0.0, 0.0, 0.0, 0.0]
            };

            if bg_color[3] > 0.0 {
                prims.push(RenderPrimitive::solid_rect(
                    [
                        (row_rect.x + 2.0) * scale,
                        (row_rect.y + 1.0) * scale,
                        (row_rect.width - 4.0) * scale,
                        (row_rect.height - 2.0) * scale,
                    ],
                    bg_color,
                    CornerRadii::all(3.0 * scale),
                ));
            }

            // Cell rendering
            for (col_idx, col) in self.columns.iter().enumerate() {
                let cell_x = offsets[col_idx];
                let cell_w = col.width;
                let cell_rect = LayoutRect::new(cell_x, row_rect.y, cell_w, row_rect.height);

                if let Some(renderer) = &self.cell_renderer {
                    let cell_prims = renderer(row_idx, col_idx, cell_rect, state, theme, shaper, vector_atlas, scale);
                    prims.extend(cell_prims);
                } else if let Some(row_cells) = self.rows.get(row_idx) {
                    if let Some(cell_text) = row_cells.get(col_idx) {
                        let font_size = 12.0;
                        let shaped = shaper.shape(cell_text, font_size);
                        let text_x = cell_rect.x + 10.0;
                        let center_y = cell_rect.y + cell_rect.height * 0.5;
                        let baseline = center_y + font_size * 0.35;
                        let text_color = if is_selected {
                            [1.0, 1.0, 1.0, 1.0]
                        } else {
                            theme.colors.text
                        };

                        crate::text::emit_text_glyphs(
                            &shaped,
                            text_x,
                            baseline,
                            font_size,
                            text_color,
                            scale,
                            atlas,
                            rasterizer,
                            &mut prims,
                        );
                    }
                }
            }
        }

        // Viewport scissor clipping pop
        prims.push(RenderPrimitive::ClipPop);

        // Scrollbar track & thumb
        if has_scrollbar {
            let (track_rect, thumb_rect) = self.virtual_list.track_and_thumb_rects(list_rect);
            let track_color = [0.12, 0.12, 0.12, 0.5];
            prims.push(RenderPrimitive::solid_rect(
                [
                    track_rect.x * scale,
                    track_rect.y * scale,
                    track_rect.width * scale,
                    track_rect.height * scale,
                ],
                track_color,
                CornerRadii::all(4.0 * scale),
            ));

            let thumb_color = if self.virtual_list.is_dragging_thumb {
                [0.55, 0.55, 0.55, 0.85]
            } else if self.virtual_list.thumb_hovered {
                [0.45, 0.45, 0.45, 0.75]
            } else {
                [0.32, 0.32, 0.32, 0.60]
            };
            prims.push(RenderPrimitive::solid_rect(
                [
                    thumb_rect.x * scale,
                    thumb_rect.y * scale,
                    thumb_rect.width * scale,
                    thumb_rect.height * scale,
                ],
                thumb_color,
                CornerRadii::all(4.0 * scale),
            ));
        }

        prims
    }

    fn as_any(&self) -> &dyn Any {
        self
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_table_view_column_resizing_and_sorting() {
        let columns = vec![
            TableColumn::new("id", "ID", 80.0),
            TableColumn::new("name", "Name", 200.0),
            TableColumn::new("status", "Status", 120.0),
        ];

        let rows = vec![
            vec!["3".to_string(), "Charlie".to_string(), "Active".to_string()],
            vec!["1".to_string(), "Alice".to_string(), "Idle".to_string()],
            vec!["2".to_string(), "Bob".to_string(), "Active".to_string()],
        ];

        let mut table = TableView::with_rows(columns, rows);
        let rect = LayoutRect::new(0.0, 0.0, 600.0, 400.0);

        // Check separators
        // Sep 0: at x = 80.0
        // Sep 1: at x = 280.0
        assert_eq!(table.separator_at(rect, 80.0, 15.0), Some(0));
        assert_eq!(table.separator_at(rect, 280.0, 15.0), Some(1));
        assert_eq!(table.separator_at(rect, 150.0, 15.0), None);

        // Drag separator 0
        table.on_pointer_event(
            &PointerEvent::Down { x: 80.0, y: 15.0, button: MouseButton::Left },
            rect,
        );
        assert_eq!(table.resizing_column, Some((0, 80.0, 80.0)));

        table.on_pointer_event(
            &PointerEvent::Move { x: 120.0, y: 15.0 },
            rect,
        );
        assert_eq!(table.columns[0].width, 120.0);

        table.on_pointer_event(
            &PointerEvent::Up { x: 120.0, y: 15.0, button: MouseButton::Left },
            rect,
        );
        assert_eq!(table.resizing_column, None);

        // Click column header 1 to sort (Ascending by Name)
        table.on_pointer_event(
            &PointerEvent::Down { x: 200.0, y: 15.0, button: MouseButton::Left },
            rect,
        );
        assert_eq!(table.sort_column, Some((1, SortDirection::Ascending)));
        assert_eq!(table.rows[0][1], "Alice");
        assert_eq!(table.rows[1][1], "Bob");
        assert_eq!(table.rows[2][1], "Charlie");

        // Click again to toggle descending
        table.on_pointer_event(
            &PointerEvent::Down { x: 200.0, y: 15.0, button: MouseButton::Left },
            rect,
        );
        assert_eq!(table.sort_column, Some((1, SortDirection::Descending)));
        assert_eq!(table.rows[0][1], "Charlie");
        assert_eq!(table.rows[1][1], "Bob");
        assert_eq!(table.rows[2][1], "Alice");

        // Click column 0 to sort by ID numerically
        table.on_pointer_event(
            &PointerEvent::Down { x: 40.0, y: 15.0, button: MouseButton::Left },
            rect,
        );
        assert_eq!(table.sort_column, Some((0, SortDirection::Ascending)));
        assert_eq!(table.rows[0][0], "1");
        assert_eq!(table.rows[1][0], "2");
        assert_eq!(table.rows[2][0], "3");
    }
}
