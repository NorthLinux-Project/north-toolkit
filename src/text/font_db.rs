use std::collections::HashMap;
use std::sync::{Arc, RwLock};

use anyhow::{Context, Result};
use fontdb::{Database, Family, ID, Query, Stretch, Style, Weight};
use swash::FontRef;

#[derive(Clone)]
pub struct FontDatabase {
    db: Arc<Database>,
    fallback_cache: Arc<RwLock<HashMap<char, Option<ID>>>>,
    default_sans: ID,
}

impl FontDatabase {
    pub fn new() -> Result<Self> {
        let mut db = Database::new();
        db.load_system_fonts();

        const CANDIDATES: &[&str] = &[
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        ];

        for path in CANDIDATES {
            if std::path::Path::new(path).exists() {
                db.load_font_file(path).ok();
            }
        }

        let query = Query {
            families: &[Family::SansSerif, Family::Name("DejaVu Sans"), Family::Name("Noto Sans")],
            weight: Weight::NORMAL,
            stretch: Stretch::Normal,
            style: Style::Normal,
        };

        let default_sans = db
            .query(&query)
            .or_else(|| db.faces().next().map(|f| f.id))
            .context("No fonts available in font database")?;

        Ok(Self {
            db: Arc::new(db),
            fallback_cache: Arc::new(RwLock::new(HashMap::new())),
            default_sans,
        })
    }

    pub fn default_face(&self) -> ID {
        self.default_sans
    }

    pub fn query_face(&self, family_name: &str, weight: u16, style: Style) -> ID {
        let query = Query {
            families: &[Family::Name(family_name), Family::SansSerif],
            weight: Weight(weight),
            stretch: Stretch::Normal,
            style,
        };
        self.db.query(&query).unwrap_or(self.default_sans)
    }

    pub fn face_for_char(&self, ch: char, preferred_face: ID) -> ID {
        if self.face_has_glyph(preferred_face, ch) {
            return preferred_face;
        }

        if let Ok(cache) = self.fallback_cache.read() {
            if let Some(&cached) = cache.get(&ch) {
                if let Some(id) = cached {
                    return id;
                }
            }
        }

        let found = self.db.faces().find_map(|face| {
            if self.face_has_glyph(face.id, ch) {
                Some(face.id)
            } else {
                None
            }
        });

        if let Ok(mut cache) = self.fallback_cache.write() {
            cache.insert(ch, found);
        }

        found.unwrap_or(preferred_face)
    }

    pub fn face_has_glyph(&self, face_id: ID, ch: char) -> bool {
        self.db
            .with_face_data(face_id, |data, index| {
                FontRef::from_index(data, index as usize)
                    .map(|f| f.charmap().map(ch) != 0)
                    .unwrap_or(false)
            })
            .unwrap_or(false)
    }

    pub fn with_face_data<R, F: FnOnce(&[u8], u32) -> R>(&self, face_id: ID, f: F) -> Option<R> {
        self.db.with_face_data(face_id, f)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_font_db_initialization_and_default_face() {
        let db = FontDatabase::new().expect("FontDatabase initialization failed");
        let default_face = db.default_face();
        assert!(db.face_has_glyph(default_face, 'A'));
    }

    #[test]
    fn test_font_db_fallback_lookup() {
        let db = FontDatabase::new().expect("FontDatabase initialization failed");
        let default_face = db.default_face();
        let ascii_face = db.face_for_char('x', default_face);
        assert_eq!(ascii_face, default_face);

        let fallback_face = db.face_for_char('語', default_face);
        assert!(db.face_has_glyph(fallback_face, '語') || fallback_face == default_face);

        let cached_face = db.face_for_char('x', default_face);
        assert_eq!(cached_face, ascii_face);
    }
}
