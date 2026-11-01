//! Toggle checkbox widget with checkmark icon and reactive state tracking.

use std::any::Any;

use super::icon::IconName;
use super::state::Property;
use super::tooltip::Tooltip;
use super::tree::Widget;
use crate::event::{EventResponse, KeyEvent, MouseButton, PointerEvent};
use crate::layout::{AlignItems, Dimension, EdgeInsets, FlexDirection, FlexStyle, LayoutRect, MeasureFunc};
use crate::render::vector::{StrokeStyle, VectorAtlas, VectorPath, VectorRasterizer};
use crate::render::{Border, CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// A binary checkable control with accompanying text label and vector check icon.
pub struct Checkbox {
    /// Label text displayed next to the checkbox.
    pub label: String,
    /// Reactive boolean checked state.
    pub checked: Property<bool>,
    /// Font size in logical points.
    pub font_size: f32,
    /// Flexbox layout style.
    pub style: FlexStyle,
    /// Whether the cursor is currently hovering over the checkbox.
    pub is_hovered: bool,
    /// Whether the checkbox currently has keyboard focus.
    pub is_focused: bool,
    /// Callback triggered when the checked state changes.
    pub on_toggle: Option<Box<dyn FnMut(bool)>>,
    /// Optional hover tooltip.
    pub tooltip: Option<Tooltip>,
}

impl Checkbox {
    /// Creates a new unchecked checkbox with the given label.
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
            tooltip: None,
        }
    }

    /// Sets the initial checked state.
    pub fn with_checked(mut self, checked: bool) -> Self {
        self.checked.set(checked);
        self
    }

    /// Attaches a hover tooltip to the checkbox.
    pub fn with_tooltip<S: Into<String>>(mut self, text: S) -> Self {
        self.tooltip = Some(Tooltip::new(text));
        self
    }

    /// Registers a state toggle callback closure.
    pub fn on_toggle<F: FnMut(bool) + 'static>(mut self, cb: F) -> Self {
        self.on_toggle = Some(Box::new(cb));
        self
    }

    /// Toggles the checked state and invokes `on_toggle` if attached.
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
            if let KeyEvent::KeyDown { key, keysym, is_repeat, .. } = event {
                if !*is_repeat
                    && (key == "Return" || key == "Space" || *keysym == 0xff0d || *keysym == 0x20)
                {
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
        vector_atlas: &mut VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let mut prims = Vec::new();
        let center_y = rect.y + rect.height * 0.5;
        let box_size = 18.0;
        let pad_left = self.style.padding.left;
        let box_x = rect.x + pad_left;
        let box_y = center_y - box_size * 0.5;

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
            let inset = 2.0;
            let check_w = (box_size - inset * 2.0) * scale;
            let phys_w = (check_w.round() as u32).max(1);
            let phys_h = phys_w;
            let uv_rect = vector_atlas.get_or_insert(IconName::Check as u64, phys_w, phys_h, || {
                let path = VectorPath::parse_svg(IconName::Check.svg_path()).with_view_box(0.0, 0.0, 24.0, 24.0);
                VectorRasterizer::rasterize(
                    &path,
                    phys_w,
                    phys_h,
                    None,
                    Some(StrokeStyle {
                        color: [1.0, 1.0, 1.0, 1.0],
                        width: (2.8 * (phys_w as f32 / 24.0)).max(1.8),
                        ..Default::default()
                    }),
                )
            });

            if let Some(uv) = uv_rect {
                prims.push(RenderPrimitive::Rect {
                    rect: [
                        (box_x + inset) * scale,
                        (box_y + inset) * scale,
                        check_w,
                        check_w,
                    ],
                    color: theme.colors.on_primary,
                    radii: CornerRadii::all(0.0),
                    border: None,
                    uv_rect: uv,
                    use_texture: true,
                });
            }
        }

        if !self.label.is_empty() {
            let shaped = shaper.shape(&self.label, self.font_size);
            let text_x = box_x + box_size + 8.0;
            let cap_height = self.font_size * 0.70;
            let text_y = center_y + cap_height * 0.5;

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
            is_repeat: false,
        });
        assert_eq!(key_resp, EventResponse::Handled);
        assert!(!*cb.checked.get());
        assert!(!toggled.load(Ordering::SeqCst));
    }
}
