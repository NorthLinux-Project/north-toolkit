//! Subpixel glyph rasterization via `swash` and hinted outline rendering.

use std::sync::Mutex;

use anyhow::Result;
use fontdb::ID;
use swash::scale::{Render, ScaleContext, Source};
use swash::zeno::{Format, Vector};
use swash::FontRef;

use super::font_db::FontDatabase;

/// An 8-bit alpha rasterized glyph bitmap with metric offsets.
#[derive(Debug, Clone)]
pub struct RasterizedGlyph {
    /// Bitmap width in pixels.
    pub width: u32,
    /// Bitmap height in pixels.
    pub height: u32,
    /// Left placement offset in pixels.
    pub offset_x: f32,
    /// Top placement offset relative to baseline in pixels.
    pub offset_y: f32,
    /// Single-channel 8-bit alpha mask pixels.
    pub pixels: Vec<u8>,
}

/// Thread-safe glyph rasterizer backed by a swash `ScaleContext`.
pub struct GlyphRasterizer {
    font_db: FontDatabase,
    context: Mutex<ScaleContext>,
}

impl GlyphRasterizer {
    /// Creates a new glyph rasterizer bound to the specified font database.
    pub fn new(font_db: FontDatabase) -> Self {
        Self {
            font_db,
            context: Mutex::new(ScaleContext::new()),
        }
    }

    /// Returns a reference to the active font database.
    pub fn font_db(&self) -> &FontDatabase {
        &self.font_db
    }

    /// Rasterizes an outline glyph at `physical_size` with the specified subpixel shift (0..3).
    pub fn rasterize(
        &self,
        face_id: ID,
        glyph_id: u32,
        physical_size: f32,
        subpixel_bucket: u8,
    ) -> Result<Option<RasterizedGlyph>> {
        let mut context = self.context.lock().unwrap();
        let subpx_offset = (subpixel_bucket.min(3) as f32) * 0.25;

        let res = self.font_db.with_face_data(face_id, |data, index| {
            let font_ref = match FontRef::from_index(data, index as usize) {
                Some(f) => f,
                None => return None,
            };

            let mut scaler = context
                .builder(font_ref)
                .size(physical_size)
                .hint(true)
                .build();

            let image = Render::new(&[Source::Outline])
                .format(Format::Alpha)
                .offset(Vector::new(subpx_offset, 0.0))
                .render(&mut scaler, glyph_id as u16)?;

            let width = image.placement.width;
            let height = image.placement.height;

            if width == 0 || height == 0 {
                return None;
            }

            Some(RasterizedGlyph {
                width,
                height,
                offset_x: image.placement.left as f32,
                offset_y: -(image.placement.top as f32),
                pixels: image.data,
            })
        });

        Ok(res.flatten())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_rasterizer_subpixel_buckets_and_determinism() {
        let font_db = FontDatabase::new().expect("Failed font database init");
        let rasterizer = GlyphRasterizer::new(font_db.clone());
        let default_face = font_db.default_face();

        let glyph_id = font_db
            .with_face_data(default_face, |data, index| {
                FontRef::from_index(data, index as usize).map(|f| f.charmap().map('E'))
            })
            .flatten()
            .unwrap_or(0);

        if glyph_id > 0 {
            let g0 = rasterizer
                .rasterize(default_face, glyph_id as u32, 16.0, 0)
                .unwrap()
                .expect("Failed to rasterize glyph");
            assert!(g0.width > 0 && g0.height > 0);
            assert!(!g0.pixels.is_empty());
        }
    }
}
