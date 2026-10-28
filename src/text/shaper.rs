use std::sync::{Arc, Mutex};

use anyhow::Result;
use fontdb::ID;
use lru::LruCache;

use super::font_db::FontDatabase;

#[derive(Debug, Clone, PartialEq)]
pub struct ShapedGlyph {
    pub face_id: ID,
    pub glyph_id: u32,
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
    pub x_advance: f32,
    pub cluster: u32,
}

#[derive(Debug, Clone, PartialEq)]
pub struct ShapedText {
    pub glyphs: Vec<ShapedGlyph>,
    pub width: f32,
    pub height: f32,
    pub ascent: f32,
    pub descent: f32,
    pub line_gap: f32,
}

#[derive(Clone)]
pub struct TextShaper {
    font_db: FontDatabase,
    cache: Arc<Mutex<LruCache<(String, ID, u32), ShapedText>>>,
}

impl TextShaper {
    pub fn new(font_db: FontDatabase) -> Self {
        Self {
            font_db,
            cache: Arc::new(Mutex::new(LruCache::new(
                std::num::NonZeroUsize::new(1024).unwrap(),
            ))),
        }
    }

    pub fn default_system_font() -> Result<Self> {
        let font_db = FontDatabase::new()?;
        Ok(Self::new(font_db))
    }

    pub fn font_db(&self) -> &FontDatabase {
        &self.font_db
    }

    pub fn shape(&self, text: &str, font_size: f32) -> ShapedText {
        self.shape_with_face(text, font_size, self.font_db.default_face())
    }

    pub fn shape_with_face(&self, text: &str, font_size: f32, preferred_face: ID) -> ShapedText {
        if text.is_empty() {
            return ShapedText {
                glyphs: Vec::new(),
                width: 0.0,
                height: font_size * 1.2,
                ascent: font_size * 0.8,
                descent: font_size * 0.2,
                line_gap: font_size * 0.2,
            };
        }

        let cache_key = (text.to_string(), preferred_face, font_size.to_bits());

        if let Ok(mut cache) = self.cache.lock() {
            if let Some(cached) = cache.get(&cache_key) {
                return cached.clone();
            }
        }

        let mut runs: Vec<(ID, String)> = Vec::new();
        for ch in text.chars() {
            let face_id = self.font_db.face_for_char(ch, preferred_face);
            if let Some(last) = runs.last_mut() {
                if last.0 == face_id {
                    last.1.push(ch);
                    continue;
                }
            }
            runs.push((face_id, ch.to_string()));
        }

        let mut all_glyphs = Vec::new();
        let mut cursor_x = 0.0f32;
        let mut max_ascent = 0.0f32;
        let mut max_descent = 0.0f32;
        let mut max_line_gap = 0.0f32;
        let mut global_cluster_offset = 0u32;

        for (face_id, run_text) in runs {
            let run_len = run_text.chars().count() as u32;
            self.font_db.with_face_data(face_id, |data, index| {
                if let Some(face) = rustybuzz::Face::from_slice(data, index) {
                    let upem = face.units_per_em() as f32;
                    let scale_factor = font_size / upem;

                    let asc = face.ascender() as f32 * scale_factor;
                    let desc = -face.descender() as f32 * scale_factor;
                    let lgap = face.line_gap() as f32 * scale_factor;

                    if asc > max_ascent {
                        max_ascent = asc;
                    }
                    if desc > max_descent {
                        max_descent = desc;
                    }
                    if lgap > max_line_gap {
                        max_line_gap = lgap;
                    }

                    let mut buffer = rustybuzz::UnicodeBuffer::new();
                    buffer.push_str(&run_text);
                    let output = rustybuzz::shape(&face, &[], buffer);

                    let infos = output.glyph_infos();
                    let positions = output.glyph_positions();

                    for (info, pos) in infos.iter().zip(positions.iter()) {
                        let x_offset = pos.x_offset as f32 * scale_factor;
                        let y_offset = pos.y_offset as f32 * scale_factor;
                        let x_adv = pos.x_advance as f32 * scale_factor;

                        all_glyphs.push(ShapedGlyph {
                            face_id,
                            glyph_id: info.glyph_id,
                            x: cursor_x + x_offset,
                            y: y_offset,
                            width: x_adv,
                            height: asc + desc,
                            x_advance: x_adv,
                            cluster: global_cluster_offset + info.cluster,
                        });

                        cursor_x += x_adv;
                    }
                }
            });
            global_cluster_offset += run_len;
        }

        if max_ascent == 0.0 {
            max_ascent = font_size * 0.8;
            max_descent = font_size * 0.2;
        }

        let result = ShapedText {
            glyphs: all_glyphs,
            width: cursor_x,
            height: max_ascent + max_descent + max_line_gap,
            ascent: max_ascent,
            descent: max_descent,
            line_gap: max_line_gap,
        };

        if let Ok(mut cache) = self.cache.lock() {
            cache.put(cache_key, result.clone());
        }

        result
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_shaper_empty_string() {
        let shaper = TextShaper::default_system_font().expect("Failed font load");
        let shaped = shaper.shape("", 16.0);
        assert_eq!(shaped.width, 0.0);
        assert!(shaped.glyphs.is_empty());
    }

    #[test]
    fn test_shaper_caching_and_advances() {
        let shaper = TextShaper::default_system_font().expect("Failed font load");
        let shaped1 = shaper.shape("Hello World!", 16.0);
        assert!(shaped1.width > 0.0);
        assert_eq!(shaped1.glyphs.len(), 12);

        let shaped2 = shaper.shape("Hello World!", 16.0);
        assert_eq!(shaped1.width, shaped2.width);
        assert_eq!(shaped1.glyphs.len(), shaped2.glyphs.len());
        for (g1, g2) in shaped1.glyphs.iter().zip(shaped2.glyphs.iter()) {
            assert_eq!(g1.glyph_id, g2.glyph_id);
            assert_eq!(g1.x, g2.x);
        }
    }

    #[test]
    fn test_shaper_kerning() {
        let shaper = TextShaper::default_system_font().expect("Failed font load");
        let a = shaper.shape("A", 24.0);
        let v = shaper.shape("V", 24.0);
        let av = shaper.shape("AV", 24.0);
        // With kerning, "AV" width is typically less than or equal to "A" width + "V" width
        assert!(av.width <= a.width + v.width + 0.001);
    }
}
