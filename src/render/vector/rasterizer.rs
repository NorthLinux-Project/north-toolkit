//! High-quality software rasterization of 2D paths to RGBA8 bitmaps using tiny-skia.

use tiny_skia::{Color, FillRule, LineCap, LineJoin, Paint, Pixmap, Stroke, Transform};

use super::path::VectorPath;

/// Line stroke parameters for vector outline rendering.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct StrokeStyle {
    /// RGBA stroke color in range `[0.0, 1.0]`.
    pub color: [f32; 4],
    /// Stroke width in pixels.
    pub width: f32,
    /// Line cap style at stroke endpoints.
    pub cap: LineCap,
    /// Line join style at stroke vertices.
    pub join: LineJoin,
}

impl Default for StrokeStyle {
    fn default() -> Self {
        Self {
            color: [1.0, 1.0, 1.0, 1.0],
            width: 1.5,
            cap: LineCap::Round,
            join: LineJoin::Round,
        }
    }
}

/// Software rasterizer converting vector paths to anti-aliased RGBA8 pixel buffers.
pub struct VectorRasterizer;

impl VectorRasterizer {
    /// Rasterizes a vector path into an RGBA8 pixel buffer of size `width x height`.
    pub fn rasterize(
        path: &VectorPath,
        width: u32,
        height: u32,
        fill: Option<[f32; 4]>,
        stroke: Option<StrokeStyle>,
    ) -> Option<Vec<u8>> {
        if width == 0 || height == 0 {
            return None;
        }

        let mut pixmap = Pixmap::new(width, height)?;
        let bounds = path.compute_bounds();
        let (bx, by, bw, bh) = (bounds[0], bounds[1], bounds[2], bounds[3]);
        if bw <= 0.0 || bh <= 0.0 {
            return None;
        }

        let scale = (width as f32 / bw).min(height as f32 / bh);
        let offset_x = (width as f32 - bw * scale) * 0.5 - bx * scale;
        let offset_y = (height as f32 - bh * scale) * 0.5 - by * scale;

        let skia_path = path.to_tiny_skia(scale, scale, offset_x, offset_y)?;

        if let Some(fill_color) = fill {
            let mut paint = Paint::default();
            paint.set_color(Color::from_rgba(
                fill_color[0],
                fill_color[1],
                fill_color[2],
                fill_color[3],
            )?);
            pixmap.fill_path(&skia_path, &paint, FillRule::Winding, Transform::identity(), None);
        }

        if let Some(s) = stroke {
            let mut paint = Paint::default();
            paint.set_color(Color::from_rgba(
                s.color[0],
                s.color[1],
                s.color[2],
                s.color[3],
            )?);

            let mut skia_stroke = Stroke::default();
            skia_stroke.width = s.width;
            skia_stroke.line_cap = s.cap;
            skia_stroke.line_join = s.join;

            pixmap.stroke_path(&skia_path, &paint, &skia_stroke, Transform::identity(), None);
        }

        Some(pixmap.take())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_rasterize_simple_check() {
        let mut path = VectorPath::new();
        path.move_to(2.0, 6.0);
        path.line_to(6.0, 10.0);
        path.line_to(14.0, 2.0);

        let data = VectorRasterizer::rasterize(
            &path,
            16,
            16,
            None,
            Some(StrokeStyle {
                color: [1.0, 1.0, 1.0, 1.0],
                ..Default::default()
            }),
        );

        assert!(data.is_some());
        let pixels = data.unwrap();
        assert_eq!(pixels.len(), 16 * 16 * 4);
        assert!(pixels.iter().any(|&b| b > 0));
    }
}
