use std::any::Any;

use super::tree::Widget;
use crate::animation::{Easing, Tween};
use crate::event::{EventResponse, KeyEvent, MouseButton, PointerEvent};
use crate::layout::{AlignItems, Dimension, EdgeInsets, FlexDirection, FlexStyle, JustifyContent, LayoutRect, MeasureFunc};
use crate::render::{Border, CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

pub struct Button {
    pub label: String,
    pub font_size: f32,
    pub style: FlexStyle,
    pub on_click: Option<Box<dyn FnMut()>>,
    pub is_hovered: bool,
    pub is_pressed: bool,
    pub is_focused: bool,
    pub corner_radii: Option<CornerRadii>,
    color_tween: Tween<[f32; 4]>,
    pub base_color: [f32; 4],
    pub hover_color: [f32; 4],
    pub pressed_color: [f32; 4],
    pub text_color: Option<[f32; 4]>,
}

impl Button {
    pub fn new<S: Into<String>>(label: S) -> Self {
        let base_color = [0.20, 0.45, 0.90, 1.0];
        Self {
            label: label.into(),
            font_size: 14.0,
            style: FlexStyle {
                direction: FlexDirection::Row,
                justify_content: JustifyContent::Center,
                align_items: AlignItems::Center,
                padding: EdgeInsets::symmetric(8.0, 16.0),
                width: Dimension::Auto,
                height: Dimension::Auto,
                ..Default::default()
            },
            on_click: None,
            is_hovered: false,
            is_pressed: false,
            is_focused: false,
            corner_radii: None,
            color_tween: Tween::new(base_color),
            base_color,
            hover_color: [0.26, 0.52, 0.96, 1.0],
            pressed_color: [0.12, 0.34, 0.82, 1.0],
            text_color: None,
        }
    }

    pub fn secondary<S: Into<String>>(label: S) -> Self {
        let mut btn = Self::new(label);
        btn.base_color = [0.12, 0.12, 0.12, 1.0];
        btn.hover_color = [0.18, 0.18, 0.18, 1.0];
        btn.pressed_color = [0.08, 0.08, 0.08, 1.0];
        btn.color_tween = Tween::new(btn.base_color);
        btn.text_color = Some([0.88, 0.88, 0.88, 1.0]);
        btn
    }

    pub fn with_colors(
        mut self,
        base: [f32; 4],
        hover: [f32; 4],
        pressed: [f32; 4],
    ) -> Self {
        self.base_color = base;
        self.hover_color = hover;
        self.pressed_color = pressed;
        self.color_tween = Tween::new(base);
        self
    }

    pub fn on_click<F: FnMut() + 'static>(mut self, cb: F) -> Self {
        self.on_click = Some(Box::new(cb));
        self
    }

    pub fn with_padding(mut self, vertical: f32, horizontal: f32) -> Self {
        self.style.padding = EdgeInsets::symmetric(vertical, horizontal);
        self
    }

    pub fn with_corner_radii(mut self, radii: impl Into<CornerRadii>) -> Self {
        self.corner_radii = Some(radii.into());
        self
    }
}

impl Widget for Button {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn measure(&self, shaper: &TextShaper) -> Option<MeasureFunc> {
        let shaped = shaper.shape(&self.label, self.font_size);
        let pad_h = self.style.padding.horizontal();
        let pad_v = self.style.padding.vertical();
        let w = shaped.width + pad_h;
        let h = shaped.height + pad_v;
        Some(Box::new(move |_avail_w, _avail_h| (w, h)))
    }

