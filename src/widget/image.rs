//! Textured quad image widget for rendering pre-uploaded textures or sprite slices.

use std::any::Any;

use super::tree::Widget;
use crate::layout::{Dimension, FlexStyle, LayoutRect, MeasureFunc};
use crate::render::{GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// Retained-mode textured image display widget.
pub struct Image {
    /// Flexbox layout style.
    pub style: FlexStyle,
    /// Explicit width in logical points.
    pub width: f32,
    /// Explicit height in logical points.
    pub height: f32,
    /// Normalized UV texture coordinates `[u_min, v_min, u_max, v_max]`.
    pub uv_rect: [f32; 4],
    /// Overall transparency multiplier between 0.0 and 1.0.
    pub alpha: f32,
}

impl Image {
    /// Creates a new image widget with specified logical width and height.
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

    /// Sets normalized UV coordinates for texture atlas sampling.
    pub fn with_uv(mut self, uv_rect: [f32; 4]) -> Self {
        self.uv_rect = uv_rect;
        self
    }

    /// Sets the opacity multiplier.
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
        _vector_atlas: &mut crate::render::VectorAtlas,
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
