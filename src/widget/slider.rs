//! Continuous and stepped slider input widget with draggable thumb and keyboard adjustment.

use std::any::Any;

use super::state::Property;
use super::tooltip::Tooltip;
use super::tree::Widget;
use crate::event::{EventResponse, KeyEvent, MouseButton, PointerEvent};
use crate::layout::{Dimension, FlexStyle, LayoutRect, MeasureFunc};
use crate::render::{Border, CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// Horizontal slider widget for selecting numeric values within a defined range.
pub struct Slider {
    /// Minimum allowed value.
    pub min: f32,
    /// Maximum allowed value.
    pub max: f32,
    /// Reactive current value property.
    pub value: Property<f32>,
    /// Optional discrete quantization step interval.
    pub step: Option<f32>,
    /// Flexbox layout style.
    pub style: FlexStyle,
    /// Whether the pointer is hovering over the slider.
    pub is_hovered: bool,
    /// Whether the user is actively dragging the slider thumb.
    pub is_dragging: bool,
    /// Whether the slider currently has keyboard focus.
    pub is_focused: bool,
    /// Callback executed when the value changes.
    pub on_change: Option<Box<dyn FnMut(f32)>>,
    /// Optional hover tooltip.
    pub tooltip: Option<Tooltip>,
}

impl Slider {
    /// Creates a new slider with specified bounds and initial value.
    pub fn new(min: f32, max: f32, initial_value: f32) -> Self {
        let clamped = initial_value.clamp(min, max);
        Self {
            min,
            max,
            value: Property::new(clamped),
            step: None,
            style: FlexStyle {
                width: Dimension::Points(160.0),
                height: Dimension::Points(24.0),
                ..Default::default()
            },
            is_hovered: false,
            is_dragging: false,
            is_focused: false,
            on_change: None,
            tooltip: None,
        }
    }

    /// Sets the discrete quantization step size.
    pub fn with_step(mut self, step: f32) -> Self {
        self.step = Some(step);
        self
    }

    /// Sets the width of the slider track.
    pub fn with_width(mut self, width: f32) -> Self {
        self.style.width = Dimension::Points(width);
        self
    }

    /// Attaches a hover tooltip to the slider widget.
    pub fn with_tooltip<S: Into<String>>(mut self, text: S) -> Self {
        self.tooltip = Some(Tooltip::new(text));
        self
    }

    /// Attaches a value change callback handler.
    pub fn on_change<F: FnMut(f32) + 'static>(mut self, cb: F) -> Self {
        self.on_change = Some(Box::new(cb));
        self
    }

    fn update_value_from_x(&mut self, px: f32, rect: LayoutRect) {
        let thumb_radius = 8.0;
        let track_start = rect.x + thumb_radius;
        let track_width = (rect.width - thumb_radius * 2.0).max(1.0);
        let fraction = ((px - track_start) / track_width).clamp(0.0, 1.0);

        let mut raw = self.min + fraction * (self.max - self.min);
        if let Some(step) = self.step {
            if step > 0.0 {
                raw = (raw / step).round() * step;
            }
        }
        let final_val = raw.clamp(self.min, self.max);
        if self.value.set(final_val) {
            if let Some(cb) = &mut self.on_change {
                cb(final_val);
            }
        }
    }
}

impl Widget for Slider {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn measure(&self, _shaper: &TextShaper) -> Option<MeasureFunc> {
        let w = match self.style.width {
            Dimension::Points(p) => p,
            _ => 160.0,
        };
        let h = match self.style.height {
            Dimension::Points(p) => p,
            _ => 24.0,
        };
        Some(Box::new(move |_avail_w, _avail_h| (w, h)))
    }

    fn is_focusable(&self) -> bool {
        true
    }

    fn on_focus_change(&mut self, focused: bool) {
        self.is_focused = focused;
    }

