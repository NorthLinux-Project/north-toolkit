//! High-performance virtualized list widget supporting large item sets, multi-selection, and custom item renderers.

use std::any::Any;
use std::collections::BTreeSet;

use super::icon::IconName;
use super::tree::Widget;
use crate::event::{CursorIcon, EventResponse, KeyEvent, MouseButton, PointerEvent};
use crate::layout::{Dimension, FlexStyle, LayoutRect, Overflow};
use crate::render::vector::{StrokeStyle, VectorAtlas, VectorPath, VectorRasterizer};
use crate::render::{CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// Standard two-line list item model with optional icon and trailing metadata text.
#[derive(Debug, Clone, PartialEq)]
pub struct ListItem {
    /// Primary title string.
    pub title: String,
    /// Optional secondary subtitle string.
    pub subtitle: Option<String>,
    /// Optional leading vector icon identifier.
    pub icon: Option<IconName>,
    /// Optional right-aligned trailing caption or metadata text.
    pub trailing_text: Option<String>,
}

impl ListItem {
    /// Creates a new list item with the specified title.
    pub fn new<S: Into<String>>(title: S) -> Self {
        Self {
            title: title.into(),
            subtitle: None,
            icon: None,
            trailing_text: None,
        }
    }

    /// Sets the secondary subtitle text.
    pub fn with_subtitle<S: Into<String>>(mut self, subtitle: S) -> Self {
        self.subtitle = Some(subtitle.into());
        self
    }

    /// Sets the leading icon.
    pub fn with_icon(mut self, icon: IconName) -> Self {
        self.icon = Some(icon);
        self
    }

    /// Sets the right-aligned trailing caption text.
    pub fn with_trailing_text<S: Into<String>>(mut self, text: S) -> Self {
        self.trailing_text = Some(text.into());
        self
    }
}

/// Snapshot of transient visual and interaction state passed to item renderers.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ItemState {
    /// Absolute row or item index.
    pub index: usize,
    /// Whether this item is part of the current selection.
    pub is_selected: bool,
    /// Whether the pointer is currently hovering over this item.
    pub is_hovered: bool,
    /// Whether this item currently holds keyboard focus.
    pub is_focused: bool,
    /// Calculated layout rectangle of this item in window coordinates.
    pub rect: LayoutRect,
}

/// Closure signature for custom list item rendering.
pub type ItemRenderer =
    Box<dyn Fn(usize, ItemState, &Theme, &TextShaper, &mut VectorAtlas, f32) -> Vec<RenderPrimitive>>;

/// Retained-mode virtualized list widget rendering only visible items in the viewport.
pub struct VirtualList {
    /// Flexbox layout style.
    pub style: FlexStyle,
    /// Total number of items in the dataset.
    pub item_count: usize,
    /// Default uniform height per row in logical points.
    pub item_height: f32,
    /// Optional dynamic height provider closure for variable-height rows.
    pub height_provider: Option<Box<dyn Fn(usize) -> f32>>,
    /// Set of selected item indices.
    pub selected_indices: BTreeSet<usize>,
    /// Anchor index for range selections (Shift+Click).
    pub anchor_index: Option<usize>,
    /// Focused item index for keyboard arrow navigation.
    pub focused_index: Option<usize>,
    /// Index of the item currently under the pointer cursor.
    pub hovered_index: Option<usize>,
    /// Whether multiple items can be selected simultaneously.
    pub multi_select: bool,
    /// Current vertical scroll position in logical points.
    pub scroll_y: f32,
    /// Target vertical scroll position for smooth scrolling.
    pub target_scroll_y: f32,
    /// Whether the scrollbar thumb is currently being dragged.
    pub is_dragging_thumb: bool,
    /// Pointer y coordinate at the start of thumb drag.
    pub drag_start_pointer_y: f32,
    /// Scroll position at the start of thumb drag.
    pub drag_start_scroll_y: f32,
    /// Whether the scrollbar thumb or track is hovered.
    pub thumb_hovered: bool,
    /// Custom item rendering closure.
    pub item_renderer: Option<ItemRenderer>,
    /// Stored item models if using default rendering.
    pub items: Vec<ListItem>,
    /// Callback invoked when selection changes.
    pub on_select: Option<Box<dyn FnMut(&[usize])>>,
    /// Callback invoked when an item is double-clicked or activated with Enter.
    pub on_activate: Option<Box<dyn FnMut(usize)>>,
}

