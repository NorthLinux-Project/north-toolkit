use std::any::Any;

use super::tree::Widget;
use crate::layout::{Dimension, FlexStyle, LayoutRect, MeasureFunc};
use crate::render::{GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

pub struct Image {
    pub style: FlexStyle,
    pub width: f32,
    pub height: f32,
    pub uv_rect: [f32; 4],
    pub alpha: f32,
}

impl Image {
    pub fn new(width: f32, height: f32) -> Self {
        Self {
            style: FlexStyle {
                width: Dimension::Points(width),
                height: Dimension::Points(height),
                ..Default::default()
            },
            width,
            height,
            uv_rect: [0.0, 0.0, 1.0, 1.0],
            alpha: 1.0,
        }
    }

    pub fn with_uv(mut self, uv_rect: [f32; 4]) -> Self {
        self.uv_rect = uv_rect;
        self
    }

    pub fn with_alpha(mut self, alpha: f32) -> Self {
        self.alpha = alpha;
        self
    }
}

impl Widget for Image {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn measure(&self, _shaper: &TextShaper) -> Option<MeasureFunc> {
        let (w, h) = (self.width, self.height);
        Some(Box::new(move |_avail_w, _avail_h| (w, h)))
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
        vec![RenderPrimitive::textured_rect(
            [
                rect.x * scale,
                rect.y * scale,
                rect.width * scale,
                rect.height * scale,
            ],
            self.uv_rect,
            self.alpha,
        )]
    }

    fn as_any(&self) -> &dyn Any {
        self
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}
