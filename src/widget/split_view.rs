//! Two-pane split container with draggable divider and resizable child panes.

use std::any::Any;
use std::cell::Cell;

use super::state::Property;
use super::tree::Widget;
use crate::event::{CursorIcon, EventResponse, KeyEvent, MouseButton, PointerEvent, WidgetId};
use crate::layout::{
    AlignItems, Dimension, FlexDirection, FlexStyle, LayoutRect,
};
use crate::render::{CornerRadii, GlyphAtlas, RenderPrimitive, VectorAtlas};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// Layout orientation for a two-pane `SplitView`.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum SplitOrientation {
    /// Horizontal split: Left and Right panes with a vertical divider bar.
    #[default]
    Horizontal,
    /// Vertical split: Top and Bottom panes with a horizontal divider bar.
    Vertical,
}

/// A two-pane container with an interactive draggable divider handle and keyboard nudging.
pub struct SplitView {
    /// Layout orientation (Horizontal or Vertical).
    pub orientation: SplitOrientation,
    /// Position of the divider in logical points from the start edge.
    pub split_offset: Property<f32>,
    /// Minimum allowed size for the first (top or left) pane in logical points.
    pub min_first_pane_size: f32,
    /// Minimum allowed size for the second (bottom or right) pane in logical points.
    pub min_second_pane_size: f32,
    /// Divider handle bar thickness in logical points.
    pub divider_thickness: f32,
    /// Keyboard arrow navigation step increment in logical points.
    pub step: f32,
    /// Flexbox layout configuration.
    pub style: FlexStyle,
    /// Whether user is currently dragging the divider.
    pub is_dragging: bool,
    drag_start_pointer: f32,
    drag_start_offset: f32,
    /// Whether pointer is hovering over the divider handle.
    pub divider_hovered: bool,
    /// Whether the split view holds keyboard focus.
    pub is_focused: bool,
    /// Callback triggered when the split position changes.
    pub on_change: Option<Box<dyn FnMut(f32)>>,
    last_container_size: Cell<f32>,
}

impl SplitView {
    /// Default divider thickness in logical points.
    pub const DEFAULT_DIVIDER_THICKNESS: f32 = 6.0;
    /// Default minimum pane size in logical points.
    pub const DEFAULT_MIN_PANE_SIZE: f32 = 60.0;
    /// Default initial split offset in logical points.
    pub const DEFAULT_SPLIT_OFFSET: f32 = 240.0;
    /// Default keyboard arrow nudge step in logical points.
    pub const DEFAULT_STEP: f32 = 10.0;

    /// Creates a new split view with the given orientation.
    pub fn new(orientation: SplitOrientation) -> Self {
        let dir = match orientation {
            SplitOrientation::Horizontal => FlexDirection::Row,
            SplitOrientation::Vertical => FlexDirection::Column,
        };

        Self {
            orientation,
            split_offset: Property::new(Self::DEFAULT_SPLIT_OFFSET),
            min_first_pane_size: Self::DEFAULT_MIN_PANE_SIZE,
            min_second_pane_size: Self::DEFAULT_MIN_PANE_SIZE,
            divider_thickness: Self::DEFAULT_DIVIDER_THICKNESS,
            step: Self::DEFAULT_STEP,
            style: FlexStyle {
                direction: dir,
                align_items: AlignItems::Stretch,
                gap: Self::DEFAULT_DIVIDER_THICKNESS,
                width: Dimension::Percent(100.0),
                height: Dimension::Percent(100.0),
                ..Default::default()
            },
            is_dragging: false,
            drag_start_pointer: 0.0,
            drag_start_offset: Self::DEFAULT_SPLIT_OFFSET,
            divider_hovered: false,
            is_focused: false,
            on_change: None,
            last_container_size: Cell::new(800.0),
        }
    }

    /// Sets the initial split offset in logical points.
    pub fn with_split_offset(mut self, offset: f32) -> Self {
        self.split_offset.set(offset.max(self.min_first_pane_size));
        self
    }

    /// Sets minimum pane size boundaries for both panes.
    pub fn with_min_sizes(mut self, min_first: f32, min_second: f32) -> Self {
        self.min_first_pane_size = min_first.max(10.0);
        self.min_second_pane_size = min_second.max(10.0);
        self
    }

    /// Sets the thickness of the divider bar in logical points.
    pub fn with_divider_thickness(mut self, thickness: f32) -> Self {
        self.divider_thickness = thickness.max(2.0);
        self.style.gap = self.divider_thickness;
        self
    }

    /// Sets the keyboard arrow nudge step in logical points.
    pub fn with_step(mut self, step: f32) -> Self {
        self.step = step.max(1.0);
        self
    }

