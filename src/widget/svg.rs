//! Arbitrary SVG path vector widget rendered into cached texture atlas pages.

use std::any::Any;
use std::collections::hash_map::DefaultHasher;
use std::hash::{Hash, Hasher};

use super::tree::Widget;
use crate::layout::{Dimension, FlexStyle, LayoutRect, MeasureFunc};
use crate::render::primitives::CornerRadii;
use crate::render::vector::{StrokeStyle, VectorAtlas, VectorPath, VectorRasterizer};
use crate::render::{GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// Vector graphic widget rendering arbitrary SVG path strings with fill and stroke.
pub struct Svg {
    /// Parsed vector path geometry.
    pub path: VectorPath,
    /// 64-bit hash of the SVG source string used for atlas cache lookups.
    pub path_hash: u64,
    /// Optional fill color for closed path areas.
    pub fill_color: Option<[f32; 4]>,
    /// Optional stroke style for path outlines.
    pub stroke: Option<StrokeStyle>,
    /// Logical width in points.
    pub width: f32,
    /// Logical height in points.
    pub height: f32,
    /// Flexbox layout style.
    pub style: FlexStyle,
}

impl Svg {
    /// Parses SVG path data string (such as `"M10 10 H 90 V 90 H 10 Z"`) and constructs an `Svg` widget.
    pub fn from_path_data<S: AsRef<str>>(d: S) -> Self {
        let s = d.as_ref();
        let mut hasher = DefaultHasher::new();
        s.hash(&mut hasher);
        let path_hash = hasher.finish();

        let path = VectorPath::parse_svg(s);
        let bounds = path.compute_bounds();
        let w = bounds[2].max(16.0);
        let h = bounds[3].max(16.0);

        Self {
            path,
            path_hash,
            fill_color: Some([1.0, 1.0, 1.0, 1.0]),
            stroke: None,
            width: w,
            height: h,
            style: FlexStyle {
                width: Dimension::Points(w),
                height: Dimension::Points(h),
                ..Default::default()
            },
        }
    }

    /// Sets the layout dimensions in logical points.
    pub fn with_size(mut self, width: f32, height: f32) -> Self {
        self.width = width;
        self.height = height;
        self.style.width = Dimension::Points(width);
        self.style.height = Dimension::Points(height);
        self
    }

    /// Sets explicit viewBox boundaries `[min_x, min_y, width, height]`.
    pub fn with_view_box(mut self, min_x: f32, min_y: f32, width: f32, height: f32) -> Self {
        self.path.view_box = Some([min_x, min_y, width, height]);
        self
    }

    /// Sets the fill color.
    pub fn with_fill(mut self, color: [f32; 4]) -> Self {
        self.fill_color = Some(color);
        self
    }

    /// Sets the stroke outline style.
    pub fn with_stroke(mut self, stroke: StrokeStyle) -> Self {
        self.stroke = Some(stroke);
        self
    }
}

impl Widget for Svg {
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
        theme: &Theme,
        _atlas: &mut GlyphAtlas,
        _rasterizer: &GlyphRasterizer,
        _shaper: &TextShaper,
        vector_atlas: &mut VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let phys_w = ((rect.width * scale).round() as u32).max(1);
        let phys_h = ((rect.height * scale).round() as u32).max(1);

        let fill = self.fill_color.or(Some(theme.colors.text));
        let path_ref = &self.path;
        let stroke_copy = self.stroke.map(|mut s| {
            s.width *= scale;
            s
        });

        let uv_rect = vector_atlas.get_or_insert(self.path_hash, phys_w, phys_h, || {
            VectorRasterizer::rasterize(
                path_ref,
                phys_w,
                phys_h,
                Some([1.0, 1.0, 1.0, 1.0]),
                stroke_copy,
            )
        });

        if let Some(uv) = uv_rect {
            vec![RenderPrimitive::Rect {
                rect: [
                    rect.x * scale,
                    rect.y * scale,
                    rect.width * scale,
                    rect.height * scale,
                ],
                color: fill.unwrap_or([1.0, 1.0, 1.0, 1.0]),
                radii: CornerRadii::all(0.0),
                border: None,
                uv_rect: uv,
                use_texture: true,
            }]
        } else {
            Vec::new()
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
    fn test_svg_widget_measure_and_paint() {
        let svg = Svg::from_path_data("M 0 0 L 10 10 Z")
            .with_view_box(0.0, 0.0, 10.0, 10.0)
            .with_size(32.0, 32.0)
            .with_fill([1.0, 0.0, 0.0, 1.0]);

        let style = svg.layout_style();
        assert_eq!(style.width, Dimension::Points(32.0));
        assert_eq!(style.height, Dimension::Points(32.0));

        let mut atlas = VectorAtlas::new(128, 128);
        let mut glyph_atlas = GlyphAtlas::new(128, 128, 1);
        let shaper = TextShaper::default_system_font().unwrap();
        let rasterizer = GlyphRasterizer::new(shaper.font_db().clone());
        let theme = Theme::dark();

        let prims = svg.paint(
            LayoutRect::new(0.0, 0.0, 32.0, 32.0),
            &theme,
            &mut glyph_atlas,
            &rasterizer,
            &shaper,
            &mut atlas,
            1.0,
        );

        assert_eq!(prims.len(), 1);
        match &prims[0] {
            RenderPrimitive::Rect { use_texture, uv_rect, color, .. } => {
                assert!(use_texture);
                assert_eq!(*color, [1.0f32, 0.0, 0.0, 1.0]);
                assert!(uv_rect[2] > 0.0);
                assert!(uv_rect[3] > 0.0);
            }
            _ => panic!("Expected textured rect primitive for svg"),
        }
    }
}
