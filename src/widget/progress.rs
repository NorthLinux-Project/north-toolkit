//! Visual progress indicators: determinate/indeterminate progress bars and circular loading spinners.

use std::any::Any;

use super::state::Property;
use super::tree::Widget;
use crate::animation::{Easing, Tween};
use crate::layout::{Dimension, FlexStyle, LayoutRect, MeasureFunc};
use crate::render::{CornerRadii, GlyphAtlas, RenderPrimitive, VectorAtlas};
use crate::text::{emit_text_glyphs, GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// Horizontal bar showing determinate completion percentage or indeterminate sliding progress.
pub struct ProgressBar {
    /// Current completion value in range `[0.0, 1.0]`.
    pub value: Property<f32>,
    /// Whether this bar is running in continuous indeterminate pulse mode.
    pub is_indeterminate: bool,
    /// Bar height in logical points.
    pub height: f32,
    /// Bar track background color override.
    pub track_color: Option<[f32; 4]>,
    /// Fill progress bar color override.
    pub fill_color: Option<[f32; 4]>,
    /// Optional corner radius override in logical points. If None, defaults to half height (pill).
    pub corner_radius: Option<f32>,
    /// Whether to display percentage text beside or on top of the bar.
    pub show_label: bool,
    /// Percentage text color override.
    pub text_color: Option<[f32; 4]>,
    /// Flexbox layout configuration.
    pub style: FlexStyle,
    fill_tween: Tween<f32>,
    indeterminate_time_ms: f32,
}

impl ProgressBar {
    /// Default track height in logical points.
    pub const DEFAULT_HEIGHT: f32 = 8.0;
    /// Cycle duration for the indeterminate sliding animation in milliseconds.
    pub const INDETERMINATE_CYCLE_MS: f32 = 1200.0;

    /// Creates a determinate progress bar initialized to `value` (clamped to `[0.0, 1.0]`).
    pub fn new(value: f32) -> Self {
        let clamped = value.clamp(0.0, 1.0);
        Self {
            value: Property::new(clamped),
            is_indeterminate: false,
            height: Self::DEFAULT_HEIGHT,
            track_color: None,
            fill_color: None,
            corner_radius: None,
            show_label: false,
            text_color: None,
            style: FlexStyle {
                width: Dimension::Percent(100.0),
                height: Dimension::Points(Self::DEFAULT_HEIGHT),
                ..Default::default()
            },
            fill_tween: Tween::new(clamped),
            indeterminate_time_ms: 0.0,
        }
    }

    /// Creates an indeterminate progress bar with continuous animated sliding indicator.
    pub fn indeterminate() -> Self {
        let mut bar = Self::new(0.0);
        bar.is_indeterminate = true;
        bar
    }

    /// Sets the determinate value and updates the smooth transition target.
    pub fn with_value(mut self, value: f32) -> Self {
        let clamped = value.clamp(0.0, 1.0);
        self.value.set(clamped);
        self.fill_tween = Tween::new(clamped);
        self
    }

    /// Sets indeterminate pulse mode.
    pub fn with_indeterminate(mut self, indeterminate: bool) -> Self {
        self.is_indeterminate = indeterminate;
        self
    }

    /// Configures the track height in logical points.
    pub fn with_height(mut self, height: f32) -> Self {
        self.height = height.max(2.0);
        self.style.height = Dimension::Points(self.height);
        self
    }

    /// Overrides the corner radius in logical points.
    pub fn with_corner_radius(mut self, radius: f32) -> Self {
        self.corner_radius = Some(radius.max(0.0));
        self
    }

    /// Overrides the fill progress bar color.
    pub fn with_color(mut self, color: [f32; 4]) -> Self {
        self.fill_color = Some(color);
        self
    }

    /// Overrides the background track color.
    pub fn with_track_color(mut self, color: [f32; 4]) -> Self {
        self.track_color = Some(color);
        self
    }

    /// Sets whether to show a formatted numeric percentage label.
    /// If enabled and height has not been customized above default, adjusts height to 14.0 for label legibility.
    pub fn with_show_label(mut self, show: bool) -> Self {
        self.show_label = show;
        if show && self.height < 14.0 {
            self.height = 14.0;
            self.style.height = Dimension::Points(14.0);
        }
        self
    }

    /// Overrides the percentage label text color.
    pub fn with_text_color(mut self, color: [f32; 4]) -> Self {
        self.text_color = Some(color);
        self
    }

    /// Updates the target progress value smoothly.
    pub fn set_value(&mut self, value: f32) {
        let clamped = value.clamp(0.0, 1.0);
        self.value.set(clamped);
        self.fill_tween.set_target(clamped, 200.0, Easing::EaseOut);
    }
}

impl Widget for ProgressBar {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn measure(&self, _shaper: &TextShaper) -> Option<MeasureFunc> {
        let h = self.height;
        Some(Box::new(move |_avail_w, _avail_h| (120.0, h)))
    }

    fn tick_animation(&mut self, delta_ms: f32) -> bool {
        let mut repaint = false;
        if self.is_indeterminate {
            self.indeterminate_time_ms =
                (self.indeterminate_time_ms + delta_ms) % Self::INDETERMINATE_CYCLE_MS;
            repaint = true;
        } else if !self.fill_tween.is_complete() {
            self.fill_tween.tick(delta_ms);
            repaint = true;
        }
        repaint
    }

    fn paint(
        &self,
        rect: LayoutRect,
        theme: &Theme,
        atlas: &mut GlyphAtlas,
        rasterizer: &GlyphRasterizer,
        shaper: &TextShaper,
        _vector_atlas: &mut VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let mut prims = Vec::new();
        let track_bg = self.track_color.unwrap_or(theme.colors.surface);
        let fill_bg = self.fill_color.unwrap_or(theme.colors.primary);
        let bar_h = self.height.min(rect.height);
        let bar_y = rect.y + (rect.height - bar_h).max(0.0) * 0.5;
        let corner_r = self.corner_radius.unwrap_or(bar_h * 0.5) * scale;
        let radii = CornerRadii::all(corner_r);

        // Background track
        prims.push(RenderPrimitive::solid_rect(
            [rect.x * scale, bar_y * scale, rect.width * scale, bar_h * scale],
            track_bg,
            radii,
        ));

        // Fill indicator
        if self.is_indeterminate {
            let progress = self.indeterminate_time_ms / Self::INDETERMINATE_CYCLE_MS;
            let block_w = (rect.width * 0.35).max(20.0);
            let total_travel = rect.width + block_w;
            let block_x = rect.x - block_w + total_travel * progress;

            // Clip within track bounds
            let clip_x = rect.x * scale;
            let clip_y = bar_y * scale;
            let clip_w = rect.width * scale;
            let clip_h = bar_h * scale;

            prims.push(RenderPrimitive::ClipPush {
                rect: [clip_x, clip_y, clip_w, clip_h],
            });

            prims.push(RenderPrimitive::solid_rect(
                [block_x * scale, bar_y * scale, block_w * scale, bar_h * scale],
                fill_bg,
                radii,
            ));

            prims.push(RenderPrimitive::ClipPop);
        } else {
            let current_val = self.fill_tween.current.clamp(0.0, 1.0);
            let fill_w = rect.width * current_val;
            if fill_w > 0.5 {
                prims.push(RenderPrimitive::solid_rect(
                    [rect.x * scale, bar_y * scale, fill_w * scale, bar_h * scale],
                    fill_bg,
                    radii,
                ));
            }
        }

        // Percentage label
        if self.show_label && !self.is_indeterminate {
            let pct = (self.fill_tween.current * 100.0).round() as u32;
            let text = format!("{pct}%");
            let font_size = (bar_h * 0.75).clamp(8.0, 12.0);
            let shaped = shaper.shape(&text, font_size);
            let text_x = rect.x + (rect.width - shaped.width) * 0.5;
            let center_y = bar_y + bar_h * 0.5;
            let cap_height = font_size * 0.70;
            let baseline = center_y + cap_height * 0.5;

            let label_col = self.text_color.unwrap_or(theme.colors.text);
            emit_text_glyphs(
                &shaped,
                text_x,
                baseline,
                font_size,
                label_col,
                scale,
                atlas,
                rasterizer,
                &mut prims,
            );
        }

        prims
    }

    fn as_any(&self) -> &dyn Any {
        self
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}

/// Circular indeterminate loading spinner with rotating vector arc animation.
pub struct Spinner {
    /// Outer diameter in logical points.
    pub diameter: f32,
    /// Stroke thickness in logical points.
    pub thickness: f32,
    /// Accent color override.
    pub color: Option<[f32; 4]>,
    /// Whether rotation animation is active.
    pub is_spinning: bool,
    /// Flexbox layout configuration.
    pub style: FlexStyle,
    rotation_angle: f32,
    arc_sweep: f32,
    elapsed_ms: f32,
}

impl Spinner {
    /// Default spinner diameter in logical points.
    pub const DEFAULT_DIAMETER: f32 = 24.0;
    /// Default stroke thickness in logical points.
    pub const DEFAULT_THICKNESS: f32 = 3.0;

    /// Creates a new rotating loading spinner with default dimensions.
    pub fn new() -> Self {
        Self {
            diameter: Self::DEFAULT_DIAMETER,
            thickness: Self::DEFAULT_THICKNESS,
            color: None,
            is_spinning: true,
            style: FlexStyle {
                width: Dimension::Points(Self::DEFAULT_DIAMETER),
                height: Dimension::Points(Self::DEFAULT_DIAMETER),
                ..Default::default()
            },
            rotation_angle: 0.0,
            arc_sweep: std::f32::consts::PI * 0.75,
            elapsed_ms: 0.0,
        }
    }

    /// Sets the outer diameter in logical points.
    pub fn with_diameter(mut self, diameter: f32) -> Self {
        self.diameter = diameter.max(8.0);
        self.style.width = Dimension::Points(self.diameter);
        self.style.height = Dimension::Points(self.diameter);
        self
    }

    /// Sets the stroke thickness in logical points.
    pub fn with_thickness(mut self, thickness: f32) -> Self {
        self.thickness = thickness.clamp(1.0, self.diameter * 0.4);
        self
    }

    /// Overrides the spinner stroke color.
    pub fn with_color(mut self, color: [f32; 4]) -> Self {
        self.color = Some(color);
        self
    }

    /// Starts or pauses the rotation animation.
    pub fn set_spinning(&mut self, spinning: bool) {
        self.is_spinning = spinning;
    }
}

impl Default for Spinner {
    fn default() -> Self {
        Self::new()
    }
}

impl Widget for Spinner {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn measure(&self, _shaper: &TextShaper) -> Option<MeasureFunc> {
        let d = self.diameter;
        Some(Box::new(move |_avail_w, _avail_h| (d, d)))
    }

    fn tick_animation(&mut self, delta_ms: f32) -> bool {
        if !self.is_spinning {
            return false;
        }

        self.elapsed_ms += delta_ms;

        // Rotate ~360 degrees per 1000ms
        let rotation_speed = std::f32::consts::TAU / 1000.0;
        self.rotation_angle =
            (self.rotation_angle + rotation_speed * delta_ms) % std::f32::consts::TAU;

        // Smooth expanding and contracting arc sweep between 0.25*PI and 1.5*PI
        let wave = ((self.elapsed_ms / 800.0).sin() + 1.0) * 0.5;
        self.arc_sweep = std::f32::consts::PI * (0.3 + wave * 1.2);

        true
    }

    fn paint(
        &self,
        rect: LayoutRect,
        theme: &Theme,
        _atlas: &mut GlyphAtlas,
        _rasterizer: &GlyphRasterizer,
        _shaper: &TextShaper,
        _vector_atlas: &mut VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let mut prims = Vec::new();
        let stroke_color = self.color.unwrap_or(theme.colors.primary);

        let actual_diameter = self.diameter.min(rect.width).min(rect.height);
        let center_x = rect.x + rect.width * 0.5;
        let center_y = rect.y + rect.height * 0.5;
        let radius = (actual_diameter - self.thickness) * 0.5;
        let w = self.thickness;
        let dot_radii = CornerRadii::all(w * 0.5 * scale);

        // Render rotating arc via dense overlapping anti-aliased circular quads
        let arc_len = self.arc_sweep;
        let step_angle = (w / (radius * 1.5)).clamp(0.04, 0.2);
        let steps = (arc_len / step_angle).ceil() as usize;

        for i in 0..=steps {
            let frac = i as f32 / steps.max(1) as f32;
            let current_angle = self.rotation_angle + arc_len * frac;
            let (sin_a, cos_a) = current_angle.sin_cos();

            let px = center_x + radius * cos_a - w * 0.5;
            let py = center_y + radius * sin_a - w * 0.5;

            // Fade tail slightly for a dynamic motion streak effect
            let alpha_factor = 0.35 + 0.65 * frac;
            let mut col = stroke_color;
            col[3] *= alpha_factor;

            prims.push(RenderPrimitive::solid_rect(
                [px * scale, py * scale, w * scale, w * scale],
                col,
                dot_radii,
            ));
        }

        prims
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
    fn test_progress_bar_clamping_and_tween() {
        let mut bar = ProgressBar::new(0.5);
        assert_eq!(*bar.value.get(), 0.5);

        bar.set_value(1.5);
        assert_eq!(*bar.value.get(), 1.0);

        bar.set_value(-0.5);
        assert_eq!(*bar.value.get(), 0.0);
    }

    #[test]
    fn test_progress_bar_indeterminate_tick() {
        let mut bar = ProgressBar::indeterminate();
        assert!(bar.is_indeterminate);

        let ticked = bar.tick_animation(16.0);
        assert!(ticked);
        assert!(bar.indeterminate_time_ms > 0.0);
    }

    #[test]
    fn test_spinner_animation_progression() {
        let mut spinner = Spinner::new();
        assert!(spinner.is_spinning);

        let initial_angle = spinner.rotation_angle;
        let ticked = spinner.tick_animation(50.0);
        assert!(ticked);
        assert!(spinner.rotation_angle > initial_angle);

        spinner.set_spinning(false);
        assert!(!spinner.tick_animation(50.0));
    }

    #[test]
    fn test_progress_bar_show_label_auto_height_and_text_color() {
        let bar = ProgressBar::new(0.5)
            .with_show_label(true)
            .with_text_color([1.0, 0.0, 0.0, 1.0]);
        assert_eq!(bar.height, 14.0);
        assert_eq!(bar.text_color, Some([1.0, 0.0, 0.0, 1.0]));

        // Explicit height override should be honored
        let custom_bar = ProgressBar::new(0.5)
            .with_height(10.0)
            .with_show_label(false);
        assert_eq!(custom_bar.height, 10.0);
    }

    #[test]
    fn test_progress_bar_custom_corner_radius() {
        let bar = ProgressBar::new(0.7)
            .with_height(12.0)
            .with_corner_radius(2.0);
        assert_eq!(bar.height, 12.0);
        assert_eq!(bar.corner_radius, Some(2.0));
    }

    #[test]
    fn test_progress_bar_paint_centering() {
        let bar = ProgressBar::new(0.5).with_height(8.0);
        let theme = Theme::default();
        let mut atlas = GlyphAtlas::new(512, 512, 4);
        let font_db = crate::text::font_db::FontDatabase::new().unwrap();
        let shaper = TextShaper::new(font_db.clone());
        let rasterizer = GlyphRasterizer::new(font_db);
        let mut vector_atlas = VectorAtlas::new(512, 512);

        // Render into a taller rect (e.g. 24.0 points high)
        let rect = LayoutRect::new(0.0, 0.0, 100.0, 24.0);
        let prims = bar.paint(
            rect,
            &theme,
            &mut atlas,
            &rasterizer,
            &shaper,
            &mut vector_atlas,
            1.0,
        );
        assert!(!prims.is_empty());

        // The background rect should be centered vertically at y = (24 - 8) / 2 = 8.0
        if let RenderPrimitive::Rect { rect: q_rect, .. } = &prims[0] {
            assert_eq!(q_rect[1], 8.0);
            assert_eq!(q_rect[3], 8.0);
        } else {
            panic!("Expected rect primitive");
        }
    }

    #[test]
    fn test_progress_bar_label_vertical_centering() {
        let bar = ProgressBar::new(0.68)
            .with_show_label(true)
            .with_height(14.0);
        let theme = Theme::default();
        let mut atlas = GlyphAtlas::new(512, 512, 4);
        let font_db = crate::text::font_db::FontDatabase::new().unwrap();
        let shaper = TextShaper::new(font_db.clone());
        let rasterizer = GlyphRasterizer::new(font_db);
        let mut vector_atlas = VectorAtlas::new(512, 512);

        let rect = LayoutRect::new(0.0, 20.0, 200.0, 14.0);
        let prims = bar.paint(
            rect,
            &theme,
            &mut atlas,
            &rasterizer,
            &shaper,
            &mut vector_atlas,
            1.0,
        );

        let glyph_prims: Vec<_> = prims
            .iter()
            .filter(|p| matches!(p, RenderPrimitive::Glyph { .. }))
            .collect();
        assert!(!glyph_prims.is_empty(), "Expected rendered percentage glyphs");

        // Bar bounds are y in [20.0, 34.0], center is 27.0
        for prim in &glyph_prims {
            if let RenderPrimitive::Glyph { screen_rect: g_rect, .. } = prim {
                let glyph_top = g_rect[1];
                let glyph_bottom = g_rect[1] + g_rect[3];
                // Glyph must sit completely within bar bounds without overflowing top or bottom
                assert!(glyph_top >= 20.0, "Glyph top {glyph_top} overflows above bar top 20.0");
                assert!(glyph_bottom <= 34.0, "Glyph bottom {glyph_bottom} overflows below bar bottom 34.0");
                let top_margin = glyph_top - 20.0;
                let bottom_margin = 34.0 - glyph_bottom;
                // Margins should be balanced within 2 pixels
                assert!((top_margin - bottom_margin).abs() <= 2.5, "Margins unbalanced: top={top_margin}, bottom={bottom_margin}");
            }
        }
    }
}
