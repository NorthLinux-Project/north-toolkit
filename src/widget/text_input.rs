//! Single-line interactive text input field with cursor blinking, text selection, and clipboard integration.

use std::any::Any;
use std::cell::{Cell, RefCell};
use std::time::Instant;

use super::icon::IconName;
use super::state::Property;
use super::tooltip::Tooltip;
use super::tree::{ClipboardAction, Widget};
use crate::event::{EventResponse, ImeEvent, KeyEvent, MouseButton, PointerEvent};
use crate::layout::{AlignItems, Dimension, EdgeInsets, FlexDirection, FlexStyle, LayoutRect, MeasureFunc};
use crate::render::vector::{StrokeStyle, VectorAtlas, VectorPath, VectorRasterizer};
use crate::render::{Border, CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{EditBuffer, GlyphRasterizer, TextShaper};
use crate::theme::Theme;

#[derive(Debug, Clone, PartialEq, Eq)]
struct PreeditState {
    text: String,
    cursor_begin: i32,
    cursor_end: i32,
}

fn byte_to_char_clamped(s: &str, byte_idx: usize) -> usize {
    let mut b = byte_idx.min(s.len());
    while b > 0 && !s.is_char_boundary(b) {
        b -= 1;
    }
    s[..b].chars().count()
}

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
    cached_cursor_rel_x: Cell<f32>,
    cached_shaper: RefCell<Option<TextShaper>>,
    /// Horizontal scroll offset for long lines exceeding input width.
    pub scroll_offset_x: Cell<f32>,
    pending_clipboard_action: Option<ClipboardAction>,
    preedit: Option<PreeditState>,
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
            cached_cursor_rel_x: Cell::new(0.0),
            cached_shaper: RefCell::new(None),
            scroll_offset_x: Cell::new(0.0),
            pending_clipboard_action: None,
            preedit: None,
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

    /// Returns the active IME preedit composition string, if any.
    pub fn preedit_text(&self) -> Option<&str> {
        self.preedit.as_ref().map(|p| p.text.as_str())
    }

    fn edit_buffer(&self) -> EditBuffer {
        let anchor = self.selection.map(|(a, _)| a);
        let focus = self.selection.map(|(_, f)| f).unwrap_or(self.cursor_pos);
        EditBuffer::with_state(self.text.get().clone(), focus, anchor)
    }

    fn apply_edit_buffer(&mut self, buf: EditBuffer) {
        self.cursor_pos = buf.cursor();
        self.selection = buf.selection_pair();
        let updated = buf.into_text();
        if self.text.get() != &updated {
            self.text.set(updated);
        }
    }

    fn reset_caret_blink(&mut self) {
        self.cursor_visible = true;
        self.cursor_timer_ms = 0.0;
    }

    /// Returns the normalized `(start, end)` character bounds of the active selection if non-empty.
    pub fn selection_range(&self) -> Option<(usize, usize)> {
        self.edit_buffer().selection_range()
    }

    /// Returns the text slice contained within the active selection.
    pub fn selected_text(&self) -> Option<String> {
        self.edit_buffer().selected_text()
    }

    /// Deletes the currently selected text characters from the input buffer.
    pub fn delete_selection(&mut self) -> bool {
        let mut buf = self.edit_buffer();
        let deleted = buf.delete_selection();
        self.apply_edit_buffer(buf);
        if deleted {
            self.reset_caret_blink();
        }
        deleted
    }

    /// Finds word boundaries `(start, end)` around `char_idx` for double-click word selection.
    pub fn find_word_boundaries(&self, char_idx: usize) -> (usize, usize) {
        self.edit_buffer().find_word_boundaries(char_idx)
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

    fn compose_display_string(&self) -> Option<(String, usize, usize, usize)> {
        let pre = self.preedit.as_ref()?;
        if pre.text.is_empty() {
            return None;
        }
        let buf = self.edit_buffer();
        let pre_start_char = buf.cursor().min(buf.char_len());
        let pre_byte = buf.char_to_byte(pre_start_char);
        let pre_char_len = pre.text.chars().count();
        let pre_end_char = pre_start_char + pre_char_len;
        let active_char = if pre.cursor_begin >= 0 {
            pre_start_char + byte_to_char_clamped(&pre.text, pre.cursor_begin as usize)
        } else {
            pre_end_char
        };
        let committed = buf.text();
        let mut combined = String::with_capacity(committed.len() + pre.text.len());
        combined.push_str(&committed[..pre_byte]);
        combined.push_str(&pre.text);
        combined.push_str(&committed[pre_byte..]);
        Some((combined, pre_start_char, pre_end_char, active_char))
    }

    fn resolve_cursor_rel_x(&self, text_area_w: f32) -> (f32, f32) {
        if self.cached_shaper.borrow().is_none() {
            if let Ok(shaper) = TextShaper::default_system_font() {
                *self.cached_shaper.borrow_mut() = Some(shaper);
            }
        }
        let shaper_ref = self.cached_shaper.borrow();
        let (cursor_rel_x, total_w) = if let Some((combined, _, _, active_char)) = self.compose_display_string() {
            let total_chars = combined.chars().count();
            if let Some(shaper) = shaper_ref.as_ref() {
                let shaped = shaper.shape(&combined, self.font_size);
                (shaped.x_for_char(active_char, total_chars), shaped.width)
            } else {
                let est = active_char as f32 * self.font_size * 0.55;
                (est, total_chars as f32 * self.font_size * 0.55)
            }
        } else if self.text.get().is_empty() {
            (0.0, 0.0)
        } else {
            let s = self.text.get();
            let total_chars = s.chars().count();
            let cur = self.cursor_pos.min(total_chars);
            if let Some(shaper) = shaper_ref.as_ref() {
                let shaped = shaper.shape(s, self.font_size);
                (shaped.x_for_char(cur, total_chars), shaped.width)
            } else {
                let est = cur as f32 * self.font_size * 0.55;
                (est, total_chars as f32 * self.font_size * 0.55)
            }
        };

        let mut scroll_x = self.scroll_offset_x.get();
        if total_w <= text_area_w {
            scroll_x = 0.0;
        } else {
            if cursor_rel_x - scroll_x > text_area_w - 6.0 {
                scroll_x = cursor_rel_x - (text_area_w - 6.0);
            } else if cursor_rel_x - scroll_x < 6.0 {
                scroll_x = (cursor_rel_x - 6.0).max(0.0);
            }
            scroll_x = scroll_x.clamp(0.0, (total_w - text_area_w + 12.0).max(0.0));
        }

        self.cached_cursor_rel_x.set(cursor_rel_x);
        self.scroll_offset_x.set(scroll_x);
        (cursor_rel_x, scroll_x)
    }
}

impl Widget for TextInput {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn measure(&self, shaper: &TextShaper) -> Option<MeasureFunc> {
        if self.cached_shaper.borrow().is_none() {
            *self.cached_shaper.borrow_mut() = Some(shaper.clone());
        }
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
        if self.is_focused && self.preedit.is_none() {
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
            self.preedit = None;
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
        self.preedit = None;
        let filtered: String = text
            .chars()
            .filter(|&c| !c.is_control() || c == '\t')
            .collect();
        if !filtered.is_empty() {
            let mut buf = self.edit_buffer();
            buf.insert(&filtered);
            self.apply_edit_buffer(buf);
        }
        self.reset_caret_blink();
    }

    fn on_ime_event(&mut self, event: &ImeEvent) -> EventResponse {
        if !self.is_focused {
            return EventResponse::Ignored;
        }
        match event {
            ImeEvent::Preedit {
                text,
                cursor_begin,
                cursor_end,
            } => {
                if text.is_empty() {
                    self.preedit = None;
                } else {
                    self.delete_selection();
                    self.preedit = Some(PreeditState {
                        text: text.clone(),
                        cursor_begin: *cursor_begin,
                        cursor_end: *cursor_end,
                    });
                }
                self.reset_caret_blink();
                EventResponse::Handled
            }
            ImeEvent::Commit(committed) => {
                self.preedit = None;
                let filtered: String = committed
                    .chars()
                    .filter(|&c| !c.is_control() || c == '\t')
                    .collect();
                if !filtered.is_empty() {
                    let mut buf = self.edit_buffer();
                    buf.insert(&filtered);
                    self.apply_edit_buffer(buf);
                }
                self.reset_caret_blink();
                EventResponse::Handled
            }
            ImeEvent::DeleteSurrounding { before, after } => {
                let mut buf = self.edit_buffer();
                buf.delete_surrounding_bytes(*before as usize, *after as usize);
                self.apply_edit_buffer(buf);
                self.reset_caret_blink();
                EventResponse::Handled
            }
        }
    }

    fn ime_cursor_rect(&self, rect: LayoutRect) -> Option<LayoutRect> {
        if !self.is_focused {
            return None;
        }
        let icon_offset = if self.leading_icon.is_some() { 24.0 } else { 0.0 };
        let text_area_x = rect.x + self.style.padding.left + icon_offset;
        let text_area_w = (rect.x + rect.width - self.style.padding.right - text_area_x).max(1.0);
        let (cursor_rel_x, scroll_x) = self.resolve_cursor_rel_x(text_area_w);
        let cursor_x = (text_area_x + cursor_rel_x - scroll_x)
            .clamp(text_area_x, text_area_x + text_area_w);
        let cursor_h = self.font_size * 1.2;
        let cursor_y = rect.y + rect.height * 0.5 - cursor_h * 0.5;
        Some(LayoutRect::new(cursor_x, cursor_y, 2.0, cursor_h))
    }

    fn has_ime_preedit(&self) -> bool {
        self.preedit.is_some()
    }

    fn on_pointer_event(&mut self, event: &PointerEvent, rect: LayoutRect) -> EventResponse {
        match *event {
            PointerEvent::Down { x, y, button } => {
                if rect.contains(x, y) {
                    if button == MouseButton::Left {
                        self.is_focused = true;
                        self.preedit = None;
                        self.reset_caret_blink();

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

                        let mut buf = self.edit_buffer();
                        let total_chars = buf.char_len();
                        let char_idx = self.char_index_from_x(x).min(total_chars);

                        if self.click_count == 2 {
                            let (w_start, w_end) = buf.find_word_boundaries(char_idx);
                            buf.set_selection(w_start, w_end);
                            if let Some(sel) = buf.selected_text() {
                                self.pending_clipboard_action = Some(ClipboardAction::SetPrimary(sel));
                            }
                        } else if self.click_count == 3 {
                            buf.select_all();
                            if let Some(sel) = buf.selected_text() {
                                self.pending_clipboard_action = Some(ClipboardAction::SetPrimary(sel));
                            }
                        } else {
                            buf.set_selection(char_idx, char_idx);
                            self.is_dragging_selection = true;
                        }
                        self.apply_edit_buffer(buf);
                        return EventResponse::Handled;
                    } else if button == MouseButton::Middle {
                        self.is_focused = true;
                        self.preedit = None;
                        self.reset_caret_blink();
                        let mut buf = self.edit_buffer();
                        let char_idx = self.char_index_from_x(x).min(buf.char_len());
                        buf.set_cursor(char_idx);
                        self.apply_edit_buffer(buf);
                        self.pending_clipboard_action = Some(ClipboardAction::RequestPaste(
                            crate::platform::clipboard::SelectionTarget::Primary,
                        ));
                        return EventResponse::Handled;
                    }
                }
            }
            PointerEvent::Move { x, .. } => {
                if self.is_dragging_selection {
                    let mut buf = self.edit_buffer();
                    let total_chars = buf.char_len();
                    let char_idx = self.char_index_from_x(x).min(total_chars);
                    let anchor = buf.anchor().unwrap_or(buf.cursor()).min(total_chars);
                    buf.set_selection(anchor, char_idx);
                    self.apply_edit_buffer(buf);
                    self.reset_caret_blink();
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Up { button, .. } => {
                if button == MouseButton::Left && self.is_dragging_selection {
                    self.is_dragging_selection = false;
                    let mut buf = self.edit_buffer();
                    if let Some((a, f)) = buf.selection_pair() {
                        if a == f {
                            buf.clear_selection();
                        } else if let Some(sel) = buf.selected_text() {
                            self.pending_clipboard_action = Some(ClipboardAction::SetPrimary(sel));
                        }
                    }
                    self.apply_edit_buffer(buf);
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
            KeyEvent::KeyDown {
                key,
                keysym,
                text,
                modifiers,
                is_repeat,
            } => {
                if self.preedit.is_some() {
                    return if crate::text::editing::is_modifier_keysym(key, *keysym) {
                        EventResponse::Ignored
                    } else {
                        EventResponse::Handled
                    };
                }

                let mut buf = self.edit_buffer();
                let total_chars = buf.char_len();

                if modifiers.ctrl && !modifiers.alt && !modifiers.logo {
                    if key == "a" || key == "A" || *keysym == 0x61 || *keysym == 0x41 {
                        if !*is_repeat {
                            buf.select_all();
                            if let Some(sel) = buf.selected_text() {
                                self.pending_clipboard_action =
                                    Some(ClipboardAction::SetPrimary(sel));
                            }
                            self.apply_edit_buffer(buf);
                            self.reset_caret_blink();
                        }
                        return EventResponse::Handled;
                    } else if key == "c" || key == "C" || *keysym == 0x63 || *keysym == 0x43 {
                        if !*is_repeat {
                            if let Some(sel) = buf.selected_text() {
                                self.pending_clipboard_action =
                                    Some(ClipboardAction::SetClipboard(sel));
                            }
                        }
                        return EventResponse::Handled;
                    } else if key == "x" || key == "X" || *keysym == 0x78 || *keysym == 0x58 {
                        if !*is_repeat {
                            if let Some(sel) = buf.selected_text() {
                                self.pending_clipboard_action =
                                    Some(ClipboardAction::SetClipboard(sel));
                                buf.delete_selection();
                                self.apply_edit_buffer(buf);
                                self.reset_caret_blink();
                            }
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
                    let changed = if modifiers.ctrl {
                        buf.delete_word_backward()
                    } else {
                        buf.backspace()
                    };
                    if changed {
                        self.apply_edit_buffer(buf);
                        self.reset_caret_blink();
                        return EventResponse::Handled;
                    }
                } else if key == "Delete" || *keysym == 0xffff {
                    let changed = if modifiers.ctrl {
                        buf.delete_word_forward()
                    } else {
                        buf.delete_forward()
                    };
                    if changed {
                        self.apply_edit_buffer(buf);
                        self.reset_caret_blink();
                        return EventResponse::Handled;
                    }
                } else if key == "Home" || *keysym == 0xff50 {
                    buf.move_to(0, modifiers.shift);
                    if modifiers.shift {
                        if let Some(sel) = buf.selected_text() {
                            self.pending_clipboard_action = Some(ClipboardAction::SetPrimary(sel));
                        }
                    }
                    self.apply_edit_buffer(buf);
                    self.reset_caret_blink();
                    return EventResponse::Handled;
                } else if key == "End" || *keysym == 0xff57 {
                    buf.move_to(total_chars, modifiers.shift);
                    if modifiers.shift {
                        if let Some(sel) = buf.selected_text() {
                            self.pending_clipboard_action = Some(ClipboardAction::SetPrimary(sel));
                        }
                    }
                    self.apply_edit_buffer(buf);
                    self.reset_caret_blink();
                    return EventResponse::Handled;
                } else if key == "Left" || *keysym == 0xff51 {
                    if modifiers.ctrl {
                        buf.move_word_left(modifiers.shift);
                    } else {
                        buf.move_left(modifiers.shift);
                    }
                    if modifiers.shift {
                        if let Some(sel) = buf.selected_text() {
                            self.pending_clipboard_action = Some(ClipboardAction::SetPrimary(sel));
                        }
                    }
                    self.apply_edit_buffer(buf);
                    self.reset_caret_blink();
                    return EventResponse::Handled;
                } else if key == "Right" || *keysym == 0xff53 {
                    if modifiers.ctrl {
                        buf.move_word_right(modifiers.shift);
                    } else {
                        buf.move_right(modifiers.shift);
                    }
                    if modifiers.shift {
                        if let Some(sel) = buf.selected_text() {
                            self.pending_clipboard_action = Some(ClipboardAction::SetPrimary(sel));
                        }
                    }
                    self.apply_edit_buffer(buf);
                    self.reset_caret_blink();
                    return EventResponse::Handled;
                } else if let Some(t) = text {
                    if !modifiers.ctrl && !modifiers.alt && !modifiers.logo {
                        let filtered: String = t.chars().filter(|c| !c.is_control()).collect();
                        if !filtered.is_empty() {
                            buf.insert(&filtered);
                            self.apply_edit_buffer(buf);
                            self.reset_caret_blink();
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
        vector_atlas: &mut VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        if self.cached_shaper.borrow().is_none() {
            *self.cached_shaper.borrow_mut() = Some(shaper.clone());
        }

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

        let composed = self.compose_display_string();
        let has_preedit = composed.is_some();
        let is_empty = self.text.get().is_empty() && !has_preedit;

        let display_text: &str = if let Some((ref combined, _, _, _)) = composed {
            combined.as_str()
        } else if is_empty {
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
        let display_chars = if is_empty {
            0
        } else {
            display_text.chars().count()
        };
        let char_positions = if is_empty {
            vec![0.0]
        } else {
            shaped.char_x_positions(display_chars)
        };

        let text_area_x = rect.x + self.style.padding.left + icon_offset;
        let text_area_w = (rect.x + rect.width - self.style.padding.right - text_area_x).max(0.0);
        let text_area_y = rect.y;
        let text_area_h = rect.height;

        let active_cursor_char = if let Some((_, _, _, active_char)) = composed {
            active_char.min(display_chars)
        } else {
            self.cursor_pos.min(display_chars)
        };

        let cursor_rel_x = if is_empty {
            0.0
        } else {
            char_positions
                .get(active_cursor_char)
                .copied()
                .unwrap_or(shaped.width)
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
        self.cached_cursor_rel_x.set(cursor_rel_x);

        let text_logical_x = text_area_x - scroll_x;
        self.cached_text_origin_x.set(text_logical_x);

        let cap_height = self.font_size * 0.70;
        let text_logical_baseline = center_y + cap_height * 0.5;

        if !has_preedit {
            *self.cached_glyph_positions.borrow_mut() = char_positions.clone();
        } else if self.text.get().is_empty() {
            *self.cached_glyph_positions.borrow_mut() = vec![0.0];
        } else {
            let committed_shaped = shaper.shape(self.text.get(), self.font_size);
            let committed_chars = self.text.get().chars().count();
            *self.cached_glyph_positions.borrow_mut() =
                committed_shaped.char_x_positions(committed_chars);
        }

        prims.push(RenderPrimitive::ClipPush {
            rect: [
                text_area_x * scale,
                text_area_y * scale,
                text_area_w * scale,
                text_area_h * scale,
            ],
        });

        if !is_empty && !has_preedit {
            if let Some((start, end)) = self.selection_range() {
                if start < end {
                    let x_start = char_positions.get(start).copied().unwrap_or(0.0);
                    let x_end = char_positions.get(end).copied().unwrap_or(shaped.width);
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

        if let Some((_, pre_start, pre_end, _)) = composed {
            let x_start = char_positions.get(pre_start).copied().unwrap_or(0.0);
            let x_end = char_positions.get(pre_end).copied().unwrap_or(shaped.width);
            let underline_y = (text_logical_baseline + 2.0) * scale;
            prims.push(RenderPrimitive::solid_rect(
                [
                    (text_logical_x + x_start) * scale,
                    underline_y,
                    ((x_end - x_start).max(1.0)) * scale,
                    1.5 * scale,
                ],
                theme.colors.text,
                0.5 * scale,
            ));

            if let Some(pre) = &self.preedit {
                if pre.cursor_begin >= 0 && pre.cursor_end > pre.cursor_begin {
                    let c_start =
                        pre_start + byte_to_char_clamped(&pre.text, pre.cursor_begin as usize);
                    let c_end =
                        pre_start + byte_to_char_clamped(&pre.text, pre.cursor_end as usize);
                    if c_start < c_end {
                        let cx_start = char_positions.get(c_start).copied().unwrap_or(x_start);
                        let cx_end = char_positions.get(c_end).copied().unwrap_or(x_end);
                        prims.push(RenderPrimitive::solid_rect(
                            [
                                (text_logical_x + cx_start) * scale,
                                underline_y,
                                ((cx_end - cx_start).max(1.0)) * scale,
                                2.5 * scale,
                            ],
                            theme.colors.primary,
                            0.5 * scale,
                        ));
                    }
                }
            }
        } else if self.is_focused && self.cursor_visible {
            let cursor_screen_x = (text_logical_x + cursor_rel_x) * scale;
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
            is_repeat: false,
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
            is_repeat: false,
        });
        input.on_key_event(&KeyEvent::KeyDown {
            key: "i".to_string(),
            keysym: 0,
            text: Some("i".to_string()),
            modifiers: Modifiers::default(),
            is_repeat: false,
        });
        assert_eq!(input.text.get(), "Hi");
        assert_eq!(input.cursor_pos, 2);

        input.on_key_event(&KeyEvent::KeyDown {
            key: "Backspace".to_string(),
            keysym: 0xff08,
            text: None,
            modifiers: Modifiers::default(),
            is_repeat: false,
        });
        assert_eq!(input.text.get(), "H");
        assert_eq!(input.cursor_pos, 1);

        input.on_key_event(&KeyEvent::KeyDown {
            key: "Home".to_string(),
            keysym: 0xff50,
            text: None,
            modifiers: Modifiers::default(),
            is_repeat: false,
        });
        assert_eq!(input.cursor_pos, 0);

        input.on_key_event(&KeyEvent::KeyDown {
            key: "A".to_string(),
            keysym: 0,
            text: Some("A".to_string()),
            modifiers: Modifiers::default(),
            is_repeat: false,
        });
        assert_eq!(input.text.get(), "AH");
        assert_eq!(input.cursor_pos, 1);

        input.on_key_event(&KeyEvent::KeyDown {
            key: "Delete".to_string(),
            keysym: 0xffff,
            text: None,
            modifiers: Modifiers::default(),
            is_repeat: false,
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
            is_repeat: false,
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
            is_repeat: false,
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
            is_repeat: false,
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
            is_repeat: false,
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
            is_repeat: false,
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

    #[test]
    fn test_text_input_ime_preedit_commit_delete_and_cursor_rect() {
        let mut input = TextInput::new("Placeholder").with_width(240.0);
        let rect = LayoutRect::new(10.0, 20.0, 240.0, 36.0);

        // Unfocused reports no IME cursor rectangle and ignores IME events
        assert!(input.ime_cursor_rect(rect).is_none());
        assert_eq!(
            input.on_ime_event(&ImeEvent::Commit("ignored".into())),
            EventResponse::Ignored
        );

        input.on_focus_change(true);
        let r0 = input.ime_cursor_rect(rect).expect("IME rect when focused");
        assert!(r0.x >= rect.x && r0.x <= rect.x + rect.width);

        // Preedit is displayed with underline, hides caret, and does not mutate committed text
        assert_eq!(
            input.on_ime_event(&ImeEvent::Preedit {
                text: "にほん".into(),
                cursor_begin: 9,
                cursor_end: 9,
            }),
            EventResponse::Handled
        );
        assert_eq!(input.text.get(), "");
        assert_eq!(input.preedit_text(), Some("にほん"));
        let r_pre = input.ime_cursor_rect(rect).expect("IME rect during preedit");
        assert!(r_pre.x > r0.x, "IME cursor rect should advance with preedit");

        let theme = Theme::dark();
        let mut atlas = GlyphAtlas::new(512, 512, 1);
        let shaper = TextShaper::default_system_font().expect("Font available");
        let rasterizer = GlyphRasterizer::new(shaper.font_db().clone());
        let mut vector_atlas = VectorAtlas::new(512, 512);
        let prims = input.paint(
            rect,
            &theme,
            &mut atlas,
            &rasterizer,
            &shaper,
            &mut vector_atlas,
            1.0,
        );
        let has_underline = prims.iter().any(|p| match p {
            RenderPrimitive::Rect { rect: r, border: None, use_texture: false, .. } => {
                (r[3] - 1.5).abs() < 0.01 && r[2] > 5.0
            }
            _ => false,
        });
        let has_caret = prims.iter().any(|p| match p {
            RenderPrimitive::Rect { rect: r, border: None, use_texture: false, .. } => {
                (r[2] - 2.0).abs() < 0.01 && r[3] > 10.0
            }
            _ => false,
        });
        assert!(has_underline, "Expected preedit underline primitive");
        assert!(!has_caret, "Caret must be hidden while IME preedit is active");

        // Commit replaces preedit and inserts into buffer
        assert_eq!(
            input.on_ime_event(&ImeEvent::Commit("日本語".into())),
            EventResponse::Handled
        );
        assert_eq!(input.preedit_text(), None);
        assert_eq!(input.text.get(), "日本語");
        assert_eq!(input.cursor_pos, 3);

        // DeleteSurrounding removes 3 UTF-8 bytes before cursor ('語')
        assert_eq!(
            input.on_ime_event(&ImeEvent::DeleteSurrounding {
                before: 3,
                after: 0,
            }),
            EventResponse::Handled
        );
        assert_eq!(input.text.get(), "日本");
        assert_eq!(input.cursor_pos, 2);
    }

    #[test]
    fn test_text_input_preedit_suppresses_raw_keydown_and_noop_paste_preserves_selection() {
        let mut input = TextInput::new("");
        input.text.set("hello".into());
        input.on_focus_change(true);
        input.cursor_pos = 5;

        input.on_ime_event(&ImeEvent::Preedit {
            text: "か".into(),
            cursor_begin: 3,
            cursor_end: 3,
        });
        assert!(input.has_ime_preedit());

        assert_eq!(
            input.on_key_event(&KeyEvent::KeyDown {
                key: "Left".into(),
                keysym: 0xff51,
                text: None,
                modifiers: Modifiers::default(),
                is_repeat: false,
            }),
            EventResponse::Handled
        );
        assert_eq!(input.cursor_pos, 5);
        assert_eq!(input.text.get(), "hello");

        assert_eq!(
            input.on_key_event(&KeyEvent::KeyDown {
                key: "a".into(),
                keysym: 0x61,
                text: Some("a".into()),
                modifiers: Modifiers::default(),
                is_repeat: false,
            }),
            EventResponse::Handled
        );
        assert_eq!(input.text.get(), "hello");

        assert_eq!(
            input.on_key_event(&KeyEvent::KeyDown {
                key: "Shift_L".into(),
                keysym: 0xffe1,
                text: None,
                modifiers: Modifiers {
                    shift: true,
                    ..Default::default()
                },
                is_repeat: false,
            }),
            EventResponse::Ignored
        );

        input.on_ime_event(&ImeEvent::Preedit {
            text: String::new(),
            cursor_begin: 0,
            cursor_end: 0,
        });
        assert!(!input.has_ime_preedit());

        input.selection = Some((0, 5));
        input.cursor_pos = 5;
        input.on_paste("\n\r", false);
        assert_eq!(input.text.get(), "hello");
        assert_eq!(input.selection_range(), Some((0, 5)));

        let ctrl = Modifiers {
            ctrl: true,
            ..Default::default()
        };
        input.on_key_event(&KeyEvent::KeyDown {
            key: "c".into(),
            keysym: 0x63,
            text: None,
            modifiers: ctrl,
            is_repeat: true,
        });
        assert_eq!(input.take_clipboard_action(), None);
        input.on_key_event(&KeyEvent::KeyDown {
            key: "c".into(),
            keysym: 0x63,
            text: None,
            modifiers: ctrl,
            is_repeat: false,
        });
        assert_eq!(
            input.take_clipboard_action(),
            Some(ClipboardAction::SetClipboard("hello".into()))
        );
    }
}
