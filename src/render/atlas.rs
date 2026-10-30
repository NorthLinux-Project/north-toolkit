//! Dynamic 2D bin-packing glyph cache atlas with multi-page LRU eviction.

use std::collections::HashMap;

use anyhow::{bail, Result};
use fontdb::ID;

use crate::text::rasterizer::GlyphRasterizer;

/// Coordinates and metrics of a cached glyph within a texture atlas page.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct AtlasRegion {
    /// Texture page index where glyph is stored.
    pub page: u8,
    /// Pixel X coordinate on page.
    pub x: u32,
    /// Pixel Y coordinate on page.
    pub y: u32,
    /// Pixel width of cached glyph bitmap.
    pub width: u32,
    /// Pixel height of cached glyph bitmap.
    pub height: u32,
    /// Normalized UV coordinates `[u_min, v_min, u_max, v_max]`.
    pub uv_rect: [f32; 4],
    /// Horizontal offset from origin in pixels.
    pub offset_x: f32,
    /// Vertical offset from origin in pixels.
    pub offset_y: f32,
}

/// Cache key uniquely identifying a shaped, sized, and subpixel-bucketed glyph.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct GlyphKey {
    /// Font face identifier.
    pub face_id: ID,
    /// OpenType glyph ID.
    pub glyph_id: u32,
    /// Floating point font size in bits for exact matching.
    pub size_bits: u32,
    /// Subpixel horizontal offset bucket (0..3).
    pub subpixel_bucket: u8,
}

struct Shelf {
    y: u32,
    height: u32,
    current_x: u32,
}

/// A single 2D texture allocation page holding packed glyphs.
pub struct AtlasPage {
    /// Single-channel 8-bit alpha pixel buffer.
    pub data: Vec<u8>,
    shelves: Vec<Shelf>,
    dirty_rect: Option<[u32; 4]>,
}

impl AtlasPage {
    fn new(width: u32, height: u32) -> Self {
        Self {
            data: vec![0u8; (width * height) as usize],
            shelves: Vec::new(),
            dirty_rect: None,
        }
    }

    fn clear(&mut self) {
        self.data.fill(0);
        self.shelves.clear();
        self.dirty_rect = Some([0, 0, 0, 0]);
    }
}

/// Dynamic multi-page texture atlas manager for rasterized font glyphs.
pub struct GlyphAtlas {
    width: u32,
    height: u32,
    max_pages: usize,
    pages: Vec<AtlasPage>,
    cache: HashMap<GlyphKey, AtlasRegion>,
    oldest_page_index: usize,
    dirty: bool,
}

impl GlyphAtlas {
    /// Creates a new glyph atlas with initial dimensions and maximum page capacity.
    pub fn new(width: u32, height: u32, max_pages: usize) -> Self {
        let mut initial_pages = Vec::with_capacity(max_pages.max(1));
        initial_pages.push(AtlasPage::new(width, height));

        Self {
            width,
            height,
            max_pages: max_pages.max(1),
            pages: initial_pages,
            cache: HashMap::new(),
            oldest_page_index: 0,
            dirty: false,
        }
    }

    /// Returns `(width, height)` of an atlas page in pixels.
    pub fn dimensions(&self) -> (u32, u32) {
        (self.width, self.height)
    }

    /// Returns the number of allocated pages.
    pub fn page_count(&self) -> usize {
        self.pages.len()
    }

    /// Returns a reference to the raw pixel data for a page.
    pub fn page_data(&self, page: u8) -> &[u8] {
        &self.pages[page as usize].data
    }

    /// Returns `true` if new glyphs were inserted since last GPU texture sync.
    pub fn is_dirty(&self) -> bool {
        self.dirty
    }

    /// Clears the atlas dirty flag.
    pub fn mark_clean(&mut self) {
        self.dirty = false;
        for page in &mut self.pages {
            page.dirty_rect = None;
        }
    }

    /// Returns and resets all dirty bounding rectangles across pages.
    pub fn take_dirty_rects(&mut self) -> Vec<(u8, [u32; 4])> {
        let mut rects = Vec::new();
        for (idx, page) in self.pages.iter_mut().enumerate() {
            if let Some(r) = page.dirty_rect.take() {
                rects.push((idx as u8, r));
            }
        }
        self.dirty = false;
        rects
    }