impl VirtualList {
    /// Default height for uniform list items in logical points.
    pub const DEFAULT_ITEM_HEIGHT: f32 = 36.0;

    /// Creates a virtual list for `item_count` items.
    pub fn new(item_count: usize) -> Self {
        Self {
            style: FlexStyle {
                overflow: Overflow::Scroll,
                width: Dimension::Percent(100.0),
                height: Dimension::Percent(100.0),
                ..Default::default()
            },
            item_count,
            item_height: Self::DEFAULT_ITEM_HEIGHT,
            height_provider: None,
            selected_indices: BTreeSet::new(),
            anchor_index: None,
            focused_index: None,
            hovered_index: None,
            multi_select: false,
            scroll_y: 0.0,
            target_scroll_y: 0.0,
            is_dragging_thumb: false,
            drag_start_pointer_y: 0.0,
            drag_start_scroll_y: 0.0,
            thumb_hovered: false,
            item_renderer: None,
            items: Vec::new(),
            on_select: None,
            on_activate: None,
        }
    }

    /// Creates a virtual list populated with pre-instantiated `ListItem` models.
    pub fn with_items(items: Vec<ListItem>) -> Self {
        let count = items.len();
        let mut list = Self::new(count);
        list.items = items;
        list
    }

    /// Overrides uniform item height in logical points.
    pub fn with_item_height(mut self, height: f32) -> Self {
        self.item_height = height.max(10.0);
        self
    }

