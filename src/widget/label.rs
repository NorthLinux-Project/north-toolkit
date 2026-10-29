use std::any::Any;

use super::state::Property;
use super::tree::Widget;
use crate::layout::{Dimension, FlexStyle, LayoutRect, MeasureFunc};
use crate::render::{GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

pub struct Label {
    pub text: Property<String>,
    pub font_size: f32,
    pub color: Option<[f32; 4]>,
    pub style: FlexStyle,
}

impl Label {
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

    pub fn with_font_size(mut self, size: f32) -> Self {
        self.font_size = size;
        self
    }

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
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let text_color = self.color.unwrap_or(theme.colors.text);
        let shaped = shaper.shape(self.text.get(), self.font_size);

        let mut primitives = Vec::with_capacity(shaped.glyphs.len());
        let baseline = rect.y + shaped.ascent;

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