    fn on_pointer_event(&mut self, event: &PointerEvent, rect: LayoutRect) -> EventResponse {
        match *event {
            PointerEvent::Move { x, y } => {
                let inside = rect.contains(x, y);
                if inside != self.is_hovered {
                    self.is_hovered = inside;
                }
                if self.is_dragging {
                    self.update_value_from_x(x, rect);
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Down { x, y, button } => {
                if button == MouseButton::Left && rect.contains(x, y) {
                    self.is_dragging = true;
                    self.update_value_from_x(x, rect);
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Up { button, .. } => {
                if button == MouseButton::Left && self.is_dragging {
                    self.is_dragging = false;
                    return EventResponse::Handled;
                }
            }
            _ => {}
        }
        EventResponse::Ignored
    }

    fn on_key_event(&mut self, event: &KeyEvent) -> EventResponse {
        if self.is_focused {
            if let KeyEvent::KeyDown { key, keysym, .. } = event {
                let delta = self.step.unwrap_or((self.max - self.min) * 0.05);
                let current = *self.value.get();
                if key == "Left" || key == "Down" || *keysym == 0xff51 || *keysym == 0xff54 {
                    let next = (current - delta).clamp(self.min, self.max);
                    if self.value.set(next) {
                        if let Some(cb) = &mut self.on_change {
                            cb(next);
                        }
                    }
                    return EventResponse::Handled;
                } else if key == "Right" || key == "Up" || *keysym == 0xff53 || *keysym == 0xff52 {
                    let next = (current + delta).clamp(self.min, self.max);
                    if self.value.set(next) {
                        if let Some(cb) = &mut self.on_change {
                            cb(next);
                        }
                    }
                    return EventResponse::Handled;
                }
            }
        }
        EventResponse::Ignored
    }

    fn paint(
        &self,
        rect: LayoutRect,
        theme: &Theme,
        _atlas: &mut GlyphAtlas,
        _rasterizer: &GlyphRasterizer,
        _shaper: &TextShaper,
        _vector_atlas: &mut crate::render::VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let mut prims = Vec::new();
        let thumb_radius = 8.0;
        let track_h = 6.0;
        let track_y = rect.y + (rect.height - track_h) / 2.0;

        let track_start = rect.x + thumb_radius;
        let track_width = (rect.width - thumb_radius * 2.0).max(1.0);

        let fraction = if (self.max - self.min).abs() > 0.0001 {
            ((*self.value.get() - self.min) / (self.max - self.min)).clamp(0.0, 1.0)
        } else {
            0.0
        };

        let thumb_cx = track_start + fraction * track_width;
        let thumb_cy = rect.y + rect.height / 2.0;

        prims.push(RenderPrimitive::solid_rect(
            [
                track_start * scale,
                track_y * scale,
                track_width * scale,
                track_h * scale,
            ],
            theme.colors.surface,
            CornerRadii::all(3.0 * scale),
        ));

        let filled_width = fraction * track_width;
        if filled_width > 0.0 {
            prims.push(RenderPrimitive::solid_rect(
                [
                    track_start * scale,
                    track_y * scale,
                    filled_width * scale,
                    track_h * scale,
                ],
                theme.colors.primary,
                CornerRadii::all(3.0 * scale),
            ));
        }

        let thumb_diameter = thumb_radius * 2.0;
        let thumb_x = thumb_cx - thumb_radius;
        let thumb_y = thumb_cy - thumb_radius;

        let border_color = if self.is_focused {
            theme.colors.border_focused
        } else if self.is_hovered || self.is_dragging {
            theme.colors.primary_hover
        } else {
            theme.colors.border
        };

        let border_width = if self.is_focused { 2.0 * scale } else { 1.5 * scale };

        prims.push(RenderPrimitive::bordered_rect(
            [
                thumb_x * scale,
                thumb_y * scale,
                thumb_diameter * scale,
                thumb_diameter * scale,
            ],
            [1.0, 1.0, 1.0, 1.0],
            CornerRadii::all(thumb_radius * scale),
            Border::new(border_color, border_width),
        ));

        prims
    }

    fn cursor_icon_at(&self, _rel_x: f32, _rel_y: f32) -> crate::event::CursorIcon {
        if self.is_dragging {
            crate::event::CursorIcon::Grabbing
        } else {
            crate::event::CursorIcon::Pointer
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
    use std::sync::atomic::{AtomicU32, Ordering};
    use std::sync::Arc;
    use crate::event::Modifiers;

    #[test]
    fn test_slider_dragging_and_keyboard_steps() {
        let changed_val = Arc::new(AtomicU32::new(0));
        let changed_clone = changed_val.clone();

        let mut slider = Slider::new(0.0, 100.0, 50.0)
            .with_step(10.0)
            .on_change(move |v| {
                changed_clone.store(v as u32, Ordering::SeqCst);
            });

        assert_eq!(*slider.value.get(), 50.0);

        let rect = LayoutRect::new(0.0, 0.0, 100.0, 24.0);

        // Click near the right edge (x = 90.0) -> should update towards 90..100
        slider.on_pointer_event(
            &PointerEvent::Down {
                x: 90.0,
                y: 12.0,
                button: MouseButton::Left,
            },
            rect,
        );
        assert!(*slider.value.get() >= 80.0);

        // Keyboard Left arrow decreases by step (10.0)
        slider.on_focus_change(true);
        let curr = *slider.value.get();
        slider.on_key_event(&KeyEvent::KeyDown {
            key: "Left".to_string(),
            keysym: 0xff51,
            text: None,
            modifiers: Modifiers::default(),
        });
        assert_eq!(*slider.value.get(), curr - 10.0);
    }
}
