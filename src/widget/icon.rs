//! Vector-rendered scalable system icons with dynamic caching in the vector texture atlas.

use std::any::Any;

use super::tooltip::Tooltip;
use super::tree::Widget;
use crate::layout::{Dimension, FlexStyle, LayoutRect, MeasureFunc};
use crate::render::primitives::CornerRadii;
use crate::render::vector::{StrokeStyle, VectorAtlas, VectorPath, VectorRasterizer};
use crate::render::{GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// Standard predefined system icon identifiers.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum IconName {
    /// Chevron arrow pointing downwards.
    ChevronDown,
    /// Chevron arrow pointing upwards.
    ChevronUp,
    /// Chevron arrow pointing leftwards.
    ChevronLeft,
    /// Chevron arrow pointing rightwards.
    ChevronRight,
    /// Checkmark indicator.
    Check,
    /// Close cross symbol.
    Close,
    /// Search magnifying glass.
    Search,
    /// Settings cog gear.
    Settings,
    /// Plus symbol.
    Plus,
    /// Minus symbol.
    Minus,
    /// Circular refresh/reload arrow.
    Refresh,
    /// Folder directory icon.
    Folder,
    /// File document icon.
    File,
    /// Visible eye icon.
    Eye,
    /// Obscured eye icon with slash.
    EyeOff,
    /// Audio speaker volume icon.
    Volume,
    /// Audio speaker muted icon.
    VolumeMute,
    /// Window maximize square icon.
    WindowMaximize,
    /// Window restore overlapping squares icon.
    WindowRestore,
    /// Window minimize horizontal line icon.
    WindowMinimize,
}

impl IconName {
    /// Returns the SVG path data representation for this icon.
    pub fn svg_path(&self) -> &'static str {
        match self {
            Self::ChevronDown => "M6 9l6 6 6-6",
            Self::ChevronUp => "M18 15l-6-6-6 6",
            Self::ChevronLeft => "M15 18l-6-6 6-6",
            Self::ChevronRight => "M9 18l6-6-6-6",
            Self::Check => "M20 6L9 17l-5-5",
            Self::Close => "M18 6L6 18M6 6l12 12",
            Self::Search => "M21 21l-4.35-4.35M11 19a8 8 0 100-16 8 8 0 000 16z",
            Self::Settings => "M12 15a3 3 0 100-6 3 3 0 000 6zM19.4 15a1.65 1.65 0 00.33 1.82l.06.06a2 2 0 010 2.83 2 2 0 01-2.83 0l-.06-.06a1.65 1.65 0 00-1.82-.33 1.65 1.65 0 00-1 1.51V21a2 2 0 01-2 2 2 2 0 01-2-2v-.09A1.65 1.65 0 009 19.4a1.65 1.65 0 00-1.82.33l-.06.06a2 2 0 01-2.83 0 2 2 0 010-2.83l.06-.06a1.65 1.65 0 00.33-1.82 1.65 1.65 0 00-1.51-1H3a2 2 0 01-2-2 2 2 0 012-2h.09A1.65 1.65 0 004.6 9a1.65 1.65 0 00-.33-1.82l-.06-.06a2 2 0 010-2.83 2 2 0 012.83 0l.06.06a1.65 1.65 0 001.82.33H9a1.65 1.65 0 001-1.51V3a2 2 0 012-2 2 2 0 012 2v.09a1.65 1.65 0 001 1.51 1.65 1.65 0 001.82-.33l.06-.06a2 2 0 012.83 0 2 2 0 010 2.83l-.06.06a1.65 1.65 0 00-.33 1.82V9a1.65 1.65 0 001.51 1H21a2 2 0 012 2 2 2 0 01-2 2h-.09a1.65 1.65 0 00-1.51 1z",
            Self::Plus => "M12 5v14M5 12h14",
            Self::Minus => "M5 12h14",
            Self::Refresh => "M23 4v6h-6M1 20v-6h6M20.49 9A9 9 0 005.64 5.64L1 10m22 4l-4.64 4.36A9 9 0 013.51 15",
            Self::Folder => "M22 19a2 2 0 01-2 2H4a2 2 0 01-2-2V5a2 2 0 012-2h5l2 3h9a2 2 0 012 2z",
            Self::File => "M13 2H6a2 2 0 00-2 2v16a2 2 0 002 2h12a2 2 0 002-2V9zM13 2v7h7",
            Self::Eye => "M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8zM12 15a3 3 0 100-6 3 3 0 000 6z",
            Self::EyeOff => "M17.94 17.94A10.07 10.07 0 0112 20c-7 0-11-8-11-8a18.45 18.45 0 015.06-5.94M9.9 4.24A9.12 9.12 0 0112 4c7 0 11 8 11 8a18.5 18.5 0 01-2.16 3.19m-6.72-1.07a3 3 0 11-4.24-4.24M1 1l22 22",
            Self::Volume => "M11 5L6 9H2v6h4l5 4V5zM19.07 4.93a10 10 0 010 14.14M15.54 8.46a5 5 0 010 7.07",
            Self::VolumeMute => "M11 5L6 9H2v6h4l5 4V5zM23 9l-6 6M17 9l6 6",
            Self::WindowMaximize => "M4 4h16v16H4z",
            Self::WindowRestore => "M4 8h12v12H4z M8 4h12v12",
            Self::WindowMinimize => "M4 12h16",
        }
    }
}

