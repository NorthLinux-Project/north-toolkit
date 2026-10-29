pub mod font_db;
pub mod rasterizer;
pub mod shaper;

pub use font_db::FontDatabase;
pub use rasterizer::{GlyphRasterizer, RasterizedGlyph};
pub use shaper::{ShapedGlyph, ShapedText, TextShaper};

use crate::render::{GlyphAtlas, RenderPrimitive};

pub fn emit_text_glyphs(
    shaped: &ShapedText,
    logical_x: f32,
    logical_baseline: f32,
    font_size: f32,
    color: [f32; 4],
    scale: f32,
    atlas: &mut GlyphAtlas,
    rasterizer: &GlyphRasterizer,
    primitives: &mut Vec<RenderPrimitive>,
) {
    let physical_font_size = (font_size * scale).round();
    let baseline_y = (logical_baseline * scale).round();

    for glyph in &shaped.glyphs {
        let px_x = (logical_x + glyph.x) * scale;
        let subpx = (px_x * 4.0).round() as i32;
        let aligned_x = (subpx >> 2) as f32;
        let bucket = (subpx & 3) as u8;

        if let Ok(Some(region)) = atlas.get_or_insert(
            rasterizer,
            glyph.face_id,
            glyph.glyph_id,
            physical_font_size,
            bucket,
        ) {
            let screen_x = aligned_x + region.offset_x;
            let screen_y = baseline_y - (glyph.y * scale).round() + region.offset_y;

            primitives.push(RenderPrimitive::glyph(
                region.page,
                [screen_x, screen_y, region.width as f32, region.height as f32],
                region.uv_rect,
                color,
            ));
        }
    }
}
