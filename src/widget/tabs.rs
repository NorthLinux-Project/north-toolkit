//! Retained-mode tab navigation bar and container for multi-page views.

use std::any::Any;
use std::cell::{Cell, RefCell};

use super::icon::IconName;
use super::state::Property;
use super::tree::Widget;
use crate::animation::{Easing, Tween};
use crate::event::{CursorIcon, EventResponse, KeyEvent, MouseButton, PointerEvent, WidgetId};
use crate::layout::{
    AlignItems, Dimension, EdgeInsets, FlexDirection, FlexStyle, LayoutRect, MeasureFunc,
};
use crate::render::vector::{StrokeStyle, VectorAtlas, VectorPath, VectorRasterizer};
use crate::render::{Border, CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{emit_text_glyphs, GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// Layout orientation for a tab navigation bar.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum TabBarOrientation {
    /// Tabs are arranged horizontally side-by-side.
    #[default]
    Horizontal,
    /// Tabs are stacked vertically in a column.
    Vertical,
}

/// Specification for an individual tab inside a `TabBar`.
#[derive(Debug, Clone, PartialEq)]
pub struct TabItem {
    /// Label text displayed on the tab.
    pub label: String,
    /// Optional leading vector icon.
    pub icon: Option<IconName>,
    /// Whether this tab can be closed by the user with a close button.
    pub closable: bool,
}

impl TabItem {
    /// Creates a new non-closable tab with the given label.
    pub fn new<S: Into<String>>(label: S) -> Self {
        Self {
            label: label.into(),
            icon: None,
            closable: false,
        }
    }

    /// Attaches a leading icon to the tab.
    pub fn with_icon(mut self, icon: IconName) -> Self {
        self.icon = Some(icon);
        self
    }

    /// Configures whether this tab displays an interactive close button.
    pub fn with_closable(mut self, closable: bool) -> Self {
        self.closable = closable;
        self
    }
}

/// Interactive retained-mode tab bar supporting icons, closable tabs, and keyboard navigation.
pub struct TabBar {
    /// Ordered list of tab items.
    pub items: Vec<TabItem>,
    /// Reactive active tab index.
    pub active_index: Property<usize>,
    /// Layout orientation of the tab strip.
    pub orientation: TabBarOrientation,
    /// Font size for tab labels in logical points.
    pub font_size: f32,
    /// Flexbox layout configuration.
    pub style: FlexStyle,
    /// Whether the tab bar currently holds keyboard focus.
    pub is_focused: bool,
    /// Currently hovered tab index, if any.
    pub hovered_tab: Option<usize>,
    /// Currently hovered tab close button index, if any.
    pub hovered_close: Option<usize>,
    /// Callback triggered when the active tab index changes.
    pub on_change: Option<Box<dyn FnMut(usize)>>,
    /// Callback triggered when a tab's close button is clicked.
    pub on_close: Option<Box<dyn FnMut(usize)>>,
    /// Last closed tab index consumed by parent containers.
    pub last_closed_tab: Option<usize>,
    indicator_offset: RefCell<Tween<f32>>,
    indicator_size: RefCell<Tween<f32>>,
    has_indicator_layout: Cell<bool>,
    cached_widths: RefCell<Vec<f32>>,
}

impl TabBar {
    /// Default height for horizontal tab bars in logical points.
    pub const DEFAULT_TAB_HEIGHT: f32 = 36.0;
    /// Default width for vertical tab bars in logical points.
    pub const DEFAULT_TAB_WIDTH: f32 = 140.0;

    /// Creates a new empty tab bar with the specified orientation.
    pub fn new(orientation: TabBarOrientation) -> Self {
        let (dir, w, h) = match orientation {
            TabBarOrientation::Horizontal => (
                FlexDirection::Row,
                Dimension::Percent(100.0),
                Dimension::Points(Self::DEFAULT_TAB_HEIGHT),
            ),
            TabBarOrientation::Vertical => (
                FlexDirection::Column,
                Dimension::Points(Self::DEFAULT_TAB_WIDTH),
                Dimension::Percent(100.0),
            ),
        };

        Self {
            items: Vec::new(),
            active_index: Property::new(0),
            orientation,
            font_size: 13.0,
            style: FlexStyle {
                direction: dir,
                align_items: AlignItems::Center,
                padding: EdgeInsets::symmetric(4.0, 8.0),
                width: w,
                height: h,
                ..Default::default()
            },
            is_focused: false,
            hovered_tab: None,
            hovered_close: None,
            on_change: None,
            on_close: None,
            last_closed_tab: None,
            indicator_offset: RefCell::new(Tween::new(0.0)),
            indicator_size: RefCell::new(Tween::new(0.0)),
            has_indicator_layout: Cell::new(false),
            cached_widths: RefCell::new(Vec::new()),
        }
    }

    /// Appends a new tab with the given label.
    pub fn with_tab<S: Into<String>>(mut self, label: S) -> Self {
        self.items.push(TabItem::new(label));
        self
    }

    /// Appends a pre-configured `TabItem`.
    pub fn with_tab_item(mut self, item: TabItem) -> Self {
        self.items.push(item);
        self
    }

    /// Appends a tab item dynamically.
    pub fn add_tab<S: Into<String>>(&mut self, label: S) -> &mut Self {
        self.items.push(TabItem::new(label));
        self
    }

    /// Appends a pre-configured `TabItem` dynamically.
    pub fn add_tab_item(&mut self, item: TabItem) -> &mut Self {
        self.items.push(item);
        self
    }

    /// Sets whether all currently added tabs should be closable.
    pub fn with_closable(mut self, closable: bool) -> Self {
        for item in &mut self.items {
            item.closable = closable;
        }
        self
    }

    /// Sets the initial active tab index.
    pub fn with_active_index(mut self, index: usize) -> Self {
        if !self.items.is_empty() {
            self.active_index.set(index.min(self.items.len() - 1));
        } else {
            self.active_index.set(0);
        }
        self
    }

    /// Registers a tab change callback handler.
    pub fn on_change<F: FnMut(usize) + 'static>(mut self, cb: F) -> Self {
        self.on_change = Some(Box::new(cb));
        self
    }

    /// Registers a tab close callback handler.
    pub fn on_close<F: FnMut(usize) + 'static>(mut self, cb: F) -> Self {
        self.on_close = Some(Box::new(cb));
        self
    }

    /// Switches the active tab index and triggers `on_change` if attached.
    pub fn select_tab(&mut self, index: usize) {
        if self.items.is_empty() {
            return;
        }
        let clamped = index.min(self.items.len() - 1);
        if *self.active_index.get() != clamped {
            self.active_index.set(clamped);
            if let Some(cb) = &mut self.on_change {
                cb(clamped);
            }
        }
    }

    /// Takes and clears the last closed tab index.
    pub fn take_closed_tab(&mut self) -> Option<usize> {
        self.last_closed_tab.take()
    }

    /// Closes a tab at the specified index, updating the active index and invoking `on_close`.
    pub fn close_tab(&mut self, index: usize) {
        if index >= self.items.len() {
            return;
        }
        let curr_active = *self.active_index.get();
        self.items.remove(index);
        self.last_closed_tab = Some(index);

        if let Some(cb) = &mut self.on_close {
            cb(index);
        }

        if self.items.is_empty() {
            self.active_index.set(0);
        } else if curr_active >= self.items.len() {
            self.select_tab(self.items.len() - 1);
        } else if curr_active > index {
            self.select_tab(curr_active - 1);
        } else if curr_active == index {
            if let Some(cb) = &mut self.on_change {
                cb(curr_active);
            }
        }
    }

    /// Computes the layout rectangles of each tab item and its optional close button.
    pub fn tab_rects(
        &self,
        container_rect: LayoutRect,
        shaper: Option<&TextShaper>,
    ) -> Vec<(LayoutRect, Option<LayoutRect>)> {
        let mut result = Vec::with_capacity(self.items.len());
        let pad_h = 12.0;
        let icon_size = 14.0;
        let icon_gap = 6.0;
        let close_size = 14.0;
        let close_gap = 8.0;

        if let Some(s) = shaper {
            let mut widths = self.cached_widths.borrow_mut();
            widths.clear();
            for item in &self.items {
                widths.push(s.shape(&item.label, self.font_size).width);
            }
        }

        match self.orientation {
            TabBarOrientation::Horizontal => {
                let mut curr_x = container_rect.x + self.style.padding.left;
                let tab_h = container_rect.height - self.style.padding.vertical();
                let tab_y = container_rect.y + self.style.padding.top;

                for (i, item) in self.items.iter().enumerate() {
                    let text_w = if let Some(s) = shaper {
                        s.shape(&item.label, self.font_size).width
                    } else if let Some(&w) = self.cached_widths.borrow().get(i) {
                        w
                    } else {
                        (item.label.len() as f32 * self.font_size * 0.55).max(20.0)
                    };
                    let mut tab_w = text_w + pad_h * 2.0;
                    if item.icon.is_some() {
                        tab_w += icon_size + icon_gap;
                    }
                    if item.closable {
                        tab_w += close_size + close_gap;
                    }

                    let tab_rect = LayoutRect::new(curr_x, tab_y, tab_w, tab_h);
                    let close_rect = if item.closable {
                        let cx = tab_rect.x + tab_rect.width - pad_h - close_size + 2.0;
                        let cy = tab_rect.y + (tab_rect.height - close_size) * 0.5;
                        Some(LayoutRect::new(cx, cy, close_size, close_size))
                    } else {
                        None
                    };

                    result.push((tab_rect, close_rect));
                    curr_x += tab_w + 4.0;
                }
            }
            TabBarOrientation::Vertical => {
                let mut curr_y = container_rect.y + self.style.padding.top;
                let tab_w = container_rect.width - self.style.padding.horizontal();
                let tab_h = (Self::DEFAULT_TAB_HEIGHT - 4.0).max(28.0);
                let tab_x = container_rect.x + self.style.padding.left;

                for item in &self.items {
                    let tab_rect = LayoutRect::new(tab_x, curr_y, tab_w, tab_h);
                    let close_rect = if item.closable {
                        let cx = tab_rect.x + tab_rect.width - pad_h - close_size;
                        let cy = tab_rect.y + (tab_rect.height - close_size) * 0.5;
                        Some(LayoutRect::new(cx, cy, close_size, close_size))
                    } else {
                        None
                    };

                    result.push((tab_rect, close_rect));
                    curr_y += tab_h + 4.0;
                }
            }
        }

        result
    }

    fn hit_test_tabs(
        &self,
        rect: LayoutRect,
        x: f32,
        y: f32,
    ) -> (Option<usize>, Option<usize>) {
        let rects = self.tab_rects(rect, None);
        for (i, (tab_r, close_opt)) in rects.iter().enumerate() {
            if let Some(close_r) = close_opt {
                if close_r.contains(x, y) {
                    return (Some(i), Some(i));
                }
            }
            if tab_r.contains(x, y) {
                return (Some(i), None);
            }
        }
        (None, None)
    }
}

