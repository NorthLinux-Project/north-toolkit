//! Animated toggle switch widget with smooth thumb easing and focus indicator.

use std::any::Any;

use super::state::Property;
use super::tooltip::Tooltip;
use super::tree::Widget;
use crate::animation::{Easing, Tween};
use crate::event::{EventResponse, KeyEvent, MouseButton, PointerEvent};
use crate::layout::{Dimension, FlexStyle, LayoutRect, MeasureFunc};
use crate::render::{Border, CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// Animated boolean toggle switch widget.
pub struct Switch {
    /// Reactive checked state property.
    pub checked: Property<bool>,
    /// Flexbox layout style.
    pub style: FlexStyle,
    /// Whether the pointer is currently hovering over the switch track.
    pub is_hovered: bool,
    /// Whether the switch currently has keyboard focus.
    pub is_focused: bool,
    /// Callback executed when the switch state is toggled.
    pub on_toggle: Option<Box<dyn FnMut(bool)>>,
    thumb_tween: Tween<f32>,
    /// Optional hover tooltip.
    pub tooltip: Option<Tooltip>,
}

impl Switch {
    /// Creates a new switch with specified initial checked state.
    pub fn new(checked: bool) -> Self {
        let initial_progress = if checked { 1.0 } else { 0.0 };
        Self {
            checked: Property::new(checked),
            style: FlexStyle {
                width: Dimension::Points(44.0),
                height: Dimension::Points(24.0),
                ..Default::default()
            },
            is_hovered: false,
            is_focused: false,
            on_toggle: None,
            thumb_tween: Tween::new(initial_progress),
            tooltip: None,
        }
    }

    /// Attaches a hover tooltip to the switch widget.
    pub fn with_tooltip<S: Into<String>>(mut self, text: S) -> Self {
        self.tooltip = Some(Tooltip::new(text));
        self
    }

    /// Attaches a state change callback handler.
    pub fn on_toggle<F: FnMut(bool) + 'static>(mut self, cb: F) -> Self {
        self.on_toggle = Some(Box::new(cb));
        self
    }

    /// Toggles the checked state and starts the thumb slide easing animation.
    pub fn toggle(&mut self) {
        let new_val = !*self.checked.get();
        self.checked.set(new_val);
        let target = if new_val { 1.0 } else { 0.0 };
        self.thumb_tween.set_target(target, 180.0, Easing::EaseOut);
        if let Some(cb) = &mut self.on_toggle {
            cb(new_val);
        }
    }
}

impl Widget for Switch {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn measure(&self, _shaper: &TextShaper) -> Option<MeasureFunc> {
        Some(Box::new(|_avail_w, _avail_h| (44.0, 24.0)))
    }

    fn is_focusable(&self) -> bool {
        true
    }

    fn on_focus_change(&mut self, focused: bool) {
        self.is_focused = focused;
    }

    fn tick_animation(&mut self, delta_ms: f32) -> bool {
        if !self.thumb_tween.is_complete() {
            self.thumb_tween.tick(delta_ms);
            true
        } else {
            false
        }
    }

    fn on_pointer_event(&mut self, event: &PointerEvent, rect: LayoutRect) -> EventResponse {
        match *event {
            PointerEvent::Move { x, y } => {
                let inside = rect.contains(x, y);
                if inside != self.is_hovered {
                    self.is_hovered = inside;
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Down { x, y, button } => {
                if button == MouseButton::Left && rect.contains(x, y) {
                    self.toggle();
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
                if key == "Return" || key == "Space" || *keysym == 0xff0d || *keysym == 0x20 {
                    self.toggle();
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
        let track_w = 44.0;
        let track_h = 24.0;
        let track_x = rect.x;
        let track_y = rect.y + (rect.height - track_h) / 2.0;

        let t = self.thumb_tween.value().clamp(0.0, 1.0);

        let off_color = theme.colors.surface_active;
        let on_color = theme.colors.primary;
        let track_color = [
            off_color[0] + (on_color[0] - off_color[0]) * t,
            off_color[1] + (on_color[1] - off_color[1]) * t,
            off_color[2] + (on_color[2] - off_color[2]) * t,
            1.0,
        ];

        let border_color = if self.is_focused {
            theme.colors.border_focused
        } else if self.is_hovered {
            theme.colors.primary_hover
        } else {
            theme.colors.border
        };

        let border_width = if self.is_focused { 2.0 * scale } else { 1.0 * scale };

        prims.push(RenderPrimitive::bordered_rect(
            [
                track_x * scale,
                track_y * scale,
                track_w * scale,
                track_h * scale,
            ],
            track_color,
            CornerRadii::all((track_h / 2.0) * scale),
            Border::new(border_color, border_width),
        ));

        let thumb_size = 18.0;
        let padding = 3.0;
        let min_thumb_x = track_x + padding;
        let max_thumb_x = track_x + track_w - thumb_size - padding;
        let thumb_x = min_thumb_x + t * (max_thumb_x - min_thumb_x);
        let thumb_y = track_y + (track_h - thumb_size) / 2.0;

        prims.push(RenderPrimitive::solid_rect(
            [
                thumb_x * scale,
                thumb_y * scale,
                thumb_size * scale,
                thumb_size * scale,
            ],
            [1.0, 1.0, 1.0, 1.0],
            CornerRadii::all((thumb_size / 2.0) * scale),
        ));

        prims
    }

    fn cursor_icon_at(&self, _rel_x: f32, _rel_y: f32) -> crate::event::CursorIcon {
        crate::event::CursorIcon::Pointer
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
    use std::sync::atomic::{AtomicBool, Ordering};
    use std::sync::Arc;
    use crate::event::Modifiers;

    #[test]
    fn test_switch_toggle_and_animation() {
        let state = Arc::new(AtomicBool::new(false));
        let state_clone = state.clone();

        let mut sw = Switch::new(false).on_toggle(move |v| {
            state_clone.store(v, Ordering::SeqCst);
        });

        assert!(!*sw.checked.get());

        let rect = LayoutRect::new(0.0, 0.0, 44.0, 24.0);
        sw.on_pointer_event(
            &PointerEvent::Down {
                x: 10.0,
                y: 12.0,
                button: MouseButton::Left,
            },
            rect,
        );

        assert!(*sw.checked.get());
        assert!(state.load(Ordering::SeqCst));
        assert!(sw.tick_animation(50.0));

        // Advance animation to completion
        sw.tick_animation(200.0);
        assert!(!sw.tick_animation(10.0));

        // Keyboard toggle
        sw.on_focus_change(true);
        sw.on_key_event(&KeyEvent::KeyDown {
            key: "Return".to_string(),
            keysym: 0xff0d,
            text: Some("\n".to_string()),
            modifiers: Modifiers::default(),
        });
        assert!(!*sw.checked.get());
    }
}
