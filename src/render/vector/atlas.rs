//! Dynamic 2D bin-packing texture atlas for vector paths, SVG icons, and curves.

use std::collections::HashMap;

/// Horizontal shelf for packing vector icons into the atlas.
pub struct VectorShelf {
    /// Vertical pixel offset of this shelf.
    pub y: u32,
    /// Height allocated to this shelf.
    pub height: u32,
    /// Current rightward allocation cursor.
    pub current_x: u32,
}

/// Dynamic RGBA8 texture atlas caching rasterized vector icons and SVG paths.
pub struct VectorAtlas {
    /// Total width of the texture atlas in pixels.
    pub width: u32,
    /// Total height of the texture atlas in pixels.
    pub height: u32,
    /// Underlying RGBA8 pixel storage.
    pub data: Vec<u8>,
    /// Active shelves for 2D bin-packing.
    pub shelves: Vec<VectorShelf>,
    /// Cache mapping `(icon_hash, phys_w, phys_h)` to normalized UV coordinates.
    pub cache: HashMap<(u64, u32, u32), [f32; 4]>,
    /// Dirty bounding rectangles needing upload to GPU texture.
    pub dirty_rects: Vec<[u32; 4]>,
}

impl VectorAtlas {
    /// Creates a new vector atlas of specified dimensions.
    pub fn new(width: u32, height: u32) -> Self {
        let size = (width * height * 4) as usize;
        let mut data = vec![0u8; size];
        // Reserve pixel (0, 0) as solid white for fallback / default quad sampling
        data[0] = 255;
        data[1] = 255;
        data[2] = 255;
        data[3] = 255;

        let mut shelves = Vec::new();
        // Shelf 0 reserves (0, 0..2)
        shelves.push(VectorShelf {
            y: 0,
            height: 4,
            current_x: 4,
        });

        Self {
            width,
            height,
            data,
            shelves,
            cache: HashMap::new(),
            dirty_rects: vec![[0, 0, 1, 1]],
        }
    }

    /// Looks up a cached vector icon or invokes `rasterize_fn` and packs it into the atlas.
    pub fn get_or_insert<F>(&mut self, key: u64, phys_w: u32, phys_h: u32, rasterize_fn: F) -> Option<[f32; 4]>
    where
        F: FnOnce() -> Option<Vec<u8>>,
    {
        let cache_key = (key, phys_w, phys_h);
        if let Some(&uv) = self.cache.get(&cache_key) {
            return Some(uv);
        }

        let rgba_pixels = rasterize_fn()?;
        if rgba_pixels.len() != (phys_w * phys_h * 4) as usize {
            return None;
        }

        let (alloc_x, alloc_y) = self.allocate(phys_w, phys_h)?;

        // Copy pixel rows into atlas buffer
        for row in 0..phys_h {
            let src_start = (row * phys_w * 4) as usize;
            let src_end = src_start + (phys_w * 4) as usize;
            let dst_start = (((alloc_y + row) * self.width + alloc_x) * 4) as usize;
            let dst_end = dst_start + (phys_w * 4) as usize;

            if dst_end <= self.data.len() && src_end <= rgba_pixels.len() {
                self.data[dst_start..dst_end].copy_from_slice(&rgba_pixels[src_start..src_end]);
            }
        }

        self.dirty_rects.push([alloc_x, alloc_y, phys_w, phys_h]);

        let uv_x1 = alloc_x as f32 / self.width as f32;
        let uv_y1 = alloc_y as f32 / self.height as f32;
        let uv_x2 = (alloc_x + phys_w) as f32 / self.width as f32;
        let uv_y2 = (alloc_y + phys_h) as f32 / self.height as f32;
        let uv_rect = [uv_x1, uv_y1, uv_x2, uv_y2];

        self.cache.insert(cache_key, uv_rect);
        Some(uv_rect)
    }

    fn allocate(&mut self, w: u32, h: u32) -> Option<(u32, u32)> {
        let pad = 2u32;
        let padded_w = w + pad;
        let padded_h = h + pad;

        if padded_w > self.width || padded_h > self.height {
            return None;
        }

        for shelf in &mut self.shelves {
            if shelf.height >= padded_h && shelf.current_x + padded_w <= self.width {
                let x = shelf.current_x;
                let y = shelf.y;
                shelf.current_x += padded_w;
                return Some((x, y));
            }
        }

        let next_y = self.shelves.last().map(|s| s.y + s.height).unwrap_or(0);
        if next_y + padded_h <= self.height {
            let shelf = VectorShelf {
                y: next_y,
                height: padded_h.max(32),
                current_x: padded_w,
            };
            let y = shelf.y;
            self.shelves.push(shelf);
            return Some((0, y));
        }

        None
    }

    /// Returns `true` if there are dirty rects awaiting upload to GPU memory.
    pub fn is_dirty(&self) -> bool {
        !self.dirty_rects.is_empty()
    }

    /// Takes and drains all pending dirty rects.
    pub fn take_dirty_rects(&mut self) -> Vec<[u32; 4]> {
        std::mem::take(&mut self.dirty_rects)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_vector_atlas_allocation_and_caching() {
        let mut atlas = VectorAtlas::new(256, 256);
        // Initial solid white pixel is dirty
        assert!(atlas.is_dirty());
        let initial_dirty = atlas.take_dirty_rects();
        assert_eq!(initial_dirty.len(), 1);
        assert!(!atlas.is_dirty());

        let key = 42u64;
        let uv1 = atlas.get_or_insert(key, 16, 16, || Some(vec![255u8; 16 * 16 * 4]));
        assert!(uv1.is_some());
        let uv1_rect = uv1.unwrap();
        assert!(uv1_rect[2] > uv1_rect[0]);
        assert!(uv1_rect[3] > uv1_rect[1]);
        assert!(atlas.is_dirty());

        let dirty = atlas.take_dirty_rects();
        assert_eq!(dirty.len(), 1);
        assert!(!atlas.is_dirty());

        // Cache hit
        let uv2 = atlas.get_or_insert(key, 16, 16, || panic!("Should not rasterize again"));
        assert_eq!(uv1, uv2);
    }
}
