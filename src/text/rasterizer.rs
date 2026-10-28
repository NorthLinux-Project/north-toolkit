use std::sync::Mutex;

use anyhow::Result;
use fontdb::ID;
use swash::scale::{Render, ScaleContext, Source};
use swash::zeno::{Format, Vector};
use swash::FontRef;

use super::font_db::FontDatabase;

#[derive(Debug, Clone)]
pub struct RasterizedGlyph {
    pub width: u32,
    pub height: u32,
    pub offset_x: f32,
    pub offset_y: f32,
    pub pixels: Vec<u8>,
}

pub struct GlyphRasterizer {
    font_db: FontDatabase,
    context: Mutex<ScaleContext>,
}

impl GlyphRasterizer {
    pub fn new(font_db: FontDatabase) -> Self {
        Self {
            font_db,
            context: Mutex::new(ScaleContext::new()),
        }
    }

    pub fn font_db(&self) -> &FontDatabase {
        &self.font_db
    }

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

        assert!(glyph_id > 0);
        let gid = glyph_id as u32;

        let g0_a = rasterizer
            .rasterize(default_face, gid, 18.0, 0)
            .expect("Rasterize failed")
            .expect("Empty glyph");
        let g0_b = rasterizer
            .rasterize(default_face, gid, 18.0, 0)
            .expect("Rasterize failed")
            .expect("Empty glyph");

        assert_eq!(g0_a.width, g0_b.width);
        assert_eq!(g0_a.height, g0_b.height);
        assert_eq!(g0_a.pixels, g0_b.pixels);

        let g1 = rasterizer
            .rasterize(default_face, gid, 18.0, 1)
            .expect("Rasterize failed")
            .expect("Empty glyph");
        assert!(g1.width > 0 && g1.height > 0);
        assert!(!g1.pixels.is_empty());
    }
}