impl Widget for TabBar {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn measure(&self, shaper: &TextShaper) -> Option<MeasureFunc> {
        let orientation = self.orientation;
        let pad_h = 12.0;
        let icon_size = 14.0;
        let icon_gap = 6.0;
        let close_size = 14.0;
        let close_gap = 8.0;

        match orientation {
            TabBarOrientation::Horizontal => {
                let mut total_w = self.style.padding.horizontal();
                for item in &self.items {
                    let text_w = shaper.shape(&item.label, self.font_size).width;
                    let mut tab_w = text_w + pad_h * 2.0;
                    if item.icon.is_some() {
                        tab_w += icon_size + icon_gap;
                    }
                    if item.closable {
                        tab_w += close_size + close_gap;
                    }
                    total_w += tab_w + 4.0;
                }
                let total_h = Self::DEFAULT_TAB_HEIGHT;
                Some(Box::new(move |_avail_w, _avail_h| (total_w, total_h)))
            }
            TabBarOrientation::Vertical => {
                let total_w = Self::DEFAULT_TAB_WIDTH;
                let count = self.items.len();
                let total_h = self.style.padding.vertical() + count as f32 * 36.0;
                Some(Box::new(move |_avail_w, _avail_h| (total_w, total_h)))
            }
        }
    }