    /// Registers a split offset change callback handler.
    pub fn on_change<F: FnMut(f32) + 'static>(mut self, cb: F) -> Self {
        self.on_change = Some(Box::new(cb));
        self
    }

    /// Sets the split offset directly with boundary clamping.
    pub fn set_split_offset(&mut self, offset: f32, container_size: f32) {
        let max_offset = (container_size - self.divider_thickness - self.min_second_pane_size)
            .max(self.min_first_pane_size);
        let clamped = offset.clamp(self.min_first_pane_size, max_offset);

        if (*self.split_offset.get() - clamped).abs() > 0.1 {
            self.split_offset.set(clamped);
            if let Some(cb) = &mut self.on_change {
                cb(clamped);
            }
        }
    }

    /// Computes the layout rectangle of the divider bar given the container rectangle.
    pub fn divider_rect(&self, container_rect: LayoutRect) -> LayoutRect {
        let offset = *self.split_offset.get();
        match self.orientation {
            SplitOrientation::Horizontal => LayoutRect::new(
                container_rect.x + offset,
                container_rect.y,
                self.divider_thickness,
                container_rect.height,
            ),
            SplitOrientation::Vertical => LayoutRect::new(
                container_rect.x,
                container_rect.y + offset,
                container_rect.width,
                self.divider_thickness,
            ),
        }
    }
}

impl Widget for SplitView {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn child_layout_style(
        &self,
        child_index: usize,
        _child_id: WidgetId,
        current_style: &FlexStyle,
    ) -> Option<FlexStyle> {
        let offset = *self.split_offset.get();
        let mut s = current_style.clone();
        match child_index {
            0 => {
                match self.orientation {
                    SplitOrientation::Horizontal => {
                        s.width = Dimension::Points(offset);
                        s.flex_grow = 0.0;
                        s.flex_shrink = 0.0;
                    }
                    SplitOrientation::Vertical => {
                        s.height = Dimension::Points(offset);
                        s.flex_grow = 0.0;
                        s.flex_shrink = 0.0;
                    }
                }
                Some(s)
            }
            1 => {
                match self.orientation {
                    SplitOrientation::Horizontal => {
                        s.width = Dimension::Auto;
                        s.flex_grow = 1.0;
                        s.flex_shrink = 1.0;
                    }
                    SplitOrientation::Vertical => {
                        s.height = Dimension::Auto;
                        s.flex_grow = 1.0;
                        s.flex_shrink = 1.0;
                    }
                }
                Some(s)
            }
            _ => None,
        }
    }


    fn is_focusable(&self) -> bool {
        true
    }

    fn on_focus_change(&mut self, focused: bool) {
        self.is_focused = focused;
    }

    fn cursor_icon_at(&self, _rel_x: f32, _rel_y: f32) -> CursorIcon {
        if self.is_dragging || self.divider_hovered {
            match self.orientation {
                SplitOrientation::Horizontal => CursorIcon::ColResize,
                SplitOrientation::Vertical => CursorIcon::RowResize,
            }
        } else {
            CursorIcon::Default
        }
    }

