use std::any::Any;

use super::state::Property;
use super::tree::Widget;
use crate::event::{EventResponse, KeyEvent, MouseButton, PointerEvent};
use crate::layout::{AlignItems, Dimension, EdgeInsets, FlexDirection, FlexStyle, LayoutRect, MeasureFunc};
use crate::render::{Border, CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

pub struct Checkbox {
    pub label: String,
    pub checked: Property<bool>,
    pub font_size: f32,
    pub style: FlexStyle,
    pub is_hovered: bool,
    pub is_focused: bool,
    pub on_toggle: Option<Box<dyn FnMut(bool)>>,
}

impl Checkbox {
    pub fn new<S: Into<String>>(label: S) -> Self {
        Self {
            label: label.into(),
            checked: Property::new(false),
            font_size: 14.0,
            style: FlexStyle {
                direction: FlexDirection::Row,
                align_items: AlignItems::Center,
                padding: EdgeInsets::symmetric(4.0, 8.0),
                width: Dimension::Auto,
                height: Dimension::Auto,
                ..Default::default()
            },
            is_hovered: false,
            is_focused: false,
            on_toggle: None,
        }
    }

    pub fn with_checked(mut self, checked: bool) -> Self {
        self.checked.set(checked);
        self
    }

    pub fn on_toggle<F: FnMut(bool) + 'static>(mut self, cb: F) -> Self {
        self.on_toggle = Some(Box::new(cb));
        self
    }

    pub fn toggle(&mut self) {
        let new_val = !*self.checked.get();
        self.checked.set(new_val);
        if let Some(cb) = &mut self.on_toggle {
            cb(new_val);
        }
    }
}

impl Widget for Checkbox {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn measure(&self, shaper: &TextShaper) -> Option<MeasureFunc> {
        let shaped = shaper.shape(&self.label, self.font_size);
        let box_size = 18.0;
        let gap = 8.0;
        let pad_h = self.style.padding.horizontal();
        let pad_v = self.style.padding.vertical();

        let w = box_size + gap + shaped.width + pad_h;
        let h = box_size.max(shaped.height) + pad_v;
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
        atlas: &mut GlyphAtlas,
        rasterizer: &GlyphRasterizer,
        shaper: &TextShaper,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let mut prims = Vec::new();
        let box_size = 18.0;
        let pad_left = self.style.padding.left;
        let box_x = rect.x + pad_left;
        let box_y = rect.y + (rect.height - box_size) / 2.0;

        let is_checked = *self.checked.get();
        let box_bg = if is_checked {
            theme.colors.primary
        } else if self.is_hovered {
            theme.colors.surface_hover
        } else {
            theme.colors.surface
        };

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
                box_x * scale,
                box_y * scale,
                box_size * scale,
                box_size * scale,
            ],
            box_bg,
            CornerRadii::all(theme.border_radius_sm * scale),
            Border::new(border_color, border_width),
        ));

        if is_checked {
            let inset = 4.0;
            prims.push(RenderPrimitive::solid_rect(
                [
                    (box_x + inset) * scale,
                    (box_y + inset) * scale,
                    (box_size - inset * 2.0) * scale,
                    (box_size - inset * 2.0) * scale,
                ],
                theme.colors.on_primary,
                2.0 * scale,
            ));
        }

        if !self.label.is_empty() {
            let shaped = shaper.shape(&self.label, self.font_size);
            let text_x = box_x + box_size + 8.0;
            let text_y = rect.y + (rect.height - shaped.height) / 2.0 + shaped.ascent;

            crate::text::emit_text_glyphs(
                &shaped,
                text_x,
                text_y,
                self.font_size,
                theme.colors.text,
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

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicBool, Ordering};
    use std::sync::Arc;
    use crate::event::Modifiers;

    #[test]
    fn test_checkbox_toggle_and_events() {
        let toggled = Arc::new(AtomicBool::new(false));
        let toggled_clone = toggled.clone();

        let mut cb = Checkbox::new("Test Option").on_toggle(move |val| {
            toggled_clone.store(val, Ordering::SeqCst);
        });

        assert!(!*cb.checked.get());

        // Toggle via pointer click
        let rect = LayoutRect::new(0.0, 0.0, 100.0, 24.0);
        let resp = cb.on_pointer_event(
            &PointerEvent::Down {
                x: 10.0,
                y: 12.0,
                button: MouseButton::Left,
            },
            rect,
        );
        assert_eq!(resp, EventResponse::Handled);
        assert!(*cb.checked.get());
        assert!(toggled.load(Ordering::SeqCst));

        // Toggle via keyboard Space when focused
        cb.on_focus_change(true);
        let key_resp = cb.on_key_event(&KeyEvent::KeyDown {
            key: "Space".to_string(),
            keysym: 0x20,
            text: Some(" ".to_string()),
            modifiers: Modifiers::default(),
        });
        assert_eq!(key_resp, EventResponse::Handled);
        assert!(!*cb.checked.get());
        assert!(!toggled.load(Ordering::SeqCst));
    }
}
