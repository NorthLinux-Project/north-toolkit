//! HarfBuzz-grade OpenType text shaping, cluster tracking, kerning, and advance metrics via `rustybuzz`.

use std::sync::{Arc, Mutex};

use anyhow::Result;
use fontdb::ID;
use lru::LruCache;

use super::font_db::FontDatabase;

/// An individual shaped glyph with positioned offsets and advance width.
#[derive(Debug, Clone, PartialEq)]
pub struct ShapedGlyph {
    /// Font face containing this glyph.
    pub face_id: ID,
    /// OpenType glyph ID within the font face.
    pub glyph_id: u32,
    /// Logical horizontal offset from start of line in points.
    pub x: f32,
    /// Vertical offset from baseline in points.
    pub y: f32,
    /// Bounding width in points.
    pub width: f32,
    /// Bounding height in points.
    pub height: f32,
    /// Horizontal advance width to the next glyph.
    pub x_advance: f32,
    /// Character cluster index in the original UTF-8 string.
    pub cluster: u32,
}

/// A fully shaped line of text with global line height and bounding box metrics.
#[derive(Debug, Clone, PartialEq)]
pub struct ShapedText {
    /// Positioned glyphs in rendering order.
    pub glyphs: Vec<ShapedGlyph>,
    /// Total horizontal advance width.
    pub width: f32,
    /// Total line height (`ascent + descent + line_gap`).
    pub height: f32,
    /// Ascent distance above the baseline.
    pub ascent: f32,
    /// Descent distance below the baseline.
    pub descent: f32,
    /// Font designer recommended line gap.
    pub line_gap: f32,
}

impl ShapedText {
    /// Computes logical horizontal cursor positions for each character boundary `0..=char_count`.
    pub fn char_x_positions(&self, char_count: usize) -> Vec<f32> {
        let mut positions = vec![0.0f32; char_count + 1];
        if char_count == 0 || self.glyphs.is_empty() {
            return positions;
        }

        let monotonic = self.glyphs.windows(2).all(|w| w[0].cluster <= w[1].cluster);
        let groups: Vec<(usize, f32, f32)> = if monotonic {
            let mut list: Vec<(usize, f32, f32)> = Vec::new();
            let mut pen_x = 0.0f32;
            for g in &self.glyphs {
                let cluster = (g.cluster as usize).min(char_count.saturating_sub(1));
                let next_pen = pen_x + g.x_advance;
                if let Some(last) = list.last_mut() {
                    if last.0 == cluster {
                        last.2 = next_pen;
                        pen_x = next_pen;
                        continue;
                    }
                }
                list.push((cluster, pen_x, next_pen));
                pen_x = next_pen;
            }
            list
        } else {
            let mut spans = std::collections::BTreeMap::<usize, (f32, f32)>::new();
            let mut pen_x = 0.0f32;
            for g in &self.glyphs {
                let cluster = (g.cluster as usize).min(char_count.saturating_sub(1));
                let next_pen = pen_x + g.x_advance;
                spans
                    .entry(cluster)
                    .and_modify(|(s, e)| {
                        *s = s.min(pen_x);
                        *e = e.max(next_pen);
                    })
                    .or_insert((pen_x, next_pen));
                pen_x = next_pen;
            }
            spans.into_iter().map(|(c, (s, e))| (c, s, e)).collect()
        };

        for (idx, &(c_start, x_start, x_end)) in groups.iter().enumerate() {
            let c_end = groups
                .get(idx + 1)
                .map(|&(next_c, _, _)| next_c)
                .unwrap_or(char_count)
                .max(c_start + 1)
                .min(char_count);
            let span = c_end - c_start;
            for offset in 0..span {
                let frac = offset as f32 / span as f32;
                positions[c_start + offset] = x_start + (x_end - x_start) * frac;
            }
        }
        positions[char_count] = self.width;
        positions
    }

    /// Returns the logical horizontal offset for `char_idx` within a string of `char_count` characters.
    pub fn x_for_char(&self, char_idx: usize, char_count: usize) -> f32 {
        let positions = self.char_x_positions(char_count);
        positions
            .get(char_idx.min(char_count))
            .copied()
            .unwrap_or(self.width)
    }
}

/// LRU-cached text shaping engine using `rustybuzz`.
#[derive(Clone)]
pub struct TextShaper {
    font_db: FontDatabase,
    cache: Arc<Mutex<LruCache<(String, ID, u32), ShapedText>>>,
}

impl TextShaper {
    /// Creates a text shaper initialized with a font database and 1024-entry LRU cache.
    pub fn new(font_db: FontDatabase) -> Self {
        Self {
            font_db,
            cache: Arc::new(Mutex::new(LruCache::new(
                std::num::NonZeroUsize::new(1024).unwrap(),
            ))),
        }
    }

    /// Initializes a text shaper with default system fonts.
    pub fn default_system_font() -> Result<Self> {
        let font_db = FontDatabase::new()?;
        Ok(Self::new(font_db))
    }

    /// Returns a reference to the underlying font database.
    pub fn font_db(&self) -> &FontDatabase {
        &self.font_db
    }

    /// Shapes a text string at specified font size using the default system face.
    pub fn shape(&self, text: &str, font_size: f32) -> ShapedText {
        self.shape_with_face(text, font_size, self.font_db.default_face())
    }

    /// Shapes a text string with a preferred font face, falling back per-character as needed.
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
                    for (char_idx, ch) in run_text.chars().enumerate() {
                        buffer.add(ch, char_idx as u32);
                    }
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
        assert!(av.width <= a.width + v.width + 0.001);
    }

    #[test]
    fn test_shaper_multibyte_clusters_and_char_positions() {
        let shaper = TextShaper::default_system_font().expect("Failed font load");
        let sample = "é語a";
        let shaped = shaper.shape(sample, 16.0);
        assert_eq!(shaped.glyphs.len(), 3);
        assert_eq!(shaped.glyphs[0].cluster, 0);
        assert_eq!(shaped.glyphs[1].cluster, 1);
        assert_eq!(shaped.glyphs[2].cluster, 2);

        let positions = shaped.char_x_positions(3);
        assert_eq!(positions.len(), 4);
        assert_eq!(positions[0], 0.0);
        assert!(positions[1] > positions[0]);
        assert!(positions[2] > positions[1]);
        assert!((positions[3] - shaped.width).abs() < 0.001);

        let lig = shaper.shape("fine", 16.0);
        let lig_pos = lig.char_x_positions(4);
        assert_eq!(lig_pos.len(), 5);
        assert!(lig_pos[1] > lig_pos[0]);
        assert!(lig_pos[2] > lig_pos[1]);
        assert!((lig_pos[4] - lig.width).abs() < 0.001);
    }
}