/// Scalable vector icon widget.
pub struct Icon {
    /// Identifier of the icon glyph.
    pub name: IconName,
    /// Logical size of the icon in points.
    pub size: f32,
    /// Optional color override (defaults to theme text color).
    pub color: Option<[f32; 4]>,
    /// Stroke width for vector path outline.
    pub stroke_width: f32,
    /// Flexbox layout style.
    pub style: FlexStyle,
    /// Optional hover tooltip.
    pub tooltip: Option<Tooltip>,
}

impl Icon {
    /// Creates a new icon widget with default size and stroke width.
    pub fn new(name: IconName) -> Self {
        Self {
            name,
            size: 18.0,
            color: None,
            stroke_width: 2.0,
            style: FlexStyle {
                width: Dimension::Points(18.0),
                height: Dimension::Points(18.0),
                ..Default::default()
            },
            tooltip: None,
        }
    }

    /// Sets the width and height of the icon in logical points.
    pub fn with_size(mut self, size: f32) -> Self {
        self.size = size;
        self.style.width = Dimension::Points(size);
        self.style.height = Dimension::Points(size);
        self
    }

    /// Overrides the icon tint color.
    pub fn with_color(mut self, color: [f32; 4]) -> Self {
        self.color = Some(color);
        self
    }

    /// Sets the vector outline stroke width.
    pub fn with_stroke_width(mut self, width: f32) -> Self {
        self.stroke_width = width;
        self
    }

    /// Attaches a hover tooltip to the icon.
    pub fn with_tooltip<S: Into<String>>(mut self, text: S) -> Self {
        self.tooltip = Some(Tooltip::new(text));
        self
    }
}

impl Widget for Icon {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn measure(&self, _shaper: &TextShaper) -> Option<MeasureFunc> {
        let s = self.size;
        Some(Box::new(move |_avail_w, _avail_h| (s, s)))
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
        let draw_color = self.color.unwrap_or(theme.colors.text);
        let phys_w = ((rect.width * scale).round() as u32).max(1);
        let phys_h = ((rect.height * scale).round() as u32).max(1);

        let icon_key = self.name as u64;
        let d = self.name.svg_path();
        let stroke_w = (self.stroke_width * (phys_w as f32 / 24.0)).max(1.2);

        let uv_rect = vector_atlas.get_or_insert(icon_key, phys_w, phys_h, || {
            let path = VectorPath::parse_svg(d).with_view_box(0.0, 0.0, 24.0, 24.0);
            VectorRasterizer::rasterize(
                &path,
                phys_w,
                phys_h,
                None,
                Some(StrokeStyle {
                    color: [1.0, 1.0, 1.0, 1.0],
                    width: stroke_w,
                    ..Default::default()
                }),
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
                color: draw_color,
                radii: CornerRadii::all(0.0),
                border: None,
                uv_rect: uv,
                use_texture: true,
            }]
        } else {
            Vec::new()
        }
    }

    fn tooltip(&self) -> Option<Tooltip> {
        self.tooltip.clone()
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
    fn test_icon_widget_measure_and_paint() {
        let icon = Icon::new(IconName::Search).with_size(20.0);
        let style = icon.layout_style();
        assert_eq!(style.width, Dimension::Points(20.0));
        assert_eq!(style.height, Dimension::Points(20.0));

        let mut atlas = VectorAtlas::new(128, 128);
        let mut glyph_atlas = GlyphAtlas::new(128, 128, 1);
        let shaper = TextShaper::default_system_font().unwrap();
        let rasterizer = GlyphRasterizer::new(shaper.font_db().clone());
        let theme = Theme::dark();

        let prims = icon.paint(
            LayoutRect::new(10.0, 10.0, 20.0, 20.0),
            &theme,
            &mut glyph_atlas,
            &rasterizer,
            &shaper,
            &mut atlas,
            1.0,
        );

        assert_eq!(prims.len(), 1);
        match &prims[0] {
            RenderPrimitive::Rect { use_texture, uv_rect, .. } => {
                assert!(use_texture);
                assert!(uv_rect[2] > 0.0);
                assert!(uv_rect[3] > 0.0);
            }
            _ => panic!("Expected textured rect primitive for icon"),
        }
    }
}