    /// Provides dynamic row heights for heterogeneous item sizes.
    pub fn with_height_provider<F: Fn(usize) -> f32 + 'static>(mut self, provider: F) -> Self {
        self.height_provider = Some(Box::new(provider));
        self
    }

    /// Configures whether multi-item selection is enabled.
    pub fn with_multi_select(mut self, multi: bool) -> Self {
        self.multi_select = multi;
        self
    }

    /// Configures a custom closure to render row visuals.
    pub fn with_renderer<F>(mut self, renderer: F) -> Self
    where
        F: Fn(usize, ItemState, &Theme, &TextShaper, &mut VectorAtlas, f32) -> Vec<RenderPrimitive>
            + 'static,
    {
        self.item_renderer = Some(Box::new(renderer));
        self
    }

    /// Attaches a selection change callback handler.
    pub fn on_select<F: FnMut(&[usize]) + 'static>(mut self, cb: F) -> Self {
        self.on_select = Some(Box::new(cb));
        self
    }

    /// Attaches an activation (double-click/Enter) callback handler.
    pub fn on_activate<F: FnMut(usize) + 'static>(mut self, cb: F) -> Self {
        self.on_activate = Some(Box::new(cb));
        self
    }

    /// Calculates total content height summed across all items.
    pub fn total_height(&self) -> f32 {
        if let Some(provider) = &self.height_provider {
            (0..self.item_count).map(|i| provider(i)).sum()
        } else {
            self.item_count as f32 * self.item_height
        }
    }

    /// Computes the maximum vertical scroll offset for the given viewport height.
    pub fn max_scroll_y(&self, viewport_height: f32) -> f32 {
        (self.total_height() - viewport_height).max(0.0)
    }

    /// Computes the half-open range `(start, end)` of item indices intersecting the visible viewport.
    pub fn visible_range(&self, viewport_height: f32) -> (usize, usize) {
        if self.item_count == 0 {
            return (0, 0);
        }

        if let Some(provider) = &self.height_provider {
            let mut curr_y = 0.0;
            let mut start = 0;
            let mut end = 0;
            let mut found_start = false;

            for i in 0..self.item_count {
                let h = provider(i);
                if !found_start && curr_y + h >= self.scroll_y {
                    start = i;
                    found_start = true;
                }
                if curr_y <= self.scroll_y + viewport_height {
                    end = i + 1;
                } else if found_start {
                    break;
                }
                curr_y += h;
            }
            (start, end.min(self.item_count))
        } else {
            let start = ((self.scroll_y / self.item_height).floor().max(0.0) as usize)
                .min(self.item_count);
            let end = ((((self.scroll_y + viewport_height) / self.item_height).ceil().max(0.0)
                as usize)
                + 1)
            .min(self.item_count);
            (start, end)
        }
    }

    /// Returns the vertical distance from top of list to item at `index`.
    pub fn item_y_offset(&self, index: usize) -> f32 {
        if let Some(provider) = &self.height_provider {
            (0..index).map(|i| provider(i)).sum()
        } else {
            index as f32 * self.item_height
        }
    }

    /// Returns the height in points of the item at `index`.
    pub fn item_height_at(&self, index: usize) -> f32 {
        if let Some(provider) = &self.height_provider {
            provider(index)
        } else {
            self.item_height
        }
    }

    /// Adjusts `target_scroll_y` to ensure the item at `index` is completely visible.
    pub fn ensure_visible(&mut self, index: usize, viewport_height: f32) {
        if index >= self.item_count {
            return;
        }

        let item_y = self.item_y_offset(index);
        let item_h = self.item_height_at(index);
        let max_scroll = self.max_scroll_y(viewport_height);

        if item_y < self.target_scroll_y {
            self.target_scroll_y = item_y.clamp(0.0, max_scroll);
        } else if item_y + item_h > self.target_scroll_y + viewport_height {
            self.target_scroll_y = (item_y + item_h - viewport_height).clamp(0.0, max_scroll);
        }
    }

    /// Computes track and thumb rectangles for the vertical scrollbar.
    pub fn track_and_thumb_rects(&self, rect: LayoutRect) -> (LayoutRect, LayoutRect) {
        let track_w = 8.0;
        let track_x = rect.x + rect.width - track_w - 3.0;
        let track_y = rect.y + 3.0;
        let track_h = (rect.height - 6.0).max(10.0);

        let total_h = self.total_height();
        let max_scroll = self.max_scroll_y(rect.height);

        let thumb_ratio = if total_h > 0.0 {
            (rect.height / total_h).clamp(0.08, 1.0)
        } else {
            1.0
        };
        let thumb_h = (track_h * thumb_ratio).max(24.0);
        let scroll_ratio = if max_scroll > 0.0 {
            (self.scroll_y / max_scroll).clamp(0.0, 1.0)
        } else {
            0.0
        };
        let thumb_y = track_y + (track_h - thumb_h) * scroll_ratio;

        (
            LayoutRect::new(track_x, track_y, track_w, track_h),
            LayoutRect::new(track_x, thumb_y, track_w, thumb_h),
        )
    }

    /// Selects a single item index, clearing previous selection.
    pub fn select_single(&mut self, index: usize, viewport_h: f32) {
        self.selected_indices.clear();
        self.selected_indices.insert(index);
        self.focused_index = Some(index);
        self.anchor_index = Some(index);
        self.ensure_visible(index, viewport_h);
        self.notify_select();
    }

    /// Selects an inclusive range of items between anchor and target index.
    pub fn select_range(&mut self, target: usize, viewport_h: f32) {
        let anchor = self.anchor_index.unwrap_or(0);
        self.selected_indices.clear();
        let start = anchor.min(target);
        let end = anchor.max(target);
        for i in start..=end {
            self.selected_indices.insert(i);
        }
        self.focused_index = Some(target);
        self.ensure_visible(target, viewport_h);
        self.notify_select();
    }

    /// Toggles selection membership of a single item index without clearing others.
    pub fn toggle_select(&mut self, index: usize, viewport_h: f32) {
        if self.selected_indices.contains(&index) {
            self.selected_indices.remove(&index);
        } else {
            self.selected_indices.insert(index);
        }
        self.focused_index = Some(index);
        self.anchor_index = Some(index);
        self.ensure_visible(index, viewport_h);
        self.notify_select();
    }

    fn notify_select(&mut self) {
        if let Some(cb) = &mut self.on_select {
            let vec: Vec<usize> = self.selected_indices.iter().copied().collect();
            cb(&vec);
        }
    }

    fn notify_activate(&mut self, index: usize) {
        if let Some(cb) = &mut self.on_activate {
            cb(index);
        }
    }
}

impl Widget for VirtualList {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn is_focusable(&self) -> bool {
        true
    }

    fn cursor_icon_at(&self, _rel_x: f32, _rel_y: f32) -> CursorIcon {
        CursorIcon::Default
    }

    fn tick_animation(&mut self, delta_ms: f32) -> bool {
        let diff = self.target_scroll_y - self.scroll_y;
        if diff.abs() > 0.25 {
            let factor = 1.0 - (-delta_ms / 45.0).exp();
            self.scroll_y += diff * factor;
            true
        } else if diff.abs() > 0.0 {
            self.scroll_y = self.target_scroll_y;
            true
        } else {
            false
        }
    }

