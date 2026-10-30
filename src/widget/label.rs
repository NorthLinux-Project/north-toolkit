//! Text label widget with reactive text property binding and font measurement.

use std::any::Any;

use super::state::Property;
use super::tree::Widget;
use crate::layout::{Dimension, FlexStyle, LayoutRect, MeasureFunc};
use crate::render::{GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// Text label widget displaying a reactive string property.
pub struct Label {
    /// Reactive text content.
    pub text: Property<String>,
    /// Font size in logical points.
    pub font_size: f32,
    /// Optional color override (defaults to theme text color).
    pub color: Option<[f32; 4]>,
    /// Flexbox layout style.
    pub style: FlexStyle,
}

impl Label {
    /// Creates a new label widget with initial text.
    pub fn new<S: Into<String>>(text: S) -> Self {
        Self {
            text: Property::new(text.into()),
            font_size: 14.0,
            color: None,
            style: FlexStyle {
                width: Dimension::Auto,
                height: Dimension::Auto,
                ..Default::default()
            },
        }
    }

    /// Sets the label font size in points.
    pub fn with_font_size(mut self, size: f32) -> Self {
        self.font_size = size;
        self
    }

    /// Overrides the text color.
    pub fn with_color(mut self, color: [f32; 4]) -> Self {
        self.color = Some(color);
        self
    }
}

impl Widget for Label {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn measure(&self, shaper: &TextShaper) -> Option<MeasureFunc> {
        let shaped = shaper.shape(self.text.get(), self.font_size);
        let w = shaped.width;
        let h = shaped.height;
        Some(Box::new(move |_avail_w, _avail_h| (w, h)))
    }

    fn paint(
        &self,
        rect: LayoutRect,
        theme: &Theme,
        atlas: &mut GlyphAtlas,
        rasterizer: &GlyphRasterizer,
        shaper: &TextShaper,
        _vector_atlas: &mut crate::render::VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let text_color = self.color.unwrap_or(theme.colors.text);
        let shaped = shaper.shape(self.text.get(), self.font_size);

        let mut primitives = Vec::with_capacity(shaped.glyphs.len());
        let cap_height = self.font_size * 0.70;
        let baseline = if rect.height > 0.0 {
            rect.y + rect.height * 0.5 + cap_height * 0.5
        } else {
            rect.y + shaped.ascent
        };

        crate::text::emit_text_glyphs(
            &shaped,
            rect.x,
            baseline,
            self.font_size,
            text_color,
            scale,
            atlas,
            rasterizer,
            &mut primitives,
        );

        primitives
    }

    fn as_any(&self) -> &dyn Any {
        self
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}
