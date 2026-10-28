use std::any::Any;

use super::state::Property;
use super::tree::Widget;
use crate::event::{EventResponse, KeyEvent, MouseButton, PointerEvent};
use crate::layout::{AlignItems, Dimension, EdgeInsets, FlexDirection, FlexStyle, LayoutRect, MeasureFunc};
use crate::render::{Border, CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

pub struct TextInput {
    pub text: Property<String>,
    pub placeholder: String,
    pub font_size: f32,
    pub style: FlexStyle,
    pub is_focused: bool,
    pub cursor_pos: usize,
    pub corner_radii: Option<CornerRadii>,
    cursor_timer_ms: f32,
    cursor_visible: bool,
}

impl TextInput {
    pub fn new<S: Into<String>>(placeholder: S) -> Self {
        Self {
            text: Property::new(String::new()),
            placeholder: placeholder.into(),
            font_size: 14.0,
            style: FlexStyle {
                direction: FlexDirection::Row,
                align_items: AlignItems::Center,
                padding: EdgeInsets::symmetric(8.0, 12.0),
                width: Dimension::Points(200.0),
                height: Dimension::Points(36.0),
                ..Default::default()
            },
            is_focused: false,
            cursor_pos: 0,
            corner_radii: None,
            cursor_timer_ms: 0.0,
            cursor_visible: true,
        }
    }

    pub fn with_width(mut self, width: f32) -> Self {
        self.style.width = Dimension::Points(width);
        self
    }

    pub fn with_corner_radii(mut self, radii: impl Into<CornerRadii>) -> Self {
        self.corner_radii = Some(radii.into());
        self
    }
}

impl Widget for TextInput {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn measure(&self, shaper: &TextShaper) -> Option<MeasureFunc> {
        let sample = if self.text.get().is_empty() {
            &self.placeholder
        } else {
            self.text.get()
        };
        let shaped = shaper.shape(sample, self.font_size);
        let h = (shaped.height + self.style.padding.vertical()).max(32.0);
        let w = (shaped.width + self.style.padding.horizontal() + 20.0).max(120.0);
        Some(Box::new(move |_avail_w, _avail_h| (w, h)))
    }

    fn tick_animation(&mut self, delta_ms: f32) -> bool {
        if self.is_focused {
            self.cursor_timer_ms += delta_ms;
            if self.cursor_timer_ms >= 500.0 {
                self.cursor_timer_ms = 0.0;
                self.cursor_visible = !self.cursor_visible;
                return true;
            }
        }
        false
    }

    fn on_focus_change(&mut self, focused: bool) {
        self.is_focused = focused;
        self.cursor_visible = focused;
        self.cursor_timer_ms = 0.0;
    }

    fn on_pointer_event(&mut self, event: &PointerEvent, rect: LayoutRect) -> EventResponse {
        match *event {
            PointerEvent::Down { x, y, button } => {
                if button == MouseButton::Left && rect.contains(x, y) {
                    self.is_focused = true;
                    self.cursor_visible = true;
                    self.cursor_timer_ms = 0.0;
                    self.cursor_pos = self.text.get().chars().count();
                    return EventResponse::Handled;
                }
            }
            _ => {}
        }
        EventResponse::Ignored
    }

    fn on_key_event(&mut self, event: &KeyEvent) -> EventResponse {
        if !self.is_focused {
            return EventResponse::Ignored;
        }

        match event {
            KeyEvent::KeyDown { key, text } => {
                let current = self.text.get().clone();
                match key.as_str() {
                    "Backspace" => {
                        if self.cursor_pos > 0 && !current.is_empty() {
                            let mut chars: Vec<char> = current.chars().collect();
                            if self.cursor_pos <= chars.len() {
                                chars.remove(self.cursor_pos - 1);
                                self.cursor_pos -= 1;
                                self.text.set(chars.into_iter().collect());
                                self.cursor_visible = true;
                                self.cursor_timer_ms = 0.0;
                                return EventResponse::Handled;
                            }
                        }
                    }
                    "Delete" => {
                        let mut chars: Vec<char> = current.chars().collect();
                        if self.cursor_pos < chars.len() {
                            chars.remove(self.cursor_pos);
                            self.text.set(chars.into_iter().collect());
                            self.cursor_visible = true;
                            self.cursor_timer_ms = 0.0;
                            return EventResponse::Handled;
                        }
                    }
                    "Home" => {
                        self.cursor_pos = 0;
                        self.cursor_visible = true;
                        self.cursor_timer_ms = 0.0;
                        return EventResponse::Handled;
                    }
                    "End" => {
                        self.cursor_pos = current.chars().count();
                        self.cursor_visible = true;
                        self.cursor_timer_ms = 0.0;
                        return EventResponse::Handled;
                    }
                    "Left" => {
                        if self.cursor_pos > 0 {
                            self.cursor_pos -= 1;
                            self.cursor_visible = true;
                            self.cursor_timer_ms = 0.0;
                            return EventResponse::Handled;
                        }
                    }
                    "Right" => {
                        if self.cursor_pos < current.chars().count() {
                            self.cursor_pos += 1;
                            self.cursor_visible = true;
                            self.cursor_timer_ms = 0.0;
                            return EventResponse::Handled;
                        }
                    }
                    _ => {
                        if let Some(t) = text {
                            let mut chars: Vec<char> = current.chars().collect();
                            for c in t.chars() {
                                if !c.is_control() {
                                    chars.insert(self.cursor_pos, c);
                                    self.cursor_pos += 1;
                                }
                            }
                            self.text.set(chars.into_iter().collect());
                            self.cursor_visible = true;
                            self.cursor_timer_ms = 0.0;
                            return EventResponse::Handled;
                        }
                    }
                }
            }
            _ => {}
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

        let border_color = if self.is_focused {
            theme.colors.border_focused
        } else {
            theme.colors.border
        };

        let radii = self
            .corner_radii
            .map(|r| r.scale(scale))
            .unwrap_or_else(|| CornerRadii::all(theme.border_radius_sm * scale));

        prims.push(RenderPrimitive::bordered_rect(
            [rect.x * scale, rect.y * scale, rect.width * scale, rect.height * scale],
            theme.colors.surface,
            radii,
            Border::new(border_color, (if self.is_focused { 1.5 } else { 1.0 }) * scale),
        ));

        let is_empty = self.text.get().is_empty();
        let display_text = if is_empty {
            &self.placeholder
        } else {
            self.text.get()
        };

        let text_color = if is_empty {
            theme.colors.text_secondary
        } else {
            theme.colors.text
        };

        let shaped = shaper.shape(display_text, self.font_size);
        let text_logical_x = rect.x + self.style.padding.left;
        let text_logical_baseline = rect.y + (rect.height - shaped.height) / 2.0 + shaped.ascent;

        let physical_font_size = (self.font_size * scale).round();
        let baseline_y = (text_logical_baseline * scale).round();

        let mut cursor_screen_x = text_logical_x * scale;

        for (i, glyph) in shaped.glyphs.iter().enumerate() {
            if !is_empty && i == self.cursor_pos {
                cursor_screen_x = (text_logical_x + glyph.x) * scale;
            }

            let px_x = (text_logical_x + glyph.x) * scale;
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
                let gx = aligned_x + region.offset_x;
                let gy = baseline_y + region.offset_y;

                prims.push(RenderPrimitive::glyph(
                    region.page,
                    [gx, gy, region.width as f32, region.height as f32],
                    region.uv_rect,
                    text_color,
                ));
            }
        }

        if !is_empty && self.cursor_pos == shaped.glyphs.len() {
            cursor_screen_x = (text_logical_x + shaped.width) * scale;
        }

        if self.is_focused && self.cursor_visible {
            let cursor_h = self.font_size * 1.2 * scale;
            let cursor_y = (rect.y + (rect.height - self.font_size * 1.2) / 2.0) * scale;
            prims.push(RenderPrimitive::solid_rect(
                [cursor_screen_x, cursor_y, 2.0 * scale, cursor_h],
                theme.colors.cursor,
                1.0 * scale,
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
    fn test_text_input_typing_and_navigation() {
        let mut input = TextInput::new("Placeholder");
        assert!(!input.is_focused);

        // Keystrokes ignored while unfocused
        let r1 = input.on_key_event(&KeyEvent::KeyDown {
            key: "h".to_string(),
            text: Some("h".to_string()),
        });
        assert_eq!(r1, EventResponse::Ignored);
        assert_eq!(input.text.get(), "");

        // Focus
        input.on_focus_change(true);
        assert!(input.is_focused);

        // Type "Hi"
        input.on_key_event(&KeyEvent::KeyDown {
            key: "H".to_string(),
            text: Some("H".to_string()),
        });
        input.on_key_event(&KeyEvent::KeyDown {
            key: "i".to_string(),
            text: Some("i".to_string()),
        });
        assert_eq!(input.text.get(), "Hi");
        assert_eq!(input.cursor_pos, 2);

        // Backspace
        input.on_key_event(&KeyEvent::KeyDown {
            key: "Backspace".to_string(),
            text: None,
        });
        assert_eq!(input.text.get(), "H");
        assert_eq!(input.cursor_pos, 1);

        // Home navigation and insert at start
        input.on_key_event(&KeyEvent::KeyDown {
            key: "Home".to_string(),
            text: None,
        });
        assert_eq!(input.cursor_pos, 0);

        input.on_key_event(&KeyEvent::KeyDown {
            key: "A".to_string(),
            text: Some("A".to_string()),
        });
        assert_eq!(input.text.get(), "AH");
        assert_eq!(input.cursor_pos, 1);

        // Delete
        input.on_key_event(&KeyEvent::KeyDown {
            key: "Delete".to_string(),
            text: None,
        });
        assert_eq!(input.text.get(), "A");
    }
}