    fn on_pointer_event(&mut self, event: &PointerEvent, rect: LayoutRect) -> EventResponse {
        let (track_rect, thumb_rect) = self.track_and_thumb_rects(rect);
        let max_scroll = self.max_scroll_y(rect.height);

        match *event {
            PointerEvent::Scroll { x, y, dy, .. } => {
                if rect.contains(x, y) {
                    let step = dy * 36.0;
                    self.target_scroll_y = (self.target_scroll_y + step).clamp(0.0, max_scroll);
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Down { x, y, button } => {
                if button == MouseButton::Left {
                    if thumb_rect.contains(x, y) {
                        self.is_dragging_thumb = true;
                        self.drag_start_pointer_y = y;
                        self.drag_start_scroll_y = self.scroll_y;
                        return EventResponse::Handled;
                    } else if track_rect.contains(x, y) {
                        let ratio = (y - track_rect.y) / track_rect.height;
                        self.target_scroll_y = (ratio * max_scroll).clamp(0.0, max_scroll);
                        self.scroll_y = self.target_scroll_y;
                        return EventResponse::Handled;
                    } else if rect.contains(x, y) {
                        let rel_y = y - rect.y + self.scroll_y;
                        let clicked_idx = if let Some(provider) = &self.height_provider {
                            let mut accum = 0.0;
                            let mut res = None;
                            for i in 0..self.item_count {
                                let h = provider(i);
                                if rel_y >= accum && rel_y < accum + h {
                                    res = Some(i);
                                    break;
                                }
                                accum += h;
                            }
                            res
                        } else {
                            let idx = (rel_y / self.item_height).floor() as usize;
                            if idx < self.item_count {
                                Some(idx)
                            } else {
                                None
                            }
                        };

                        if let Some(idx) = clicked_idx {
                            self.select_single(idx, rect.height);
                            return EventResponse::Handled;
                        }
                    }
                }
            }
            PointerEvent::Move { x, y } => {
                let was_hovered = self.thumb_hovered;
                self.thumb_hovered = thumb_rect.contains(x, y);

                if self.is_dragging_thumb {
                    let track_h = (track_rect.height - thumb_rect.height).max(1.0);
                    let delta_y = y - self.drag_start_pointer_y;
                    let scroll_delta = (delta_y / track_h) * max_scroll;
                    self.scroll_y =
                        (self.drag_start_scroll_y + scroll_delta).clamp(0.0, max_scroll);
                    self.target_scroll_y = self.scroll_y;
                    return EventResponse::Handled;
                }

                if rect.contains(x, y) && !track_rect.contains(x, y) {
                    let rel_y = y - rect.y + self.scroll_y;
                    let new_hover = if let Some(provider) = &self.height_provider {
                        let mut accum = 0.0;
                        let mut res = None;
                        for i in 0..self.item_count {
                            let h = provider(i);
                            if rel_y >= accum && rel_y < accum + h {
                                res = Some(i);
                                break;
                            }
                            accum += h;
                        }
                        res
                    } else {
                        let idx = (rel_y / self.item_height).floor() as usize;
                        if idx < self.item_count {
                            Some(idx)
                        } else {
                            None
                        }
                    };

                    if new_hover != self.hovered_index {
                        self.hovered_index = new_hover;
                        return EventResponse::Handled;
                    }
                } else if self.hovered_index.is_some() {
                    self.hovered_index = None;
                    return EventResponse::Handled;
                }

                if self.thumb_hovered != was_hovered {
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Up { button, .. } => {
                if button == MouseButton::Left && self.is_dragging_thumb {
                    self.is_dragging_thumb = false;
                    return EventResponse::Handled;
                }
            }
        }

        EventResponse::Ignored
    }

    fn on_key_event(&mut self, event: &KeyEvent) -> EventResponse {
        if self.item_count == 0 {
            return EventResponse::Ignored;
        }

        if let KeyEvent::KeyDown {
            key,
            modifiers,
            is_repeat,
            ..
        } = event
        {
            let curr = self.focused_index.unwrap_or(0);
            match key.as_str() {
                "Up" | "\u{ff51}" => {
                    if curr > 0 {
                        let next = curr - 1;
                        if modifiers.shift && self.multi_select {
                            self.select_range(next, 300.0);
                        } else {
                            self.select_single(next, 300.0);
                        }
                    }
                    return EventResponse::Handled;
                }
                "Down" | "\u{ff53}" => {
                    if curr + 1 < self.item_count {
                        let next = curr + 1;
                        if modifiers.shift && self.multi_select {
                            self.select_range(next, 300.0);
                        } else {
                            self.select_single(next, 300.0);
                        }
                    }
                    return EventResponse::Handled;
                }
                "Home" | "\u{ff50}" => {
                    if modifiers.shift && self.multi_select {
                        self.select_range(0, 300.0);
                    } else {
                        self.select_single(0, 300.0);
                    }
                    return EventResponse::Handled;
                }
                "End" | "\u{ff57}" => {
                    let last = self.item_count.saturating_sub(1);
                    if modifiers.shift && self.multi_select {
                        self.select_range(last, 300.0);
                    } else {
                        self.select_single(last, 300.0);
                    }
                    return EventResponse::Handled;
                }
                "Return" | "Enter" | "KP_Enter" | "\u{ff0d}" => {
                    if !*is_repeat {
                        if let Some(focus) = self.focused_index {
                            self.notify_activate(focus);
                            return EventResponse::Handled;
                        }
                    }
                }
                _ => {}
            }
        }

        EventResponse::Ignored
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

        // Viewport scissor clipping push
        prims.push(RenderPrimitive::ClipPush {
            rect: [
                rect.x * scale,
                rect.y * scale,
                rect.width * scale,
                rect.height * scale,
            ],
        });

        let (start_idx, end_idx) = self.visible_range(rect.height);
        let has_scrollbar = self.total_height() > rect.height;
        let content_w = if has_scrollbar {
            rect.width - 12.0
        } else {
            rect.width
        };

        for idx in start_idx..end_idx {
            let item_y = self.item_y_offset(idx) - self.scroll_y;
            let item_h = self.item_height_at(idx);
            let row_rect = LayoutRect::new(rect.x, rect.y + item_y, content_w, item_h);

            let is_selected = self.selected_indices.contains(&idx);
            let is_hovered = self.hovered_index == Some(idx);
            let is_focused = self.focused_index == Some(idx);

            let state = ItemState {
                index: idx,
                is_selected,
                is_hovered,
                is_focused,
                rect: row_rect,
            };

            if let Some(renderer) = &self.item_renderer {
                let custom_prims = renderer(idx, state, theme, shaper, vector_atlas, scale);
                prims.extend(custom_prims);
            } else if let Some(item) = self.items.get(idx) {
                // Built-in row template rendering
                let bg_color = if is_selected {
                    [0.20, 0.45, 0.90, 0.28]
                } else if is_hovered {
                    [1.0, 1.0, 1.0, 0.05]
                } else if idx % 2 == 1 {
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
                        CornerRadii::all(4.0 * scale),
                    ));
                }

                // Left accent indicator for selected row
                if is_selected {
                    prims.push(RenderPrimitive::solid_rect(
                        [
                            (row_rect.x + 2.0) * scale,
                            (row_rect.y + 4.0) * scale,
                            3.0 * scale,
                            (row_rect.height - 8.0) * scale,
                        ],
                        [0.26, 0.52, 0.96, 1.0],
                        CornerRadii::all(1.5 * scale),
                    ));
                }

                let center_y = row_rect.y + row_rect.height * 0.5;
                let mut text_x = row_rect.x + 14.0;

                // Leading row icon
                if let Some(icon) = item.icon {
                    let icon_size = 15.0;
                    let icon_y = center_y - icon_size * 0.5;
                    let phys_size = ((icon_size * scale).round() as u32).max(1);
                    let stroke_w = (1.4 * (phys_size as f32 / 24.0)).max(1.0);

                    let uv = vector_atlas.get_or_insert(icon as u64, phys_size, phys_size, || {
                        let path = VectorPath::parse_svg(icon.svg_path())
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
                                text_x * scale,
                                icon_y * scale,
                                icon_size * scale,
                                icon_size * scale,
                            ],
                            color: if is_selected {
                                [0.40, 0.70, 1.0, 1.0]
                            } else {
                                theme.colors.text_secondary
                            },
                            radii: CornerRadii::all(0.0),
                            border: None,
                            uv_rect,
                            use_texture: true,
                        });
                    }
                    text_x += icon_size + 8.0;
                }

                // Row Title
                let font_size = 13.0;
                let shaped = shaper.shape(&item.title, font_size);
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

                // Trailing text
                if let Some(trailing) = &item.trailing_text {
                    let trailing_font_size = 11.0;
                    let trailing_shaped = shaper.shape(trailing, trailing_font_size);
                    let trailing_x = row_rect.x + row_rect.width - trailing_shaped.width - 12.0;
                    let trailing_baseline = center_y + trailing_font_size * 0.35;
                    let trailing_color = if is_selected {
                        [0.75, 0.85, 1.0, 1.0]
                    } else {
                        theme.colors.text_secondary
                    };

                    crate::text::emit_text_glyphs(
                        &trailing_shaped,
                        trailing_x,
                        trailing_baseline,
                        trailing_font_size,
                        trailing_color,
                        scale,
                        atlas,
                        rasterizer,
                        &mut prims,
                    );
                }
            }
        }

        // Viewport scissor clipping pop
        prims.push(RenderPrimitive::ClipPop);

        // Scrollbar track & thumb
        if has_scrollbar {
            let (track_rect, thumb_rect) = self.track_and_thumb_rects(rect);
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

            let thumb_color = if self.is_dragging_thumb {
                [0.55, 0.55, 0.55, 0.85]
            } else if self.thumb_hovered {
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
    use crate::event::Modifiers;

    #[test]
    fn test_virtual_list_visible_range_calculation() {
        let mut list = VirtualList::new(1000).with_item_height(40.0);
        let viewport_h = 200.0;

        // At scroll_y = 0: visible range 0..6 (items 0,1,2,3,4,5)
        let (start, end) = list.visible_range(viewport_h);
        assert_eq!(start, 0);
        assert!(end >= 5 && end <= 7);

        // Scroll down 400px (10 items down)
        list.scroll_y = 400.0;
        let (start, end) = list.visible_range(viewport_h);
        assert_eq!(start, 10);
        assert!(end >= 15 && end <= 17);
    }

    #[test]
    fn test_virtual_list_selection_and_keyboard_nav() {
        let mut list = VirtualList::new(100).with_item_height(30.0);

        assert_eq!(list.selected_indices.len(), 0);

        // Click row 5
        let rect = LayoutRect::new(0.0, 0.0, 300.0, 300.0);
        list.on_pointer_event(
            &PointerEvent::Down {
                x: 50.0,
                y: 165.0, // 5 * 30 + 15
                button: MouseButton::Left,
            },
            rect,
        );
        assert!(list.selected_indices.contains(&5));
        assert_eq!(list.focused_index, Some(5));

        // Key Down
        list.on_key_event(&KeyEvent::KeyDown {
            key: "Down".to_string(),
            keysym: 0xff53,
            text: None,
            modifiers: Modifiers::default(),
            is_repeat: false,
        });
        assert!(list.selected_indices.contains(&6));
        assert_eq!(list.focused_index, Some(6));

        // Key Up
        list.on_key_event(&KeyEvent::KeyDown {
            key: "Up".to_string(),
            keysym: 0xff51,
            text: None,
            modifiers: Modifiers::default(),
            is_repeat: false,
        });
        assert!(list.selected_indices.contains(&5));

        // Home key
        list.on_key_event(&KeyEvent::KeyDown {
            key: "Home".to_string(),
            keysym: 0xff50,
            text: None,
            modifiers: Modifiers::default(),
            is_repeat: false,
        });
        assert!(list.selected_indices.contains(&0));

        // End key
        list.on_key_event(&KeyEvent::KeyDown {
            key: "End".to_string(),
            keysym: 0xff57,
            text: None,
            modifiers: Modifiers::default(),
            is_repeat: false,
        });
        assert!(list.selected_indices.contains(&99));

        let activations = std::rc::Rc::new(std::cell::Cell::new(0usize));
        let act_clone = activations.clone();
        let mut list = list.on_activate(move |_| act_clone.set(act_clone.get() + 1));

        list.on_key_event(&KeyEvent::KeyDown {
            key: "Up".to_string(),
            keysym: 0xff51,
            text: None,
            modifiers: Modifiers::default(),
            is_repeat: true,
        });
        assert_eq!(list.focused_index, Some(98));

        list.on_key_event(&KeyEvent::KeyDown {
            key: "Return".to_string(),
            keysym: 0xff0d,
            text: None,
            modifiers: Modifiers::default(),
            is_repeat: true,
        });
        assert_eq!(activations.get(), 0);

        list.on_key_event(&KeyEvent::KeyDown {
            key: "Return".to_string(),
            keysym: 0xff0d,
            text: None,
            modifiers: Modifiers::default(),
            is_repeat: false,
        });
        assert_eq!(activations.get(), 1);
    }
}