    /// Looks up a cached glyph region or rasterizes and packs it into the atlas.
    pub fn get_or_insert(
        &mut self,
        rasterizer: &GlyphRasterizer,
        face_id: ID,
        glyph_id: u32,
        physical_size: f32,
        subpixel_bucket: u8,
    ) -> Result<Option<AtlasRegion>> {
        let key = GlyphKey {
            face_id,
            glyph_id,
            size_bits: physical_size.to_bits(),
            subpixel_bucket,
        };

        if let Some(&region) = self.cache.get(&key) {
            return Ok(Some(region));
        }

        let rasterized = match rasterizer.rasterize(face_id, glyph_id, physical_size, subpixel_bucket)? {
            Some(r) => r,
            None => return Ok(None),
        };

        let gw = rasterized.width;
        let gh = rasterized.height;
        let padding = 1u32;

        let alloc_w = gw + padding * 2;
        let alloc_h = gh + padding * 2;

        if alloc_w > self.width || alloc_h > self.height {
            bail!("Glyph exceeds atlas dimensions");
        }

        let mut placement: Option<(u8, u32, u32)> = None;

        for (page_idx, page) in self.pages.iter_mut().enumerate() {
            for shelf in &mut page.shelves {
                if shelf.height >= alloc_h && shelf.current_x + alloc_w <= self.width {
                    placement = Some((page_idx as u8, shelf.current_x, shelf.y));
                    shelf.current_x += alloc_w;
                    break;
                }
            }
            if placement.is_some() {
                break;
            }

            let next_y = page
                .shelves
                .last()
                .map(|s| s.y + s.height)
                .unwrap_or(0);

            if next_y + alloc_h <= self.height {
                let new_shelf_h = alloc_h.max(32);
                page.shelves.push(Shelf {
                    y: next_y,
                    height: new_shelf_h,
                    current_x: alloc_w,
                });
                placement = Some((page_idx as u8, 0, next_y));
                break;
            }
        }

        if placement.is_none() && self.pages.len() < self.max_pages {
            let new_page_idx = self.pages.len() as u8;
            let mut new_page = AtlasPage::new(self.width, self.height);
            let new_shelf_h = alloc_h.max(32);
            new_page.shelves.push(Shelf {
                y: 0,
                height: new_shelf_h,
                current_x: alloc_w,
            });
            self.pages.push(new_page);
            placement = Some((new_page_idx, 0, 0));
        }

        if placement.is_none() {
            let evict_page = self.oldest_page_index as u8;
            self.oldest_page_index = (self.oldest_page_index + 1) % self.pages.len();

            self.cache.retain(|_, region| region.page != evict_page);

            let page = &mut self.pages[evict_page as usize];
            page.clear();
            let new_shelf_h = alloc_h.max(32);
            page.shelves.push(Shelf {
                y: 0,
                height: new_shelf_h,
                current_x: alloc_w,
            });
            placement = Some((evict_page, 0, 0));
        }

        let (page_idx, px, py) = placement.unwrap();
        let glyph_x = px + padding;
        let glyph_y = py + padding;

        let page = &mut self.pages[page_idx as usize];
        for row in 0..gh {
            let src_start = (row * gw) as usize;
            let src_end = src_start + gw as usize;
            let dst_start = ((glyph_y + row) * self.width + glyph_x) as usize;
            let dst_end = dst_start + gw as usize;
            page.data[dst_start..dst_end].copy_from_slice(&rasterized.pixels[src_start..src_end]);
        }

        let cur_dirty = page.dirty_rect.unwrap_or([px, py, alloc_w, alloc_h]);
        let min_x = cur_dirty[0].min(px);
        let min_y = cur_dirty[1].min(py);
        let max_x = (cur_dirty[0] + cur_dirty[2]).max(px + alloc_w);
        let max_y = (cur_dirty[1] + cur_dirty[3]).max(py + alloc_h);
        page.dirty_rect = Some([min_x, min_y, max_x - min_x, max_y - min_y]);
        self.dirty = true;

        let uv_rect = [
            glyph_x as f32 / self.width as f32,
            glyph_y as f32 / self.height as f32,
            (glyph_x + gw) as f32 / self.width as f32,
            (glyph_y + gh) as f32 / self.height as f32,
        ];

        let region = AtlasRegion {
            page: page_idx,
            x: glyph_x,
            y: glyph_y,
            width: gw,
            height: gh,
            uv_rect,
            offset_x: rasterized.offset_x,
            offset_y: rasterized.offset_y,
        };

        self.cache.insert(key, region);
        Ok(Some(region))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::text::font_db::FontDatabase;
    use swash::FontRef;

    #[test]
    fn test_atlas_insert_and_dirty_rect() {
        let font_db = FontDatabase::new().expect("Failed font db");
        let rasterizer = GlyphRasterizer::new(font_db.clone());
        let default_face = font_db.default_face();

        let glyph_id = font_db
            .with_face_data(default_face, |data, index| {
                FontRef::from_index(data, index as usize).map(|f| f.charmap().map('A'))
            })
            .flatten()
            .unwrap_or(0);

        assert!(glyph_id > 0);
        let gid = glyph_id as u32;

        let mut atlas = GlyphAtlas::new(256, 256, 2);
        assert!(!atlas.is_dirty());

        let r1 = atlas
            .get_or_insert(&rasterizer, default_face, gid, 16.0, 0)
            .expect("Atlas insert failed")
            .expect("Region empty");

        assert!(atlas.is_dirty());
        assert_eq!(r1.page, 0);
        assert!(r1.width > 0 && r1.height > 0);

        let dirty = atlas.take_dirty_rects();
        assert!(!dirty.is_empty());
        assert!(!atlas.is_dirty());

        // Cache hit
        let r2 = atlas
            .get_or_insert(&rasterizer, default_face, gid, 16.0, 0)
            .expect("Atlas cache failed")
            .expect("Region empty");

        assert_eq!(r1, r2);
        assert!(!atlas.is_dirty());
    }

    #[test]
    fn test_atlas_page_eviction() {
        let font_db = FontDatabase::new().expect("Failed font db");
        let rasterizer = GlyphRasterizer::new(font_db.clone());
        let default_face = font_db.default_face();

        // Very small atlas with max_pages = 2
        let mut atlas = GlyphAtlas::new(48, 48, 2);

        let chars = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'];
        for ch in chars {
            let glyph_id = font_db
                .with_face_data(default_face, |data, index| {
                    FontRef::from_index(data, index as usize).map(|f| f.charmap().map(ch))
                })
                .flatten()
                .unwrap_or(0);

            if glyph_id > 0 {
                let res = atlas.get_or_insert(&rasterizer, default_face, glyph_id as u32, 20.0, 0);
                assert!(res.is_ok(), "Atlas insert should not fail even when evicting");
            }
        }
    }
}
