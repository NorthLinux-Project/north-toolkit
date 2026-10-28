use std::any::Any;

use super::tree::Widget;
use crate::layout::{EdgeInsets, FlexStyle, LayoutRect};
use crate::render::{Border, CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

pub struct Container {
    pub style: FlexStyle,
    pub background: Option<[f32; 4]>,
    pub border: Option<Border>,
    pub corner_radii: CornerRadii,
}

impl Container {
    pub fn new(style: FlexStyle) -> Self {
        Self {
            style,
            background: None,
            border: None,
            corner_radii: CornerRadii::default(),
        }
    }

    pub fn with_background(mut self, color: [f32; 4]) -> Self {
        self.background = Some(color);
        self
    }

    pub fn with_border(mut self, color: [f32; 4], width: f32) -> Self {
        self.border = Some(Border::new(color, width));
        self
    }

    pub fn with_corner_radius(mut self, radius: f32) -> Self {
        self.corner_radii = CornerRadii::all(radius);
        self
    }

    pub fn with_corner_radii(mut self, radii: impl Into<CornerRadii>) -> Self {
        self.corner_radii = radii.into();
        self
    }

    pub fn with_padding(mut self, padding: EdgeInsets) -> Self {
        self.style.padding = padding;
        self
    }
}

impl Widget for Container {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
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
        let mut prims = Vec::new();
        let scaled_rect = [rect.x * scale, rect.y * scale, rect.width * scale, rect.height * scale];
        if let Some(bg) = self.background {
            let scaled_radii = self.corner_radii.scale(scale);
            let prim = match self.border {
                Some(b) => RenderPrimitive::bordered_rect(scaled_rect, bg, scaled_radii, Border::new(b.color, b.width * scale)),
                None => RenderPrimitive::solid_rect(scaled_rect, bg, scaled_radii),
            };
            prims.push(prim);
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
