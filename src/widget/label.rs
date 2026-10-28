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
        let physical_font_size = (self.font_size * scale).round();
        let baseline_y = ((rect.y + shaped.ascent) * scale).round();

        for glyph in shaped.glyphs {
            let px_x = (rect.x + glyph.x) * scale;
            let subpx = (px_x * 4.0).round() as i32;
            let aligned_x = (subpx >> 2) as f32;
            let bucket = (subpx & 3) as u8;

            if let Ok(Some(region)) = atlas.get_or_insert(
                rasterizer,
                glyph.face_id,
                glyph.glyph_id,
                physical_font_size,
                bucket,
            ) {
                let screen_x = aligned_x + region.offset_x;
                let screen_y = baseline_y + region.offset_y;

                primitives.push(RenderPrimitive::glyph(
                    region.page,
                    [screen_x, screen_y, region.width as f32, region.height as f32],
                    region.uv_rect,
                    text_color,
                ));
            }
        }

        primitives
    }

    fn as_any(&self) -> &dyn Any {
        self
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}
