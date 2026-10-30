//! Single-line interactive text input field with cursor blinking, text selection, and clipboard integration.

use std::any::Any;
use std::cell::{Cell, RefCell};
use std::time::Instant;

use super::icon::IconName;
use super::state::Property;
use super::tooltip::Tooltip;
use super::tree::{ClipboardAction, Widget};
use crate::event::{EventResponse, KeyEvent, MouseButton, PointerEvent};
use crate::layout::{AlignItems, Dimension, EdgeInsets, FlexDirection, FlexStyle, LayoutRect, MeasureFunc};
use crate::render::vector::{StrokeStyle, VectorAtlas, VectorPath, VectorRasterizer};
use crate::render::{Border, CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// Single-line interactive text input widget.
pub struct TextInput {
    /// Reactive string property holding the edited text.
    pub text: Property<String>,
    /// Placeholder text shown when the input buffer is empty.
    pub placeholder: String,
    /// Font size in logical points.
    pub font_size: f32,
    /// Flexbox layout style.
    pub style: FlexStyle,
    /// Whether this input field currently has keyboard focus.
    pub is_focused: bool,
    /// Cursor character index.
    pub cursor_pos: usize,
    /// Current text selection range `(anchor, focus)`.
    pub selection: Option<(usize, usize)>,
    /// Whether the user is actively extending selection via pointer drag.
    pub is_dragging_selection: bool,
    /// Optional corner radius override for borders.
    pub corner_radii: Option<CornerRadii>,
    cursor_timer_ms: f32,
    cursor_visible: bool,
    /// Optional vector icon rendered inside the leading edge of the input.
    pub leading_icon: Option<IconName>,
    last_click_instant: Option<Instant>,
    click_count: u8,
    cached_glyph_positions: RefCell<Vec<f32>>,
    cached_text_origin_x: Cell<f32>,
    /// Horizontal scroll offset for long lines exceeding input width.
    pub scroll_offset_x: Cell<f32>,
    pending_clipboard_action: Option<ClipboardAction>,
    /// Optional hover tooltip.
    pub tooltip: Option<Tooltip>,
}

impl TextInput {
    /// Creates a new text input widget with the specified placeholder text.
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
            selection: None,
            is_dragging_selection: false,
            corner_radii: None,
            cursor_timer_ms: 0.0,
            cursor_visible: true,
            leading_icon: None,
            last_click_instant: None,
            click_count: 0,
            cached_glyph_positions: RefCell::new(Vec::new()),
            cached_text_origin_x: Cell::new(0.0),
            scroll_offset_x: Cell::new(0.0),
            pending_clipboard_action: None,
            tooltip: None,
        }
    }

    /// Sets the fixed width of the text input.
    pub fn with_width(mut self, width: f32) -> Self {
        self.style.width = Dimension::Points(width);
        self
    }

    /// Sets a leading decorative vector icon.
    pub fn with_leading_icon(mut self, icon: IconName) -> Self {
        self.leading_icon = Some(icon);
        self
    }

    /// Sets custom corner radii for the input background and borders.
    pub fn with_corner_radii(mut self, radii: impl Into<CornerRadii>) -> Self {
        self.corner_radii = Some(radii.into());
        self
    }

    /// Attaches a hover tooltip to the input field.
    pub fn with_tooltip<S: Into<String>>(mut self, text: S) -> Self {
        self.tooltip = Some(Tooltip::new(text));
        self
    }

    /// Returns the normalized `(start, end)` character bounds of the active selection if non-empty.
    pub fn selection_range(&self) -> Option<(usize, usize)> {
        if let Some((anchor, focus)) = self.selection {
            if anchor != focus {
                return Some((anchor.min(focus), anchor.max(focus)));
            }
        }
        None
    }

    /// Returns the text slice contained within the active selection.
    pub fn selected_text(&self) -> Option<String> {
        let (start, end) = self.selection_range()?;
        let chars: Vec<char> = self.text.get().chars().collect();
        if start < chars.len() {
            let end_bounded = end.min(chars.len());
            Some(chars[start..end_bounded].iter().collect())
        } else {
            None
        }
    }

    /// Deletes the currently selected text characters from the input buffer.
    pub fn delete_selection(&mut self) -> bool {
        if let Some((start, end)) = self.selection_range() {
            let mut chars: Vec<char> = self.text.get().chars().collect();
            if start < chars.len() {
                let end_bounded = end.min(chars.len());
                chars.drain(start..end_bounded);
                self.text.set(chars.into_iter().collect());
                self.cursor_pos = start;
                self.selection = None;
                self.cursor_visible = true;
                self.cursor_timer_ms = 0.0;
                return true;
            }
        }
        self.selection = None;
        false
    }

    /// Finds word boundaries `(start, end)` around `char_idx` for double-click word selection.
    pub fn find_word_boundaries(&self, char_idx: usize) -> (usize, usize) {
        let chars: Vec<char> = self.text.get().chars().collect();
        if chars.is_empty() {
            return (0, 0);
        }
        let idx = char_idx.min(chars.len().saturating_sub(1));
        let c = chars[idx];
        let is_word_char = |ch: char| ch.is_alphanumeric() || ch == '_';

        if is_word_char(c) {
            let mut start = idx;
            while start > 0 && is_word_char(chars[start - 1]) {
                start -= 1;
            }
            let mut end = idx;
            while end < chars.len() && is_word_char(chars[end]) {
                end += 1;
            }
            (start, end)
        } else if c.is_whitespace() {
            let mut start = idx;
            while start > 0 && chars[start - 1].is_whitespace() {
                start -= 1;
            }
            let mut end = idx;
            while end < chars.len() && chars[end].is_whitespace() {
                end += 1;
            }
            (start, end)
        } else {
            let mut start = idx;
            while start > 0 && !is_word_char(chars[start - 1]) && !chars[start - 1].is_whitespace() {
                start -= 1;
            }
            let mut end = idx;
            while end < chars.len() && !is_word_char(chars[end]) && !chars[end].is_whitespace() {
                end += 1;
            }
            (start, end)
        }
    }

    /// Converts an absolute horizontal coordinate `x` into a character index.
    pub fn char_index_from_x(&self, x: f32) -> usize {
        let total_chars = self.text.get().chars().count();
        if total_chars == 0 {
            return 0;
        }
        let rel_x = x - self.cached_text_origin_x.get();
        let positions = self.cached_glyph_positions.borrow();
        if positions.is_empty() || rel_x <= 0.0 {
            return 0;
        }
        if let Some(&last) = positions.last() {
            if rel_x >= last {
                return (positions.len().saturating_sub(1)).min(total_chars);
            }
        }
        for i in 0..positions.len().saturating_sub(1) {
            let mid = (positions[i] + positions[i + 1]) * 0.5;
            if rel_x < mid {
                return i.min(total_chars);
            }
        }
        (positions.len().saturating_sub(1)).min(total_chars)
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
        if !focused {
            self.selection = None;
            self.is_dragging_selection = false;
        }
    }

    fn take_clipboard_action(&mut self) -> Option<ClipboardAction> {
        self.pending_clipboard_action.take()
    }

    fn cursor_icon_at(&self, _rel_x: f32, _rel_y: f32) -> crate::event::CursorIcon {
        crate::event::CursorIcon::Text
    }

    fn tooltip(&self) -> Option<Tooltip> {
        self.tooltip.clone()
    }

    fn on_paste(&mut self, text: &str, _is_primary: bool) {
        if !self.is_focused {
            return;
        }
        self.delete_selection();
        let mut chars: Vec<char> = self.text.get().chars().collect();
        let mut pos = self.cursor_pos.min(chars.len());
        for c in text.chars() {
            if !c.is_control() || c == '\t' {
                chars.insert(pos, c);
                pos += 1;
            }
        }
        self.cursor_pos = pos;
        self.text.set(chars.into_iter().collect());
        self.cursor_visible = true;
        self.cursor_timer_ms = 0.0;
    }

    fn on_pointer_event(&mut self, event: &PointerEvent, rect: LayoutRect) -> EventResponse {
        match *event {
            PointerEvent::Down { x, y, button } => {
                if rect.contains(x, y) {
                    if button == MouseButton::Left {
                        self.is_focused = true;
                        self.cursor_visible = true;
                        self.cursor_timer_ms = 0.0;

                        let now = Instant::now();
                        let is_double = if let Some(last) = self.last_click_instant {
                            now.duration_since(last).as_millis() < 350
                        } else {
                            false
                        };

                        if is_double {
                            self.click_count = (self.click_count % 3) + 1;
                        } else {
                            self.click_count = 1;
                        }
                        self.last_click_instant = Some(now);

                        let total_chars = self.text.get().chars().count();
                        let char_idx = self.char_index_from_x(x).min(total_chars);

                        if self.click_count == 2 {
                            let (w_start, w_end) = self.find_word_boundaries(char_idx);
                            self.selection = Some((w_start, w_end));
                            self.cursor_pos = w_end;
                            if let Some(sel) = self.selected_text() {
                                self.pending_clipboard_action = Some(ClipboardAction::SetPrimary(sel));
                            }
                        } else if self.click_count == 3 {
                            self.selection = Some((0, total_chars));
                            self.cursor_pos = total_chars;
                            if let Some(sel) = self.selected_text() {
                                self.pending_clipboard_action = Some(ClipboardAction::SetPrimary(sel));
                            }
                        } else {
                            self.cursor_pos = char_idx;
                            self.selection = Some((char_idx, char_idx));
                            self.is_dragging_selection = true;
                        }
                        return EventResponse::Handled;
                    } else if button == MouseButton::Middle {
                        self.is_focused = true;
                        self.cursor_visible = true;
                        self.cursor_timer_ms = 0.0;
                        let total_chars = self.text.get().chars().count();
                        let char_idx = self.char_index_from_x(x).min(total_chars);
                        self.cursor_pos = char_idx;
                        self.selection = None;
                        self.pending_clipboard_action = Some(ClipboardAction::RequestPaste(
                            crate::platform::clipboard::SelectionTarget::Primary,
                        ));
                        return EventResponse::Handled;
                    }
                }
            }
            PointerEvent::Move { x, .. } => {
                if self.is_dragging_selection {
                    let total_chars = self.text.get().chars().count();
                    let char_idx = self.char_index_from_x(x).min(total_chars);
                    let anchor = self.selection.map(|(a, _)| a).unwrap_or(self.cursor_pos).min(total_chars);
                    self.selection = Some((anchor, char_idx));
                    self.cursor_pos = char_idx;
                    self.cursor_visible = true;
                    self.cursor_timer_ms = 0.0;
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Up { button, .. } => {
                if button == MouseButton::Left && self.is_dragging_selection {
                    self.is_dragging_selection = false;
                    if let Some((a, f)) = self.selection {
                        if a == f {
                            self.selection = None;
                        } else if let Some(sel) = self.selected_text() {
                            self.pending_clipboard_action = Some(ClipboardAction::SetPrimary(sel));
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

    fn on_key_event(&mut self, event: &KeyEvent) -> EventResponse {
        if !self.is_focused {
            return EventResponse::Ignored;
        }

        match event {
            KeyEvent::KeyDown { key, keysym, text, modifiers } => {
                let current = self.text.get().clone();
                let total_chars = current.chars().count();

                if modifiers.ctrl && !modifiers.alt && !modifiers.logo {
                    if key == "a" || key == "A" || *keysym == 0x61 || *keysym == 0x41 {
                        self.selection = Some((0, total_chars));
                        self.cursor_pos = total_chars;
                        self.cursor_visible = true;
                        self.cursor_timer_ms = 0.0;
                        if let Some(sel) = self.selected_text() {
                            self.pending_clipboard_action = Some(ClipboardAction::SetPrimary(sel));
                        }
                        return EventResponse::Handled;
                    } else if key == "c" || key == "C" || *keysym == 0x63 || *keysym == 0x43 {
                        if let Some(sel) = self.selected_text() {
                            self.pending_clipboard_action = Some(ClipboardAction::SetClipboard(sel));
                        }
                        return EventResponse::Handled;
                    } else if key == "x" || key == "X" || *keysym == 0x78 || *keysym == 0x58 {
                        if let Some(sel) = self.selected_text() {
                            self.pending_clipboard_action = Some(ClipboardAction::SetClipboard(sel));
                            self.delete_selection();
                        }
                        return EventResponse::Handled;
                    } else if key == "v" || key == "V" || *keysym == 0x76 || *keysym == 0x56 {
                        self.pending_clipboard_action = Some(ClipboardAction::RequestPaste(
                            crate::platform::clipboard::SelectionTarget::Clipboard,
                        ));
                        return EventResponse::Handled;
                    }
                }

                if key == "Backspace" || *keysym == 0xff08 {
                    if self.delete_selection() {
                        return EventResponse::Handled;
                    }
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
                } else if key == "Delete" || *keysym == 0xffff {
                    if self.delete_selection() {
                        return EventResponse::Handled;
                    }
                    let mut chars: Vec<char> = current.chars().collect();
                    if self.cursor_pos < chars.len() {
                        chars.remove(self.cursor_pos);
                        self.text.set(chars.into_iter().collect());
                        self.cursor_visible = true;
                        self.cursor_timer_ms = 0.0;
                        return EventResponse::Handled;
                    }
                } else if key == "Home" || *keysym == 0xff50 {
                    if modifiers.shift {
                        let anchor = self.selection.map(|(a, _)| a).unwrap_or(self.cursor_pos);
                        self.cursor_pos = 0;
                        self.selection = Some((anchor, 0));
                        if let Some(sel) = self.selected_text() {
                            self.pending_clipboard_action = Some(ClipboardAction::SetPrimary(sel));
                        }
                    } else {
                        self.selection = None;
                        self.cursor_pos = 0;
                    }
                    self.cursor_visible = true;
                    self.cursor_timer_ms = 0.0;
                    return EventResponse::Handled;
                } else if key == "End" || *keysym == 0xff57 {
                    if modifiers.shift {
                        let anchor = self.selection.map(|(a, _)| a).unwrap_or(self.cursor_pos);
                        self.cursor_pos = total_chars;
                        self.selection = Some((anchor, total_chars));
                        if let Some(sel) = self.selected_text() {
                            self.pending_clipboard_action = Some(ClipboardAction::SetPrimary(sel));
                        }
                    } else {
                        self.selection = None;
                        self.cursor_pos = total_chars;
                    }
                    self.cursor_visible = true;
                    self.cursor_timer_ms = 0.0;
                    return EventResponse::Handled;
                } else if key == "Left" || *keysym == 0xff51 {
                    if modifiers.shift {
                        let anchor = self.selection.map(|(a, _)| a).unwrap_or(self.cursor_pos);
                        if self.cursor_pos > 0 {
                            self.cursor_pos -= 1;
                        }
                        self.selection = Some((anchor, self.cursor_pos));
                        if let Some(sel) = self.selected_text() {
                            self.pending_clipboard_action = Some(ClipboardAction::SetPrimary(sel));
                        }
                    } else if let Some((start, _)) = self.selection_range() {
                        self.cursor_pos = start;
                        self.selection = None;
                    } else if self.cursor_pos > 0 {
                        self.cursor_pos -= 1;
                    }
                    self.cursor_visible = true;
                    self.cursor_timer_ms = 0.0;
                    return EventResponse::Handled;
                } else if key == "Right" || *keysym == 0xff53 {
                    if modifiers.shift {
                        let anchor = self.selection.map(|(a, _)| a).unwrap_or(self.cursor_pos);
                        if self.cursor_pos < total_chars {
                            self.cursor_pos += 1;
                        }
                        self.selection = Some((anchor, self.cursor_pos));
                        if let Some(sel) = self.selected_text() {
                            self.pending_clipboard_action = Some(ClipboardAction::SetPrimary(sel));
                        }
                    } else if let Some((_, end)) = self.selection_range() {
                        self.cursor_pos = end;
                        self.selection = None;
                    } else if self.cursor_pos < total_chars {
                        self.cursor_pos += 1;
                    }
                    self.cursor_visible = true;
                    self.cursor_timer_ms = 0.0;
                    return EventResponse::Handled;
                } else if let Some(t) = text {
                    if !modifiers.ctrl && !modifiers.alt && !modifiers.logo {
                        self.delete_selection();
                        let mut chars: Vec<char> = self.text.get().chars().collect();
                        let mut pos = self.cursor_pos.min(chars.len());
                        for c in t.chars() {
                            if !c.is_control() {
                                chars.insert(pos, c);
                                pos += 1;
                            }
                        }
                        self.cursor_pos = pos;
                        self.text.set(chars.into_iter().collect());
                        self.cursor_visible = true;
                        self.cursor_timer_ms = 0.0;
                        return EventResponse::Handled;
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
        vector_atlas: &mut VectorAtlas,
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

        let center_y = rect.y + rect.height * 0.5;
        let icon_offset = if let Some(icon) = self.leading_icon {
            let icon_size = 16.0;
            let icon_x = rect.x + self.style.padding.left;
            let icon_y = center_y - icon_size * 0.5;
            let phys_size = ((icon_size * scale).round() as u32).max(1);
            let stroke_w = (2.0 * (phys_size as f32 / 24.0)).max(1.3);

            let uv_rect = vector_atlas.get_or_insert(icon as u64, phys_size, phys_size, || {
                let path = VectorPath::parse_svg(icon.svg_path()).with_view_box(0.0, 0.0, 24.0, 24.0);
                VectorRasterizer::rasterize(
                    &path,
                    phys_size,
                    phys_size,
                    None,
                    Some(StrokeStyle {
                        color: [1.0, 1.0, 1.0, 1.0],
                        width: stroke_w,
                        ..Default::default()
                    }),
                )
            });

            if let Some(uv) = uv_rect {
                prims.push(RenderPrimitive::Rect {
                    rect: [
                        icon_x * scale,
                        icon_y * scale,
                        icon_size * scale,
                        icon_size * scale,
                    ],
                    color: theme.colors.text_secondary,
                    radii: CornerRadii::all(0.0),
                    border: None,
                    uv_rect: uv,
                    use_texture: true,
                });
            }
            icon_size + 8.0
        } else {
            0.0
        };

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
        let text_area_x = rect.x + self.style.padding.left + icon_offset;
        let text_area_w = (rect.x + rect.width - self.style.padding.right - text_area_x).max(0.0);
        let text_area_y = rect.y;
        let text_area_h = rect.height;

        let cursor_rel_x = if is_empty {
            0.0
        } else {
            let cursor = self.cursor_pos.min(shaped.glyphs.len());
            if cursor < shaped.glyphs.len() {
                shaped.glyphs[cursor].x
            } else {
                shaped.width
            }
        };

        let mut scroll_x = self.scroll_offset_x.get();
        if is_empty || shaped.width <= text_area_w {
            scroll_x = 0.0;
        } else {
            if cursor_rel_x - scroll_x > text_area_w - 6.0 {
                scroll_x = cursor_rel_x - (text_area_w - 6.0);
            } else if cursor_rel_x - scroll_x < 6.0 {
                scroll_x = (cursor_rel_x - 6.0).max(0.0);
            }
            scroll_x = scroll_x.clamp(0.0, (shaped.width - text_area_w + 12.0).max(0.0));
        }
        self.scroll_offset_x.set(scroll_x);

        let text_logical_x = text_area_x - scroll_x;
        self.cached_text_origin_x.set(text_logical_x);

        let cap_height = self.font_size * 0.70;
        let text_logical_baseline = center_y + cap_height * 0.5;

        if is_empty {
            *self.cached_glyph_positions.borrow_mut() = vec![0.0];
        } else {
            let mut positions = Vec::with_capacity(shaped.glyphs.len() + 1);
            for g in &shaped.glyphs {
                positions.push(g.x);
            }
            positions.push(shaped.width);
            *self.cached_glyph_positions.borrow_mut() = positions;
        }

        prims.push(RenderPrimitive::ClipPush {
            rect: [
                text_area_x * scale,
                text_area_y * scale,
                text_area_w * scale,
                text_area_h * scale,
            ],
        });

        if !is_empty {
            if let Some((start, end)) = self.selection_range() {
                if start < end {
                    let cached = self.cached_glyph_positions.borrow();
                    let x_start = cached.get(start).copied().unwrap_or(0.0);
                    let x_end = cached.get(end).copied().unwrap_or(shaped.width);
                    let sel_rect = [
                        (text_logical_x + x_start) * scale,
                        (center_y - self.font_size * 0.6) * scale,
                        ((x_end - x_start).max(1.0)) * scale,
                        (self.font_size * 1.2) * scale,
                    ];
                    prims.push(RenderPrimitive::solid_rect(
                        sel_rect,
                        theme.colors.selection,
                        2.0 * scale,
                    ));
                }
            }
        }

        let cursor_screen_x = (text_logical_x + cursor_rel_x) * scale;

        crate::text::emit_text_glyphs(
            &shaped,
            text_logical_x,
            text_logical_baseline,
            self.font_size,
            text_color,
            scale,
            atlas,
            rasterizer,
            &mut prims,
        );

        if self.is_focused && self.cursor_visible {
            let cursor_h = self.font_size * 1.2 * scale;
            let cursor_y = (center_y - self.font_size * 0.6) * scale;
            prims.push(RenderPrimitive::solid_rect(
                [cursor_screen_x, cursor_y, 2.0 * scale, cursor_h],
                theme.colors.cursor,
                1.0 * scale,
            ));
        }

        prims.push(RenderPrimitive::ClipPop);

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
    use crate::event::Modifiers;

    #[test]
    fn test_text_input_typing_and_navigation() {
        let mut input = TextInput::new("Placeholder");
        assert!(!input.is_focused);

        let r1 = input.on_key_event(&KeyEvent::KeyDown {
            key: "h".to_string(),
            keysym: 0,
            text: Some("h".to_string()),
            modifiers: Modifiers::default(),
        });
        assert_eq!(r1, EventResponse::Ignored);
        assert_eq!(input.text.get(), "");

        input.on_focus_change(true);
        assert!(input.is_focused);

        input.on_key_event(&KeyEvent::KeyDown {
            key: "H".to_string(),
            keysym: 0,
            text: Some("H".to_string()),
            modifiers: Modifiers::default(),
        });
        input.on_key_event(&KeyEvent::KeyDown {
            key: "i".to_string(),
            keysym: 0,
            text: Some("i".to_string()),
            modifiers: Modifiers::default(),
        });
        assert_eq!(input.text.get(), "Hi");
        assert_eq!(input.cursor_pos, 2);

        input.on_key_event(&KeyEvent::KeyDown {
            key: "Backspace".to_string(),
            keysym: 0xff08,
            text: None,
            modifiers: Modifiers::default(),
        });
        assert_eq!(input.text.get(), "H");
        assert_eq!(input.cursor_pos, 1);

        input.on_key_event(&KeyEvent::KeyDown {
            key: "Home".to_string(),
            keysym: 0xff50,
            text: None,
            modifiers: Modifiers::default(),
        });
        assert_eq!(input.cursor_pos, 0);

        input.on_key_event(&KeyEvent::KeyDown {
            key: "A".to_string(),
            keysym: 0,
            text: Some("A".to_string()),
            modifiers: Modifiers::default(),
        });
        assert_eq!(input.text.get(), "AH");
        assert_eq!(input.cursor_pos, 1);

        input.on_key_event(&KeyEvent::KeyDown {
            key: "Delete".to_string(),
            keysym: 0xffff,
            text: None,
            modifiers: Modifiers::default(),
        });
        assert_eq!(input.text.get(), "A");
    }

    #[test]
    fn test_text_input_selection_and_cut_copy_paste() {
        let mut input = TextInput::new("Placeholder");
        input.on_focus_change(true);
        input.text.set("Hello World".to_string());
        input.cursor_pos = 11;

        // Ctrl+A: Select all
        let r = input.on_key_event(&KeyEvent::KeyDown {
            key: "a".to_string(),
            keysym: 0x61,
            text: None,
            modifiers: Modifiers { ctrl: true, ..Default::default() },
        });
        assert_eq!(r, EventResponse::Handled);
        assert_eq!(input.selection, Some((0, 11)));
        assert_eq!(input.selected_text(), Some("Hello World".to_string()));
        assert_eq!(
            input.take_clipboard_action(),
            Some(ClipboardAction::SetPrimary("Hello World".to_string()))
        );

        // Ctrl+C: Copy
        let r = input.on_key_event(&KeyEvent::KeyDown {
            key: "c".to_string(),
            keysym: 0x63,
            text: None,
            modifiers: Modifiers { ctrl: true, ..Default::default() },
        });
        assert_eq!(r, EventResponse::Handled);
        assert_eq!(
            input.take_clipboard_action(),
            Some(ClipboardAction::SetClipboard("Hello World".to_string()))
        );

        // Ctrl+X: Cut
        let r = input.on_key_event(&KeyEvent::KeyDown {
            key: "x".to_string(),
            keysym: 0x78,
            text: None,
            modifiers: Modifiers { ctrl: true, ..Default::default() },
        });
        assert_eq!(r, EventResponse::Handled);
        assert_eq!(
            input.take_clipboard_action(),
            Some(ClipboardAction::SetClipboard("Hello World".to_string()))
        );
        assert_eq!(input.text.get(), "");
        assert_eq!(input.selection, None);
        assert_eq!(input.cursor_pos, 0);

        // Paste via on_paste
        input.on_paste("Wayland", false);
        assert_eq!(input.text.get(), "Wayland");
        assert_eq!(input.cursor_pos, 7);

        // Word boundaries
        let (s, e) = input.find_word_boundaries(3);
        assert_eq!((s, e), (0, 7));

        // Ctrl+V: Request paste action
        let r = input.on_key_event(&KeyEvent::KeyDown {
            key: "v".to_string(),
            keysym: 0x76,
            text: None,
            modifiers: Modifiers { ctrl: true, ..Default::default() },
        });
        assert_eq!(r, EventResponse::Handled);
        assert_eq!(
            input.take_clipboard_action(),
            Some(ClipboardAction::RequestPaste(
                crate::platform::clipboard::SelectionTarget::Clipboard
            ))
        );
    }

    #[test]
    fn test_text_input_word_boundary_detection() {
        let mut input = TextInput::new("");
        input.text.set("quick brown_fox 123!".to_string());

        assert_eq!(input.find_word_boundaries(2), (0, 5)); // "quick"
        assert_eq!(input.find_word_boundaries(5), (5, 6)); // " "
        assert_eq!(input.find_word_boundaries(8), (6, 15)); // "brown_fox"
        assert_eq!(input.find_word_boundaries(17), (16, 19)); // "123"
        assert_eq!(input.find_word_boundaries(19), (19, 20)); // "!"
    }

    #[test]
    fn test_text_input_empty_placeholder_click_and_type() {
        let mut input = TextInput::new("Source: Type, drag to select, Ctrl+C / Ctrl+X...");
        let rect = LayoutRect::new(0.0, 0.0, 320.0, 36.0);
        
        // Simulating pointer down in middle of empty input
        let r = input.on_pointer_event(
            &PointerEvent::Down {
                x: 100.0,
                y: 18.0,
                button: MouseButton::Left,
            },
            rect,
        );
        assert_eq!(r, EventResponse::Handled);
        assert_eq!(input.cursor_pos, 0);

        // Typing character into empty input must never panic
        let r = input.on_key_event(&KeyEvent::KeyDown {
            key: "a".to_string(),
            keysym: 0x61,
            text: Some("a".to_string()),
            modifiers: Modifiers::default(),
        });
        assert_eq!(r, EventResponse::Handled);
        assert_eq!(input.text.get(), "a");
        assert_eq!(input.cursor_pos, 1);
    }

    #[test]
    fn test_text_input_horizontal_scrolling_and_clipping() {
        let mut input = TextInput::new("Placeholder").with_width(120.0);
        input.on_focus_change(true);
        input.text.set("Very long string that significantly exceeds the visible area of the input".to_string());
        input.cursor_pos = 60;

        let theme = Theme::dark();
        let mut atlas = GlyphAtlas::new(512, 512, 1);
        let shaper = TextShaper::default_system_font().expect("Font available");
        let rasterizer = GlyphRasterizer::new(shaper.font_db().clone());
        let mut vector_atlas = VectorAtlas::new(512, 512);
        let rect = LayoutRect::new(0.0, 0.0, 120.0, 36.0);

        let prims = input.paint(
            rect,
            &theme,
            &mut atlas,
            &rasterizer,
            &shaper,
            &mut vector_atlas,
            1.0,
        );

        let has_clip_push = prims.iter().any(|p| matches!(p, RenderPrimitive::ClipPush { .. }));
        let has_clip_pop = prims.iter().any(|p| matches!(p, RenderPrimitive::ClipPop));
        assert!(has_clip_push, "Expected ClipPush primitive");
        assert!(has_clip_pop, "Expected ClipPop primitive");
        assert!(input.scroll_offset_x.get() > 0.0, "Expected horizontal scroll when cursor at end of long text");

        // Move to start
        input.cursor_pos = 0;
        let _ = input.paint(
            rect,
            &theme,
            &mut atlas,
            &rasterizer,
            &shaper,
            &mut vector_atlas,
            1.0,
        );
        assert_eq!(input.scroll_offset_x.get(), 0.0, "Expected scroll to reset when cursor at start");
    }
}