    fn on_pointer_event(&mut self, event: &PointerEvent, rect: LayoutRect) -> EventResponse {
        let (px, py) = event.position();
        let div_r = self.divider_rect(rect);
        let container_size = match self.orientation {
            SplitOrientation::Horizontal => rect.width,
            SplitOrientation::Vertical => rect.height,
        };
        self.last_container_size.set(container_size);

        // Expand hover hit test slightly around divider for ease of grabbing
        let hit_margin = 3.0;
        let hit_rect = match self.orientation {
            SplitOrientation::Horizontal => LayoutRect::new(
                div_r.x - hit_margin,
                div_r.y,
                div_r.width + hit_margin * 2.0,
                div_r.height,
            ),
            SplitOrientation::Vertical => LayoutRect::new(
                div_r.x,
                div_r.y - hit_margin,
                div_r.width,
                div_r.height + hit_margin * 2.0,
            ),
        };

        match *event {
            PointerEvent::Move { x, y } => {
                if self.is_dragging {
                    let curr_coord = match self.orientation {
                        SplitOrientation::Horizontal => x,
                        SplitOrientation::Vertical => y,
                    };
                    let delta = curr_coord - self.drag_start_pointer;
                    let new_offset = self.drag_start_offset + delta;
                    self.set_split_offset(new_offset, container_size);
                    return EventResponse::Handled;
                }

                let inside = hit_rect.contains(x, y);
                if inside != self.divider_hovered {
                    self.divider_hovered = inside;
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Down { button, .. } => {
                if button == MouseButton::Left && hit_rect.contains(px, py) {
                    self.is_dragging = true;
                    self.drag_start_pointer = match self.orientation {
                        SplitOrientation::Horizontal => px,
                        SplitOrientation::Vertical => py,
                    };
                    self.drag_start_offset = *self.split_offset.get();
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Up { button, .. } => {
                if button == MouseButton::Left && self.is_dragging {
                    self.is_dragging = false;
                    return EventResponse::Handled;
                }
            }
            _ => {}
        }

        EventResponse::Ignored
    }

    fn on_key_event(&mut self, event: &KeyEvent) -> EventResponse {
        if !self.is_focused {
            return EventResponse::Ignored;
        }

        if let KeyEvent::KeyDown { key, .. } = event {
            let curr = *self.split_offset.get();
            let container_size = self
                .last_container_size
                .get()
                .max(self.min_first_pane_size + self.divider_thickness + self.min_second_pane_size);
            let max_offset = (container_size - self.divider_thickness - self.min_second_pane_size)
                .max(self.min_first_pane_size);

            match (self.orientation, key.as_str()) {
                (SplitOrientation::Horizontal, "Left") | (SplitOrientation::Vertical, "Up") => {
                    self.set_split_offset(curr - self.step, container_size);
                    return EventResponse::Handled;
                }
                (SplitOrientation::Horizontal, "Right") | (SplitOrientation::Vertical, "Down") => {
                    self.set_split_offset(curr + self.step, container_size);
                    return EventResponse::Handled;
                }
                (_, "Home") => {
                    self.set_split_offset(self.min_first_pane_size, container_size);
                    return EventResponse::Handled;
                }
                (_, "End") => {
                    self.set_split_offset(max_offset, container_size);
                    return EventResponse::Handled;
                }
                _ => {}
            }
        }

        EventResponse::Ignored
    }

    fn paint_after(
        &self,
        rect: LayoutRect,
        theme: &Theme,
        _atlas: &mut GlyphAtlas,
        _rasterizer: &GlyphRasterizer,
        _shaper: &TextShaper,
        _vector_atlas: &mut VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let container_size = match self.orientation {
            SplitOrientation::Horizontal => rect.width,
            SplitOrientation::Vertical => rect.height,
        };
        self.last_container_size.set(container_size);

        let mut prims = Vec::new();
        let div_r = self.divider_rect(rect);

        let divider_color = if self.is_dragging {
            theme.colors.primary
        } else if self.divider_hovered {
            theme.colors.border_focused
        } else {
            theme.colors.border
        };

        // Divider strip
        prims.push(RenderPrimitive::solid_rect(
            [
                div_r.x * scale,
                div_r.y * scale,
                div_r.width * scale,
                div_r.height * scale,
            ],
            divider_color,
            CornerRadii::all(1.0 * scale),
        ));

        // Subtle grip notch pill centered in the divider
        let (grip_w, grip_h) = match self.orientation {
            SplitOrientation::Horizontal => (div_r.width.max(2.0), 32.0),
            SplitOrientation::Vertical => (32.0, div_r.height.max(2.0)),
        };
        let grip_x = div_r.x + (div_r.width - grip_w) * 0.5;
        let grip_y = div_r.y + (div_r.height - grip_h) * 0.5;

        let grip_color = if self.is_dragging {
            theme.colors.primary_hover
        } else if self.divider_hovered {
            theme.colors.text
        } else {
            theme.colors.text_secondary
        };

        prims.push(RenderPrimitive::solid_rect(
            [
                grip_x * scale,
                grip_y * scale,
                grip_w * scale,
                grip_h * scale,
            ],
            grip_color,
            CornerRadii::all(1.5 * scale),
        ));

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
    fn test_split_view_offset_clamping() {
        let mut split = SplitView::new(SplitOrientation::Horizontal)
            .with_min_sizes(50.0, 50.0)
            .with_divider_thickness(6.0);

        // Container width 400.0: max offset = 400 - 6 - 50 = 344
        split.set_split_offset(20.0, 400.0);
        assert_eq!(*split.split_offset.get(), 50.0);

        split.set_split_offset(500.0, 400.0);
        assert_eq!(*split.split_offset.get(), 344.0);

        split.set_split_offset(200.0, 400.0);
        assert_eq!(*split.split_offset.get(), 200.0);
    }

    #[test]
    fn test_split_view_keyboard_nudging() {
        let mut split = SplitView::new(SplitOrientation::Horizontal)
            .with_split_offset(100.0)
            .with_step(15.0);

        split.on_focus_change(true);

        let left_key = KeyEvent::KeyDown {
            key: "Left".into(),
            keysym: 0xff51,
            text: None,
            modifiers: Default::default(),
            is_repeat: false,
        };
        assert_eq!(split.on_key_event(&left_key), EventResponse::Handled);
        assert_eq!(*split.split_offset.get(), 85.0);

        let right_key = KeyEvent::KeyDown {
            key: "Right".into(),
            keysym: 0xff53,
            text: None,
            modifiers: Default::default(),
            is_repeat: false,
        };
        assert_eq!(split.on_key_event(&right_key), EventResponse::Handled);
        assert_eq!(*split.split_offset.get(), 100.0);

        let end_key = KeyEvent::KeyDown {
            key: "End".into(),
            keysym: 0xff57,
            text: None,
            modifiers: Default::default(),
            is_repeat: false,
        };
        assert_eq!(split.on_key_event(&end_key), EventResponse::Handled);
        // Container size defaults to 800: max_offset = 800 - 6 - 60 = 734
        assert_eq!(*split.split_offset.get(), 734.0);

        // Nudging right from max_offset stays clamped at max_offset
        assert_eq!(split.on_key_event(&right_key), EventResponse::Handled);
        assert_eq!(*split.split_offset.get(), 734.0);
    }

    #[test]
    fn test_split_view_child_layout_style_preservation() {
        let split = SplitView::new(SplitOrientation::Horizontal)
            .with_split_offset(300.0);
        let base_child_style = FlexStyle {
            direction: FlexDirection::Column,
            gap: 12.0,
            padding: crate::layout::EdgeInsets::all(16.0),
            ..Default::default()
        };

        let overridden = split.child_layout_style(0, WidgetId(1), &base_child_style).unwrap();
        assert_eq!(overridden.direction, FlexDirection::Column);
        assert_eq!(overridden.gap, 12.0);
        assert_eq!(overridden.padding, crate::layout::EdgeInsets::all(16.0));
        assert_eq!(overridden.width, Dimension::Points(300.0));
    }

    #[test]
    fn test_split_view_cursor_icon() {
        let mut split_h = SplitView::new(SplitOrientation::Horizontal);
        assert_eq!(split_h.cursor_icon_at(0.0, 0.0), CursorIcon::Default);

        split_h.divider_hovered = true;
        assert_eq!(split_h.cursor_icon_at(0.0, 0.0), CursorIcon::ColResize);

        let mut split_v = SplitView::new(SplitOrientation::Vertical);
        split_v.divider_hovered = true;
        assert_eq!(split_v.cursor_icon_at(0.0, 0.0), CursorIcon::RowResize);
    }

    #[test]
    fn test_split_view_children_layout_and_positions() {
        use crate::layout::LayoutEngine;
        use crate::widget::{Container, WidgetTree};

        let font_db = crate::text::font_db::FontDatabase::new().unwrap();
        let shaper = TextShaper::new(font_db);
        let mut engine = LayoutEngine::new();
        let mut tree = WidgetTree::new();

        let root = Container::new(FlexStyle {
            direction: FlexDirection::Column,
            width: Dimension::Points(1000.0),
            height: Dimension::Points(600.0),
            ..Default::default()
        });
        let root_id = tree.add_widget(&mut engine, Box::new(root), None);

        let split = SplitView::new(SplitOrientation::Horizontal)
            .with_split_offset(400.0)
            .with_divider_thickness(10.0);
        let split_id = tree.add_widget(&mut engine, Box::new(split), Some(root_id));

        let left = Container::new(FlexStyle {
            direction: FlexDirection::Column,
            height: Dimension::Percent(100.0),
            ..Default::default()
        });
        let left_id = tree.add_widget(&mut engine, Box::new(left), Some(split_id));

        let right = Container::new(FlexStyle {
            direction: FlexDirection::Column,
            height: Dimension::Percent(100.0),
            ..Default::default()
        });
        let right_id = tree.add_widget(&mut engine, Box::new(right), Some(split_id));

        tree.sync_layout(&mut engine, &shaper);
        let root_node = tree.nodes[root_id.0].as_ref().unwrap().layout_node;
        engine.compute(root_node, 1000.0, 600.0);

        let split_rect = engine.get_rect(tree.nodes[split_id.0].as_ref().unwrap().layout_node);
        let left_rect = engine.get_rect(tree.nodes[left_id.0].as_ref().unwrap().layout_node);
        let right_rect = engine.get_rect(tree.nodes[right_id.0].as_ref().unwrap().layout_node);

        assert_eq!(split_rect.width, 1000.0);
        assert_eq!(split_rect.height, 600.0);

        // Left pane offset is 400.0
        assert_eq!(left_rect.x, 0.0);
        assert_eq!(left_rect.width, 400.0);
        assert_eq!(left_rect.height, 600.0);

        // Right pane begins after offset + divider_thickness (400 + 10 = 410)
        assert_eq!(right_rect.x, 410.0);
        assert_eq!(right_rect.width, 590.0);
        assert_eq!(right_rect.height, 600.0);
    }
}
