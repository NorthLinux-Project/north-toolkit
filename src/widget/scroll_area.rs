use std::any::Any;

use super::tree::Widget;
use crate::event::{EventResponse, PointerEvent};
use crate::layout::{Dimension, FlexStyle, LayoutRect};
use crate::render::{GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

pub struct ScrollArea {
    pub style: FlexStyle,
    pub scroll_x: f32,
    pub scroll_y: f32,
    pub max_scroll_y: f32,
}

impl ScrollArea {
    pub fn new(style: FlexStyle) -> Self {
        Self {
            style,
            scroll_x: 0.0,
            scroll_y: 0.0,
            max_scroll_y: 1000.0,
        }
    }

    pub fn with_size(mut self, width: f32, height: f32) -> Self {
        self.style.width = Dimension::Points(width);
        self.style.height = Dimension::Points(height);
        self
    }
}

impl Widget for ScrollArea {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn on_pointer_event(&mut self, event: &PointerEvent, rect: LayoutRect) -> EventResponse {
        match *event {
            PointerEvent::Scroll { x, y, dy, .. } => {
                if rect.contains(x, y) {
                    self.scroll_y = (self.scroll_y - dy * 20.0).clamp(0.0, self.max_scroll_y);
                    return EventResponse::Handled;
                }
            }
            _ => {}
        }
        EventResponse::Ignored
    }

    fn paint(
        &self,
        rect: LayoutRect,
        _theme: &Theme,
        _atlas: &mut GlyphAtlas,
        _rasterizer: &GlyphRasterizer,
        _shaper: &TextShaper,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        vec![
            RenderPrimitive::ClipPush {
                rect: [
                    rect.x * scale,
                    rect.y * scale,
                    rect.width * scale,
                    rect.height * scale,
                ],
            },
            RenderPrimitive::ClipPop,
        ]
    }

    fn as_any(&self) -> &dyn Any {
        self
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}