    fn tick_animation(&mut self, delta_ms: f32) -> bool {
        if !self.color_tween.is_complete() {
            self.color_tween.tick(delta_ms);
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
                    let target_color = if self.is_hovered {
                        self.hover_color
                    } else {
                        self.base_color
                    };
                    self.color_tween.set_target(target_color, 150.0, Easing::EaseOut);
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Down { x, y, button } => {
                if button == MouseButton::Left && rect.contains(x, y) {
                    self.is_pressed = true;
                    self.color_tween.set_target(self.pressed_color, 80.0, Easing::EaseOut);
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Up { x, y, button } => {
                if button == MouseButton::Left && self.is_pressed {
                    self.is_pressed = false;
                    let target_color = if rect.contains(x, y) {
                        self.hover_color
                    } else {
                        self.base_color
                    };
                    self.color_tween.set_target(target_color, 150.0, Easing::EaseOut);

                    if rect.contains(x, y) {
                        if let Some(cb) = &mut self.on_click {
                            cb();
                        }
                    }
                    return EventResponse::Handled;
                }
            }
            _ => {}
        }
        EventResponse::Ignored
    }

    fn is_focusable(&self) -> bool {
        true
    }

    fn on_focus_change(&mut self, focused: bool) {
        self.is_focused = focused;
    }

    fn on_key_event(&mut self, event: &KeyEvent) -> EventResponse {
        if self.is_focused {
            if let KeyEvent::KeyDown { key, keysym, .. } = event {
                if key == "Return" || key == "Space" || *keysym == 0xff0d || *keysym == 0x20 {
                    if let Some(cb) = &mut self.on_click {
                        cb();
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
        atlas: &mut GlyphAtlas,
        rasterizer: &GlyphRasterizer,
        shaper: &TextShaper,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let mut prims = Vec::new();
        let bg_color = self.color_tween.value();

        let border_color = if self.is_pressed {
            [0.15, 0.15, 0.15, 1.0]
        } else if self.is_focused {
            theme.colors.border_focused
        } else if self.is_hovered {
            theme.colors.border_focused
        } else {
            theme.colors.border
        };

        let border_width = if self.is_focused { 2.0 * scale } else { 1.0 * scale };
        let press_offset_y = if self.is_pressed { 1.0 * scale } else { 0.0 };

        let radii = self
            .corner_radii
            .map(|r| r.scale(scale))
            .unwrap_or_else(|| CornerRadii::all(theme.border_radius_md * scale));

        prims.push(RenderPrimitive::bordered_rect(
            [
                rect.x * scale,
                (rect.y * scale) + press_offset_y,
                rect.width * scale,
                rect.height * scale,
            ],
            bg_color,
            radii,
            Border::new(border_color, border_width),
        ));

        let shaped = shaper.shape(&self.label, self.font_size);
        let text_logical_x = rect.x + (rect.width - shaped.width) / 2.0;
        let press_offset_logical_y = if self.is_pressed { 1.0 } else { 0.0 };
        let text_logical_baseline = rect.y + (rect.height - shaped.height) / 2.0 + shaped.ascent + press_offset_logical_y;
        let label_color = self.text_color.unwrap_or(theme.colors.on_primary);

        crate::text::emit_text_glyphs(
            &shaped,
            text_logical_x,
            text_logical_baseline,
            self.font_size,
            label_color,
            scale,
            atlas,
            rasterizer,
            &mut prims,
        );

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
    use std::sync::atomic::{AtomicBool, Ordering};
    use std::sync::Arc;

    #[test]
    fn test_button_hover_and_click_effects() {
        let clicked = Arc::new(AtomicBool::new(false));
        let clicked_clone = clicked.clone();

        let mut btn = Button::new("Test Button").on_click(move || {
            clicked_clone.store(true, Ordering::SeqCst);
        });

        let rect = LayoutRect {
            x: 10.0,
            y: 10.0,
            width: 100.0,
            height: 40.0,
        };

        assert!(!btn.is_hovered);
        assert!(!btn.is_pressed);

        // Hover
        let move_inside = PointerEvent::Move { x: 20.0, y: 20.0 };
        let r1 = btn.on_pointer_event(&move_inside, rect);
        assert_eq!(r1, EventResponse::Handled);
        assert!(btn.is_hovered);

        // Press
        let down = PointerEvent::Down {
            x: 20.0,
            y: 20.0,
            button: MouseButton::Left,
        };
        let r2 = btn.on_pointer_event(&down, rect);
        assert_eq!(r2, EventResponse::Handled);
        assert!(btn.is_pressed);

        // Release inside fires callback
        let up = PointerEvent::Up {
            x: 20.0,
            y: 20.0,
            button: MouseButton::Left,
        };
        let r3 = btn.on_pointer_event(&up, rect);
        assert_eq!(r3, EventResponse::Handled);
        assert!(!btn.is_pressed);
        assert!(clicked.load(Ordering::SeqCst));

        // Unhover
        let move_outside = PointerEvent::Move { x: 200.0, y: 200.0 };
        let r4 = btn.on_pointer_event(&move_outside, rect);
        assert_eq!(r4, EventResponse::Handled);
        assert!(!btn.is_hovered);
    }
}