    fn is_focusable(&self) -> bool {
        true
    }

    fn on_focus_change(&mut self, focused: bool) {
        self.is_focused = focused;
    }

    fn cursor_icon_at(&self, _rel_x: f32, _rel_y: f32) -> CursorIcon {
        if self.hovered_tab.is_some() || self.hovered_close.is_some() {
            CursorIcon::Pointer
        } else {
            CursorIcon::Default
        }
    }

    fn on_pointer_event(&mut self, event: &PointerEvent, rect: LayoutRect) -> EventResponse {
        let (px, py) = event.position();

        match *event {
            PointerEvent::Move { x, y } => {
                let (hover_tab, hover_close) = self.hit_test_tabs(rect, x, y);
                if hover_tab != self.hovered_tab || hover_close != self.hovered_close {
                    self.hovered_tab = hover_tab;
                    self.hovered_close = hover_close;
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Down { button, .. } => {
                if button == MouseButton::Left {
                    let (hit_tab, hit_close) = self.hit_test_tabs(rect, px, py);
                    if let Some(close_idx) = hit_close {
                        self.close_tab(close_idx);
                        return EventResponse::Handled;
                    } else if let Some(tab_idx) = hit_tab {
                        self.select_tab(tab_idx);
                        return EventResponse::Handled;
                    }
                }
            }
            _ => {}
        }
        EventResponse::Ignored
    }

    fn tick_animation(&mut self, delta_ms: f32) -> bool {
        let mut t1 = self.indicator_offset.borrow_mut();
        let mut t2 = self.indicator_size.borrow_mut();
        let c1 = !t1.is_complete();
        let c2 = !t2.is_complete();
        if c1 {
            t1.tick(delta_ms);
        }
        if c2 {
            t2.tick(delta_ms);
        }
        c1 || c2
    }

    fn on_key_event(&mut self, event: &KeyEvent) -> EventResponse {
        if !self.is_focused || self.items.is_empty() {
            return EventResponse::Ignored;
        }

        if let KeyEvent::KeyDown { key, keysym, .. } = event {
            let curr = *self.active_index.get();
            let count = self.items.len();

            match key.as_str() {
                "Left" | "Up" => {
                    let prev = if curr == 0 { count - 1 } else { curr - 1 };
                    self.select_tab(prev);
                    return EventResponse::Handled;
                }
                "Right" | "Down" => {
                    let next = if curr + 1 >= count { 0 } else { curr + 1 };
                    self.select_tab(next);
                    return EventResponse::Handled;
                }
                "Home" => {
                    self.select_tab(0);
                    return EventResponse::Handled;
                }
                "End" => {
                    self.select_tab(count - 1);
                    return EventResponse::Handled;
                }
                "Return" | "Enter" | "Space" | " " => {
                    self.select_tab(curr);
                    return EventResponse::Handled;
                }
                _ if *keysym == 0xff0d || *keysym == 0xff8d || *keysym == 0x20 => {
                    self.select_tab(curr);
                    return EventResponse::Handled;
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
        let active = *self.active_index.get();
        let tab_rects = self.tab_rects(rect, Some(shaper));

        if !tab_rects.is_empty() {
            let active_idx = active.min(tab_rects.len() - 1);
            let active_r = tab_rects[active_idx].0;
            let (target_offset, target_size) = match self.orientation {
                TabBarOrientation::Horizontal => (active_r.x, active_r.width),
                TabBarOrientation::Vertical => (active_r.y, active_r.height),
            };

            if !self.has_indicator_layout.get() {
                *self.indicator_offset.borrow_mut() = Tween::new(target_offset);
                *self.indicator_size.borrow_mut() = Tween::new(target_size);
                self.has_indicator_layout.set(true);
            } else {
                let mut off_tween = self.indicator_offset.borrow_mut();
                let mut sz_tween = self.indicator_size.borrow_mut();
                if (off_tween.to - target_offset).abs() > 0.5 {
                    off_tween.set_target(target_offset, 180.0, Easing::EaseOut);
                }
                if (sz_tween.to - target_size).abs() > 0.5 {
                    sz_tween.set_target(target_size, 180.0, Easing::EaseOut);
                }
            }
        }

        let bg_color = theme.colors.surface;
        prims.push(RenderPrimitive::solid_rect(
            [rect.x * scale, rect.y * scale, rect.width * scale, rect.height * scale],
            bg_color,
            CornerRadii::all(theme.border_radius_md * scale),
        ));

        for (i, (tab_r, close_opt)) in tab_rects.iter().enumerate() {
            let is_active = i == active;
            let is_hovered = self.hovered_tab == Some(i);
            let item = &self.items[i];

            let tab_bg = if is_active {
                theme.colors.surface_hover
            } else if is_hovered {
                theme.colors.background
            } else {
                [0.0, 0.0, 0.0, 0.0]
            };

            let radii = CornerRadii::all(theme.border_radius_sm * scale);
            if tab_bg[3] > 0.0 {
                prims.push(RenderPrimitive::solid_rect(
                    [
                        tab_r.x * scale,
                        tab_r.y * scale,
                        tab_r.width * scale,
                        tab_r.height * scale,
                    ],
                    tab_bg,
                    radii,
                ));
            }

            let mut content_x = tab_r.x + 10.0;
            let center_y = tab_r.y + tab_r.height * 0.5;

            if let Some(icon) = item.icon {
                let icon_size = 14.0 * scale;
                let phys_w = (icon_size.round() as u32).max(1);
                let phys_h = phys_w;
                let uv_rect = vector_atlas.get_or_insert(icon as u64, phys_w, phys_h, || {
                    let path = VectorPath::parse_svg(icon.svg_path())
                        .with_view_box(0.0, 0.0, 24.0, 24.0);
                    VectorRasterizer::rasterize(
                        &path,
                        phys_w,
                        phys_h,
                        None,
                        Some(StrokeStyle {
                            color: [1.0, 1.0, 1.0, 1.0],
                            width: 1.8,
                            ..Default::default()
                        }),
                    )
                });

                if let Some(uv) = uv_rect {
                    let iy = center_y - (icon_size / scale) * 0.5;
                    let icon_color = if is_active {
                        theme.colors.primary
                    } else {
                        theme.colors.text_secondary
                    };
                    prims.push(RenderPrimitive::Rect {
                        rect: [
                            content_x * scale,
                            iy * scale,
                            icon_size,
                            icon_size,
                        ],
                        color: icon_color,
                        radii: CornerRadii::all(0.0),
                        border: None,
                        uv_rect: uv,
                        use_texture: true,
                    });
                }
                content_x += 14.0 + 6.0;
            }

            let text_color = if is_active {
                theme.colors.text
            } else if is_hovered {
                theme.colors.text
            } else {
                theme.colors.text_secondary
            };

            let shaped = shaper.shape(&item.label, self.font_size);
            let text_y = center_y - shaped.height * 0.5;
            let baseline = text_y + shaped.height * 0.78;

            emit_text_glyphs(
                &shaped,
                content_x,
                baseline,
                self.font_size,
                text_color,
                scale,
                atlas,
                rasterizer,
                &mut prims,
            );

            if let Some(close_r) = close_opt {
                let is_close_hovered = self.hovered_close == Some(i);
                let close_bg = if is_close_hovered {
                    [1.0, 0.2, 0.2, 0.2]
                } else {
                    [0.0, 0.0, 0.0, 0.0]
                };

                if close_bg[3] > 0.0 {
                    prims.push(RenderPrimitive::solid_rect(
                        [
                            close_r.x * scale,
                            close_r.y * scale,
                            close_r.width * scale,
                            close_r.height * scale,
                        ],
                        close_bg,
                        CornerRadii::all(close_r.width * 0.5 * scale),
                    ));
                }

                let icon_size = close_r.width * scale;
                let phys_w = (icon_size.round() as u32).max(1);
                let phys_h = phys_w;
                let close_uv = vector_atlas.get_or_insert(
                    IconName::Close as u64,
                    phys_w,
                    phys_h,
                    || {
                        let path = VectorPath::parse_svg(IconName::Close.svg_path())
                            .with_view_box(0.0, 0.0, 24.0, 24.0);
                        VectorRasterizer::rasterize(
                            &path,
                            phys_w,
                            phys_h,
                            None,
                            Some(StrokeStyle {
                                color: [1.0, 1.0, 1.0, 1.0],
                                width: 2.0,
                                ..Default::default()
                            }),
                        )
                    },
                );

                if let Some(uv) = close_uv {
                    let close_color = if is_close_hovered {
                        [1.0, 0.3, 0.3, 1.0]
                    } else {
                        theme.colors.text_secondary
                    };
                    prims.push(RenderPrimitive::Rect {
                        rect: [
                            close_r.x * scale,
                            close_r.y * scale,
                            icon_size,
                            icon_size,
                        ],
                        color: close_color,
                        radii: CornerRadii::all(0.0),
                        border: None,
                        uv_rect: uv,
                        use_texture: true,
                    });
                }
            }
        }

        if !self.items.is_empty() {
            let ind_offset = self.indicator_offset.borrow().current;
            let ind_size = self.indicator_size.borrow().current;
            let indicator_color = theme.colors.primary;
            let ind_thickness = 2.5;
            let active_idx = active.min(tab_rects.len().saturating_sub(1));
            let active_r = tab_rects.get(active_idx).map(|(r, _)| *r).unwrap_or(rect);

            match self.orientation {
                TabBarOrientation::Horizontal => {
                    let ind_rect = [
                        ind_offset * scale,
                        (active_r.y + active_r.height - ind_thickness) * scale,
                        ind_size * scale,
                        ind_thickness * scale,
                    ];
                    prims.push(RenderPrimitive::solid_rect(
                        ind_rect,
                        indicator_color,
                        CornerRadii::all(1.0 * scale),
                    ));
                }
                TabBarOrientation::Vertical => {
                    let ind_rect = [
                        active_r.x * scale,
                        ind_offset * scale,
                        ind_thickness * scale,
                        ind_size * scale,
                    ];
                    prims.push(RenderPrimitive::solid_rect(
                        ind_rect,
                        indicator_color,
                        CornerRadii::all(1.0 * scale),
                    ));
                }
            }
        }

        if self.is_focused {
            prims.push(RenderPrimitive::bordered_rect(
                [rect.x * scale, rect.y * scale, rect.width * scale, rect.height * scale],
                [0.0, 0.0, 0.0, 0.0],
                CornerRadii::all(theme.border_radius_md * scale),
                Border::new(theme.colors.border_focused, 1.5 * scale),
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

/// High-level container combining a `TabBar` with child page views.
pub struct Tabs {
    /// Underlying tab navigation bar.
    pub tab_bar: TabBar,
    /// Registered page child widget identifiers mapped to tab indices.
    pub pages: Vec<WidgetId>,
    /// Container flexbox style.
    pub style: FlexStyle,
}

impl Tabs {
    /// Creates a new `Tabs` container with the specified orientation.
    pub fn new(orientation: TabBarOrientation) -> Self {
        let dir = match orientation {
            TabBarOrientation::Horizontal => FlexDirection::Column,
            TabBarOrientation::Vertical => FlexDirection::Row,
        };

        Self {
            tab_bar: TabBar::new(orientation),
            pages: Vec::new(),
            style: FlexStyle {
                direction: dir,
                align_items: AlignItems::Stretch,
                width: Dimension::Percent(100.0),
                height: Dimension::Percent(100.0),
                ..Default::default()
            },
        }
    }

    /// Appends a tab and associates it with the given child page widget ID.
    pub fn add_tab<S: Into<String>>(&mut self, label: S, page_id: WidgetId) -> &mut Self {
        self.tab_bar.add_tab(label);
        self.pages.push(page_id);
        self
    }

    /// Appends a tab item and associates it with the given child page widget ID.
    pub fn with_tab<S: Into<String>>(mut self, label: S, page_id: WidgetId) -> Self {
        self.add_tab(label, page_id);
        self
    }

    /// Removes a tab and its associated page view at the specified index.
    pub fn remove_tab(&mut self, index: usize) -> Option<WidgetId> {
        if index < self.pages.len() {
            self.tab_bar.close_tab(index);
            self.tab_bar.take_closed_tab();
            Some(self.pages.remove(index))
        } else {
            None
        }
    }

    /// Returns the active tab index.
    pub fn active_index(&self) -> usize {
        *self.tab_bar.active_index.get()
    }

    /// Switches the active tab index.
    pub fn select_tab(&mut self, index: usize) {
        self.tab_bar.select_tab(index);
    }

    /// Returns the currently active page `WidgetId`, if any.
    pub fn active_page(&self) -> Option<WidgetId> {
        let idx = self.active_index();
        self.pages.get(idx).copied()
    }

    /// Returns an immutable reference to the inner `TabBar`.
    pub fn tab_bar(&self) -> &TabBar {
        &self.tab_bar
    }

    /// Returns a mutable reference to the inner `TabBar`.
    pub fn tab_bar_mut(&mut self) -> &mut TabBar {
        &mut self.tab_bar
    }
}

impl Widget for Tabs {
    fn layout_style(&self) -> FlexStyle {
        let mut s = self.style.clone();
        match self.tab_bar.orientation {
            TabBarOrientation::Horizontal => {
                s.padding.top = TabBar::DEFAULT_TAB_HEIGHT + 4.0;
            }
            TabBarOrientation::Vertical => {
                s.padding.left = TabBar::DEFAULT_TAB_WIDTH + 4.0;
            }
        }
        s
    }

    fn active_child(&self) -> Option<WidgetId> {
        self.active_page()
    }

    fn is_focusable(&self) -> bool {
        true
    }

    fn on_focus_change(&mut self, focused: bool) {
        self.tab_bar.on_focus_change(focused);
    }

    fn cursor_icon_at(&self, rel_x: f32, rel_y: f32) -> CursorIcon {
        self.tab_bar.cursor_icon_at(rel_x, rel_y)
    }

    fn tick_animation(&mut self, delta_ms: f32) -> bool {
        self.tab_bar.tick_animation(delta_ms)
    }

    fn on_pointer_event(&mut self, event: &PointerEvent, rect: LayoutRect) -> EventResponse {
        let bar_rect = match self.tab_bar.orientation {
            TabBarOrientation::Horizontal => {
                LayoutRect::new(rect.x, rect.y, rect.width, TabBar::DEFAULT_TAB_HEIGHT)
            }
            TabBarOrientation::Vertical => {
                LayoutRect::new(rect.x, rect.y, TabBar::DEFAULT_TAB_WIDTH, rect.height)
            }
        };

        let resp = self.tab_bar.on_pointer_event(event, bar_rect);
        if let Some(closed_idx) = self.tab_bar.take_closed_tab() {
            if closed_idx < self.pages.len() {
                self.pages.remove(closed_idx);
            }
        }
        resp
    }

    fn on_key_event(&mut self, event: &KeyEvent) -> EventResponse {
        self.tab_bar.on_key_event(event)
    }

    fn paint_before(
        &self,
        rect: LayoutRect,
        theme: &Theme,
        atlas: &mut GlyphAtlas,
        rasterizer: &GlyphRasterizer,
        shaper: &TextShaper,
        vector_atlas: &mut VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let bar_rect = match self.tab_bar.orientation {
            TabBarOrientation::Horizontal => {
                LayoutRect::new(rect.x, rect.y, rect.width, TabBar::DEFAULT_TAB_HEIGHT)
            }
            TabBarOrientation::Vertical => {
                LayoutRect::new(rect.x, rect.y, TabBar::DEFAULT_TAB_WIDTH, rect.height)
            }
        };

        self.tab_bar
            .paint(bar_rect, theme, atlas, rasterizer, shaper, vector_atlas, scale)
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
    fn test_tab_bar_selection_and_clamping() {
        let mut tab_bar = TabBar::new(TabBarOrientation::Horizontal)
            .with_tab("First")
            .with_tab("Second")
            .with_tab("Third");

        assert_eq!(*tab_bar.active_index.get(), 0);
        tab_bar.select_tab(1);
        assert_eq!(*tab_bar.active_index.get(), 1);

        tab_bar.select_tab(10);
        assert_eq!(*tab_bar.active_index.get(), 2);
    }

    #[test]
    fn test_tab_bar_keyboard_navigation() {
        let mut tab_bar = TabBar::new(TabBarOrientation::Horizontal)
            .with_tab("A")
            .with_tab("B")
            .with_tab("C");

        tab_bar.on_focus_change(true);

        let left_key = KeyEvent::KeyDown {
            key: "Left".into(),
            keysym: 0xff51,
            text: None,
            modifiers: Default::default(),
            is_repeat: false,
        };
        let right_key = KeyEvent::KeyDown {
            key: "Right".into(),
            keysym: 0xff53,
            text: None,
            modifiers: Default::default(),
            is_repeat: false,
        };

        // Left from 0 wraps to 2
        assert_eq!(tab_bar.on_key_event(&left_key), EventResponse::Handled);
        assert_eq!(*tab_bar.active_index.get(), 2);

        // Right from 2 wraps to 0
        assert_eq!(tab_bar.on_key_event(&right_key), EventResponse::Handled);
        assert_eq!(*tab_bar.active_index.get(), 0);
    }

    #[test]
    fn test_tab_bar_closable_tabs() {
        let mut tab_bar = TabBar::new(TabBarOrientation::Horizontal)
            .with_tab("Tab 0")
            .with_tab("Tab 1")
            .with_tab("Tab 2")
            .with_closable(true);

        tab_bar.select_tab(2);
        assert_eq!(*tab_bar.active_index.get(), 2);

        tab_bar.close_tab(2);
        assert_eq!(tab_bar.items.len(), 2);
        assert_eq!(*tab_bar.active_index.get(), 1);

        tab_bar.close_tab(0);
        assert_eq!(tab_bar.items.len(), 1);
        assert_eq!(*tab_bar.active_index.get(), 0);
    }

    #[test]
    fn test_tabs_container_active_child() {
        let mut tabs = Tabs::new(TabBarOrientation::Horizontal);
        let p1 = WidgetId(10);
        let p2 = WidgetId(20);
        tabs.add_tab("General", p1);
        tabs.add_tab("Advanced", p2);

        assert_eq!(tabs.active_child(), Some(p1));
        tabs.select_tab(1);
        assert_eq!(tabs.active_child(), Some(p2));

        let removed = tabs.remove_tab(0);
        assert_eq!(removed, Some(p1));
        assert_eq!(tabs.pages.len(), 1);
        assert_eq!(tabs.tab_bar.items.len(), 1);
        assert_eq!(tabs.active_child(), Some(p2));
    }

    #[test]
    fn test_tab_bar_indicator_animation() {
        let mut tab_bar = TabBar::new(TabBarOrientation::Horizontal)
            .with_tab("Alpha")
            .with_tab("Beta");

        // Initial tick before layout
        assert!(!tab_bar.tick_animation(16.0));

        // Setup manual targets
        tab_bar.indicator_offset.borrow_mut().set_target(100.0, 180.0, Easing::EaseOut);
        assert!(tab_bar.tick_animation(50.0));
        assert!(tab_bar.indicator_offset.borrow().current > 0.0);
    }
}
