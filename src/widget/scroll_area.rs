//! Scrollable viewport container with animated scroll inertia and interactive scrollbar thumb dragging.

use std::any::Any;

use super::tree::Widget;
use crate::event::{EventResponse, MouseButton, PointerEvent};
use crate::layout::{Dimension, FlexStyle, LayoutRect, Overflow};
use crate::render::{CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// Viewport widget providing smooth scrolling and draggable scrollbars for oversized content.
pub struct ScrollArea {
    /// Flexbox layout style.
    pub style: FlexStyle,
    /// Current horizontal scroll offset in logical points.
    pub scroll_x: f32,
    /// Current vertical scroll offset in logical points.
    pub scroll_y: f32,
    /// Target vertical scroll offset for smooth spring-damper interpolation.
    pub target_scroll_y: f32,
    /// Maximum vertical scroll limit in logical points.
    pub max_scroll_y: f32,
    /// Whether the user is actively dragging the scrollbar thumb with the pointer.
    pub is_dragging_thumb: bool,
    /// Pointer y coordinate at the start of the thumb drag gesture.
    pub drag_start_pointer_y: f32,
    /// Scroll offset at the start of the thumb drag gesture.
    pub drag_start_scroll_y: f32,
    /// Whether the scrollbar thumb or track is currently hovered by the pointer.
    pub thumb_hovered: bool,
}

impl ScrollArea {
    /// Creates a new scroll area wrapping the provided style and setting overflow to scroll.
    pub fn new(mut style: FlexStyle) -> Self {
        style.overflow = Overflow::Scroll;
        Self {
            style,
            scroll_x: 0.0,
            scroll_y: 0.0,
            target_scroll_y: 0.0,
            max_scroll_y: 1000.0,
            is_dragging_thumb: false,
            drag_start_pointer_y: 0.0,
            drag_start_scroll_y: 0.0,
            thumb_hovered: false,
        }
    }

    /// Sets the fixed dimensions of the scroll area viewport.
    pub fn with_size(mut self, width: f32, height: f32) -> Self {
        self.style.width = Dimension::Points(width);
        self.style.height = Dimension::Points(height);
        self
    }

    /// Sets the maximum vertical scroll boundary.
    pub fn with_max_scroll(mut self, max: f32) -> Self {
        self.max_scroll_y = max;
        self
    }

    /// Computes the track and thumb layout rectangles for the vertical scrollbar.
    pub fn track_and_thumb_rects(&self, rect: LayoutRect) -> (LayoutRect, LayoutRect) {
        let track_w = 8.0;
        let track_x = rect.x + rect.width - track_w - 3.0;
        let track_y = rect.y + 3.0;
        let track_h = (rect.height - 6.0).max(10.0);

        let thumb_ratio = (rect.height / (rect.height + self.max_scroll_y)).clamp(0.08, 1.0);
        let thumb_h = (track_h * thumb_ratio).max(24.0);
        let scroll_ratio = if self.max_scroll_y > 0.0 {
            (self.scroll_y / self.max_scroll_y).clamp(0.0, 1.0)
        } else {
            0.0
        };
        let thumb_y = track_y + (track_h - thumb_h) * scroll_ratio;

        (
            LayoutRect::new(track_x, track_y, track_w, track_h),
            LayoutRect::new(track_x, thumb_y, track_w, thumb_h),
        )
    }
}

impl Widget for ScrollArea {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn scroll_offset(&self) -> (f32, f32) {
        (self.scroll_x, self.scroll_y)
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

        match *event {
            PointerEvent::Scroll { x, y, dy, .. } => {
                if rect.contains(x, y) {
                    let step = dy * 36.0;
                    self.target_scroll_y =
                        (self.target_scroll_y + step).clamp(0.0, self.max_scroll_y);
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
                        let available_track = track_rect.height - thumb_rect.height;
                        if available_track > 0.0 {
                            let click_ratio = ((y - track_rect.y - thumb_rect.height * 0.5)
                                / available_track)
                                .clamp(0.0, 1.0);
                            let target = click_ratio * self.max_scroll_y;
                            self.target_scroll_y = target;
                            self.scroll_y = target;
                        }
                        return EventResponse::Handled;
                    }
                }
            }
            PointerEvent::Move { x, y } => {
                if self.is_dragging_thumb {
                    let delta_y = y - self.drag_start_pointer_y;
                    let available_track = track_rect.height - thumb_rect.height;
                    if available_track > 0.0 {
                        let delta_ratio = delta_y / available_track;
                        let new_scroll = (self.drag_start_scroll_y
                            + delta_ratio * self.max_scroll_y)
                            .clamp(0.0, self.max_scroll_y);
                        self.scroll_y = new_scroll;
                        self.target_scroll_y = new_scroll;
                    }
                    return EventResponse::Handled;
                }

                let is_hovering = thumb_rect.contains(x, y) || track_rect.contains(x, y);
                if self.thumb_hovered != is_hovering {
                    self.thumb_hovered = is_hovering;
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

    fn paint_before(
        &self,
        rect: LayoutRect,
        _theme: &Theme,
        _atlas: &mut GlyphAtlas,
        _rasterizer: &GlyphRasterizer,
        _shaper: &TextShaper,
        _vector_atlas: &mut crate::render::VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        vec![RenderPrimitive::ClipPush {
            rect: [
                rect.x * scale,
                rect.y * scale,
                rect.width * scale,
                rect.height * scale,
            ],
        }]
    }

    fn paint_after(
        &self,
        rect: LayoutRect,
        _theme: &Theme,
        _atlas: &mut GlyphAtlas,
        _rasterizer: &GlyphRasterizer,
        _shaper: &TextShaper,
        _vector_atlas: &mut crate::render::VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let mut prims = Vec::new();
        if self.max_scroll_y > 0.0 && rect.height > 20.0 {
            let (track_rect, thumb_rect) = self.track_and_thumb_rects(rect);

            if self.is_dragging_thumb || self.thumb_hovered {
                prims.push(RenderPrimitive::solid_rect(
                    [
                        track_rect.x * scale,
                        track_rect.y * scale,
                        track_rect.width * scale,
                        track_rect.height * scale,
                    ],
                    [0.18, 0.18, 0.18, 0.40],
                    CornerRadii::all(4.0 * scale),
                ));
            }

            let thumb_color = if self.is_dragging_thumb {
                [0.48, 0.48, 0.48, 0.90]
            } else if self.thumb_hovered {
                [0.38, 0.38, 0.38, 0.75]
            } else {
                [0.28, 0.28, 0.28, 0.60]
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
        prims.push(RenderPrimitive::ClipPop);
        prims
    }

    fn cursor_icon_at(&self, rel_x: f32, _rel_y: f32) -> crate::event::CursorIcon {
        let width = self.style.width.resolve(None).unwrap_or(0.0);
        if self.is_dragging_thumb {
            crate::event::CursorIcon::Grabbing
        } else if self.thumb_hovered || (width > 0.0 && rel_x >= width - 14.0) {
            crate::event::CursorIcon::Pointer
        } else {
            crate::event::CursorIcon::Default
        }
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
    fn test_scroll_area_clipping_and_scrolling() {
        let shaper = TextShaper::default_system_font().expect("Failed font load");
        let rasterizer = GlyphRasterizer::new(shaper.font_db().clone());
        let mut atlas = GlyphAtlas::new(512, 512, 1);
        let mut vector_atlas = crate::render::VectorAtlas::new(256, 256);
        let theme = Theme::dark();

        let mut scroll = ScrollArea::new(FlexStyle::default()).with_max_scroll(500.0);
        let rect = LayoutRect::new(10.0, 20.0, 200.0, 300.0);

        let before = scroll.paint_before(rect, &theme, &mut atlas, &rasterizer, &shaper, &mut vector_atlas, 1.0);
        assert_eq!(before.len(), 1);
        match &before[0] {
            RenderPrimitive::ClipPush { rect: clip_rect } => {
                assert_eq!(*clip_rect, [10.0, 20.0, 200.0, 300.0]);
            }
            _ => panic!("Expected ClipPush"),
        }

        let after = scroll.paint_after(rect, &theme, &mut atlas, &rasterizer, &shaper, &mut vector_atlas, 1.0);
        assert!(!after.is_empty());
        assert_eq!(after.last().unwrap(), &RenderPrimitive::ClipPop);

        assert_eq!(scroll.scroll_offset(), (0.0, 0.0));
        let resp = scroll.on_pointer_event(
            &PointerEvent::Scroll {
                x: 50.0,
                y: 50.0,
                dx: 0.0,
                dy: 2.0,
            },
            rect,
        );
        assert_eq!(resp, EventResponse::Handled);
        assert_eq!(scroll.target_scroll_y, 72.0);

        let animating = scroll.tick_animation(16.0);
        assert!(animating);
        assert!(scroll.scroll_y > 0.0);

        // Tick until finish
        while scroll.tick_animation(50.0) {}
        assert_eq!(scroll.scroll_y, 72.0);
    }
}
