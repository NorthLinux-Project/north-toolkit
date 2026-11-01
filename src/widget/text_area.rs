//! Multiline interactive text editor with soft word wrapping, vertical scrolling, selection, and Wayland IME support.

use std::any::Any;
use std::cell::{Cell, RefCell};
use std::ops::Range;
use std::time::Instant;

use super::state::Property;
use super::tooltip::Tooltip;
use super::tree::{ClipboardAction, Widget};
use crate::event::{EventResponse, ImeEvent, KeyEvent, MouseButton, PointerEvent};
use crate::layout::{
    AlignItems, Dimension, EdgeInsets, FlexDirection, FlexStyle, LayoutRect, MeasureFunc, Overflow,
};
use crate::render::vector::VectorAtlas;
use crate::render::{Border, CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{EditBuffer, GlyphRasterizer, ShapedText, TextShaper};
use crate::theme::Theme;

#[derive(Debug, Clone, PartialEq, Eq)]
struct PreeditState {
    text: String,
    cursor_begin: i32,
    cursor_end: i32,
}

/// Cached visual line produced by hard newline splitting and soft word/character wrapping.
#[derive(Clone)]
pub struct LineLayout {
    /// UTF-8 byte range within the shaped source string.
    pub byte_range: Range<usize>,
    /// Start character index within the shaped source string.
    pub char_start: usize,
    /// End character index within the shaped source string (excluding `\n`).
    pub char_end: usize,
    /// `true` if this visual line wrapped softly rather than ending at `\n` or end-of-text.
    pub ends_with_soft_wrap: bool,
    /// `true` if this soft-wrapped visual line ends with trailing whitespace rather than mid-word.
    pub ends_with_whitespace: bool,
    /// Shaped glyph run for this visual line.
    pub shaped: ShapedText,
    /// Logical horizontal offsets for character boundaries `0..=(char_end - char_start)`.
    pub char_x: Vec<f32>,
    /// Unscrolled logical Y coordinate relative to the content origin.
    pub y: f32,
}

#[derive(Default)]
struct LayoutCache {
    valid: bool,
    text: String,
    font_size_bits: u32,
    line_height_bits: u32,
    max_width_bits: u32,
    lines: Vec<LineLayout>,
}

fn byte_to_char_clamped(s: &str, byte_idx: usize) -> usize {
    let mut b = byte_idx.min(s.len());
    while b > 0 && !s.is_char_boundary(b) {
        b -= 1;
    }
    s[..b].chars().count()
}

fn compute_wrapped_lines(
    text: &str,
    max_width: f32,
    font_size: f32,
    line_height: f32,
    shaper: &TextShaper,
) -> Vec<LineLayout> {
    let mut lines = Vec::new();
    let mut byte_offset = 0usize;
    let mut char_offset = 0usize;

    for para in text.split('\n') {
        let para_bytes = para.len();
        let para_chars: Vec<(usize, char)> = para.char_indices().collect();
        let para_char_len = para_chars.len();

        let rel_char_to_byte = |rel_c: usize| -> usize {
            if rel_c < para_char_len {
                para_chars[rel_c].0
            } else {
                para_bytes
            }
        };

        if para_char_len == 0 {
            let y = lines.len() as f32 * line_height;
            lines.push(LineLayout {
                byte_range: byte_offset..byte_offset,
                char_start: char_offset,
                char_end: char_offset,
                ends_with_soft_wrap: false,
                ends_with_whitespace: false,
                shaped: shaper.shape("", font_size),
                char_x: vec![0.0],
                y,
            });
        } else {
            let full_shaped = shaper.shape(para, font_size);
            if max_width <= 0.0 || full_shaped.width <= max_width || para_char_len == 1 {
                let y = lines.len() as f32 * line_height;
                let char_x = full_shaped.char_x_positions(para_char_len);
                lines.push(LineLayout {
                    byte_range: byte_offset..(byte_offset + para_bytes),
                    char_start: char_offset,
                    char_end: char_offset + para_char_len,
                    ends_with_soft_wrap: false,
                    ends_with_whitespace: false,
                    shaped: full_shaped,
                    char_x,
                    y,
                });
            } else {
                let para_x = full_shaped.char_x_positions(para_char_len);
                let mut rel_start = 0usize;

                while rel_start < para_char_len {
                    let rem_b_start = rel_char_to_byte(rel_start);
                    if para_x[para_char_len] - para_x[rel_start] <= max_width {
                        let rem_slice = &para[rem_b_start..para_bytes];
                        let rem_shaped = shaper.shape(rem_slice, font_size);
                        if rem_shaped.width <= max_width || para_char_len - rel_start <= 1 {
                            let rem_chars = para_char_len - rel_start;
                            let char_x = rem_shaped.char_x_positions(rem_chars);
                            let y = lines.len() as f32 * line_height;
                            lines.push(LineLayout {
                                byte_range: (byte_offset + rem_b_start)..(byte_offset + para_bytes),
                                char_start: char_offset + rel_start,
                                char_end: char_offset + para_char_len,
                                ends_with_soft_wrap: false,
                                ends_with_whitespace: false,
                                shaped: rem_shaped,
                                char_x,
                                y,
                            });
                            break;
                        }
                    }

                    let start_x = para_x[rel_start];
                    let mut fit_end = rel_start + 1;
                    while fit_end < para_char_len && (para_x[fit_end + 1] - start_x) <= max_width {
                        fit_end += 1;
                    }

                    let word_break = ((rel_start + 1)..=fit_end)
                        .rev()
                        .find(|&k| para_chars[k - 1].1.is_whitespace());

                    let mut break_at = if let Some(wb) = word_break {
                        wb
                    } else if fit_end < para_char_len && para_chars[fit_end].1.is_whitespace() {
                        fit_end + 1
                    } else {
                        fit_end
                    };

                    if word_break.is_none() && break_at == fit_end && break_at > rel_start + 1 {
                        while break_at > rel_start + 1 {
                            let test_slice = &para[rem_b_start..rel_char_to_byte(break_at)];
                            if shaper.shape(test_slice, font_size).width <= max_width {
                                break;
                            }
                            break_at -= 1;
                        }
                    }

                    let b_end = rel_char_to_byte(break_at);
                    let line_slice = &para[rem_b_start..b_end];
                    let shaped = shaper.shape(line_slice, font_size);
                    let seg_chars = break_at - rel_start;
                    let char_x = shaped.char_x_positions(seg_chars);
                    let y = lines.len() as f32 * line_height;
                    let ends_with_soft_wrap = break_at < para_char_len;
                    let ends_with_whitespace = ends_with_soft_wrap
                        && break_at > rel_start
                        && para_chars[break_at - 1].1.is_whitespace();

                    lines.push(LineLayout {
                        byte_range: (byte_offset + rem_b_start)..(byte_offset + b_end),
                        char_start: char_offset + rel_start,
                        char_end: char_offset + break_at,
                        ends_with_soft_wrap,
                        ends_with_whitespace,
                        shaped,
                        char_x,
                        y,
                    });

                    rel_start = break_at;
                }
            }
        }

        byte_offset += para_bytes + 1;
        char_offset += para_char_len + 1;
    }

    if lines.is_empty() {
        lines.push(LineLayout {
            byte_range: 0..0,
            char_start: 0,
            char_end: 0,
            ends_with_soft_wrap: false,
            ends_with_whitespace: false,
            shaped: shaper.shape("", font_size),
            char_x: vec![0.0],
            y: 0.0,
        });
    }

    lines
}

fn line_and_x_for_char(
    lines: &[LineLayout],
    char_idx: usize,
    upstream_affinity: bool,
) -> (usize, f32) {
    if lines.is_empty() {
        return (0, 0.0);
    }
    for (idx, line) in lines.iter().enumerate() {
        let in_line = if line.ends_with_soft_wrap && line.char_end > line.char_start {
            if !line.ends_with_whitespace && upstream_affinity {
                char_idx >= line.char_start && char_idx <= line.char_end
            } else {
                char_idx >= line.char_start && char_idx < line.char_end
            }
        } else {
            char_idx >= line.char_start && char_idx <= line.char_end
        };
        if in_line {
            let rel = char_idx - line.char_start;
            let x = line.char_x.get(rel).copied().unwrap_or(line.shaped.width);
            return (idx, x);
        }
    }
    let last_idx = lines.len() - 1;
    (last_idx, lines[last_idx].shaped.width)
}

fn char_and_affinity_for_x_in_line(line: &LineLayout, rel_x: f32) -> (usize, bool) {
    let line_chars = line.char_end.saturating_sub(line.char_start);
    if line_chars == 0 || rel_x <= 0.0 {
        return (line.char_start, false);
    }
    if line.ends_with_soft_wrap && line.ends_with_whitespace {
        let max_col = line_chars.saturating_sub(1);
        for i in 0..max_col {
            let left = line.char_x.get(i).copied().unwrap_or(0.0);
            let right = line.char_x.get(i + 1).copied().unwrap_or(left);
            if rel_x < (left + right) * 0.5 {
                return (line.char_start + i, false);
            }
        }
        return (line.char_start + max_col, false);
    }
    for i in 0..line_chars {
        let left = line.char_x.get(i).copied().unwrap_or(0.0);
        let right = line.char_x.get(i + 1).copied().unwrap_or(left);
        if rel_x < (left + right) * 0.5 {
            return (line.char_start + i, false);
        }
    }
    (line.char_end, line.ends_with_soft_wrap)
}

/// Multiline interactive text editor widget with soft word wrapping, vertical scrolling, and IME support.
pub struct TextArea {
    /// Reactive string property synchronized with the internal edit buffer.
    pub text: Property<String>,
    /// Placeholder text rendered when the buffer and preedit are empty.
    pub placeholder: String,
    /// Font size in logical points.
    pub font_size: f32,
    /// Line height in logical points.
    pub line_height: f32,
    /// Flexbox layout style.
    pub style: FlexStyle,
    /// Whether this editor currently holds keyboard focus.
    pub is_focused: bool,
    /// Whether the user is actively dragging a text selection.
    pub is_dragging_selection: bool,
    /// Optional custom corner radii override.
    pub corner_radii: Option<CornerRadii>,
    /// Vertical scroll offset in logical points.
    pub scroll_y: Cell<f32>,
    /// Optional hover tooltip.
    pub tooltip: Option<Tooltip>,
    buffer: EditBuffer,
    upstream_affinity: bool,
    preferred_x: Option<f32>,
    cursor_timer_ms: f32,
    cursor_visible: bool,
    last_click_instant: Option<Instant>,
    click_count: u8,
    pending_clipboard_action: Option<ClipboardAction>,
    preedit: Option<PreeditState>,
    scroll_to_cursor_pending: Cell<bool>,
    cached_rect: Cell<LayoutRect>,
    cached_shaper: RefCell<Option<TextShaper>>,
    committed_cache: RefCell<LayoutCache>,
    display_cache: RefCell<LayoutCache>,
    placeholder_cache: RefCell<LayoutCache>,
}

impl TextArea {
    /// Creates a new multiline text editor with the specified placeholder string.
    pub fn new<S: Into<String>>(placeholder: S) -> Self {
        let default_w = 340.0;
        let default_h = 120.0;
        Self {
            text: Property::new(String::new()),
            placeholder: placeholder.into(),
            font_size: 14.0,
            line_height: 20.0,
            style: FlexStyle {
                direction: FlexDirection::Column,
                align_items: AlignItems::Stretch,
                padding: EdgeInsets::symmetric(10.0, 12.0),
                width: Dimension::Points(default_w),
                height: Dimension::Points(default_h),
                min_width: Some(80.0),
                min_height: Some(72.0),
                overflow: Overflow::Hidden,
                ..Default::default()
            },
            is_focused: false,
            is_dragging_selection: false,
            corner_radii: None,
            scroll_y: Cell::new(0.0),
            tooltip: None,
            buffer: EditBuffer::new(),
            upstream_affinity: false,
            preferred_x: None,
            cursor_timer_ms: 0.0,
            cursor_visible: true,
            last_click_instant: None,
            click_count: 0,
            pending_clipboard_action: None,
            preedit: None,
            scroll_to_cursor_pending: Cell::new(false),
            cached_rect: Cell::new(LayoutRect::new(0.0, 0.0, default_w, default_h)),
            cached_shaper: RefCell::new(None),
            committed_cache: RefCell::new(LayoutCache::default()),
            display_cache: RefCell::new(LayoutCache::default()),
            placeholder_cache: RefCell::new(LayoutCache::default()),
        }
    }

    /// Initializes the editor with starting text and places the cursor at the end.
    pub fn with_text<S: Into<String>>(mut self, text: S) -> Self {
        let s = text.into();
        self.buffer = EditBuffer::from_text(s.clone());
        self.text = Property::new(s);
        self
    }

    /// Sets explicit width and height in logical points.
    pub fn with_size(mut self, width: f32, height: f32) -> Self {
        self.style.width = Dimension::Points(width);
        self.style.height = Dimension::Points(height);
        let prev = self.cached_rect.get();
        self.cached_rect
            .set(LayoutRect::new(prev.x, prev.y, width, height));
        self
    }

    /// Sets explicit width in logical points.
    pub fn with_width(mut self, width: f32) -> Self {
        self.style.width = Dimension::Points(width);
        let prev = self.cached_rect.get();
        self.cached_rect
            .set(LayoutRect::new(prev.x, prev.y, width, prev.height));
        self
    }

    /// Sets explicit height in logical points.
    pub fn with_height(mut self, height: f32) -> Self {
        self.style.height = Dimension::Points(height);
        let prev = self.cached_rect.get();
        self.cached_rect
            .set(LayoutRect::new(prev.x, prev.y, prev.width, height));
        self
    }

    /// Sets the font size and proportionally updates default line height.
    pub fn with_font_size(mut self, font_size: f32) -> Self {
        self.font_size = font_size;
        self.line_height = (font_size * 1.45).round().max(font_size + 2.0);
        self
    }

    /// Overrides the line height in logical points.
    pub fn with_line_height(mut self, line_height: f32) -> Self {
        self.line_height = line_height.max(1.0);
        self
    }

    /// Overrides corner radii for the editor surface and border.
    pub fn with_corner_radii(mut self, radii: impl Into<CornerRadii>) -> Self {
        self.corner_radii = Some(radii.into());
        self
    }

    /// Attaches a hover tooltip to the editor.
    pub fn with_tooltip<S: Into<String>>(mut self, text: S) -> Self {
        self.tooltip = Some(Tooltip::new(text));
        self
    }

    /// Returns the current character cursor position (`0..=char_len`).
    pub fn cursor_pos(&self) -> usize {
        if self.buffer.text() == self.text.get() {
            self.buffer.cursor()
        } else {
            self.buffer.cursor().min(self.text.get().chars().count())
        }
    }

    /// Moves the cursor to `pos` (clamped) and clears any active selection.
    pub fn set_cursor_pos(&mut self, pos: usize) {
        self.sync_from_property();
        self.buffer.set_cursor(pos);
        self.upstream_affinity = false;
        self.preferred_x = None;
        self.scroll_to_cursor_pending.set(true);
    }

    /// Returns the active IME preedit composition string, if any.
    pub fn preedit_text(&self) -> Option<&str> {
        self.preedit.as_ref().map(|p| p.text.as_str())
    }

    /// Returns the normalized `(start, end)` character bounds of any active selection.
    pub fn selection_range(&self) -> Option<(usize, usize)> {
        if self.buffer.text() == self.text.get() {
            self.buffer.selection_range()
        } else {
            None
        }
    }

    /// Returns the selected substring if a non-empty selection is active.
    pub fn selected_text(&self) -> Option<String> {
        if self.buffer.text() == self.text.get() {
            self.buffer.selected_text()
        } else {
            None
        }
    }

    /// Deletes the active selection and returns `true` if characters were removed.
    pub fn delete_selection(&mut self) -> bool {
        self.sync_from_property();
        let deleted = self.buffer.delete_selection();
        if deleted {
            self.sync_to_property();
            self.upstream_affinity = false;
            self.preferred_x = None;
            self.reset_caret_blink();
            self.scroll_to_cursor_pending.set(true);
        }
        deleted
    }

    /// Finds word boundaries `(start, end)` around `char_idx`.
    pub fn find_word_boundaries(&self, char_idx: usize) -> (usize, usize) {
        if self.buffer.text() == self.text.get() {
            self.buffer.find_word_boundaries(char_idx)
        } else {
            EditBuffer::from_text(self.text.get().clone()).find_word_boundaries(char_idx)
        }
    }

    /// Explicitly computes/updates cached wrapped lines for `max_width` using `shaper`.
    pub fn layout_lines(&self, shaper: &TextShaper, max_width: f32) {
        if self.cached_shaper.borrow().is_none() {
            *self.cached_shaper.borrow_mut() = Some(shaper.clone());
        }
        self.ensure_committed_cache(shaper, max_width);
    }

    /// Returns the wrapped visual line strings from the current committed text layout.
    pub fn visual_lines(&self) -> Vec<String> {
        let shaper = self.ensure_shaper();
        let content_w = self.content_width(self.cached_rect.get());
        if let Some(ref s) = shaper {
            self.ensure_committed_cache(s, content_w);
        }
        let cache = self.committed_cache.borrow();
        let src = self.text.get();
        cache
            .lines
            .iter()
            .map(|l| src.get(l.byte_range.clone()).unwrap_or("").to_string())
            .collect()
    }

    /// Maps logical surface coordinates `(x, y)` inside `rect` to a character index in the buffer.
    pub fn char_index_at(&self, x: f32, y: f32, rect: LayoutRect) -> usize {
        self.char_index_and_affinity_at(x, y, rect).0
    }

    fn char_index_and_affinity_at(&self, x: f32, y: f32, rect: LayoutRect) -> (usize, bool) {
        self.cached_rect.set(rect);
        let content_x = rect.x + self.style.padding.left;
        let content_y = rect.y + self.style.padding.top;
        let content_w = self.content_width(rect);

        let shaper = self.ensure_shaper();
        if let Some(ref s) = shaper {
            self.ensure_committed_cache(s, content_w);
        }
        let cache = self.committed_cache.borrow();
        if cache.lines.is_empty() {
            return (0, false);
        }

        let sy = self.scroll_y.get();
        if y < rect.y && sy <= 0.0 {
            return (0, false);
        }
        let raw_local_y = y - content_y + sy;
        let total_lines_h = cache.lines.len() as f32 * self.line_height;
        if raw_local_y >= total_lines_h {
            return (
                cache.lines.last().map(|l| l.char_end).unwrap_or(0),
                false,
            );
        }

        let local_y = raw_local_y.max(0.0);
        let line_idx = ((local_y / self.line_height).floor() as usize)
            .min(cache.lines.len().saturating_sub(1));
        let rel_x = x - content_x;
        char_and_affinity_for_x_in_line(&cache.lines[line_idx], rel_x)
    }

    fn sync_from_property(&mut self) {
        if self.buffer.text() != self.text.get() {
            self.buffer.set_text(self.text.get().clone());
            self.upstream_affinity = false;
            self.preferred_x = None;
        }
    }

    fn sync_to_property(&mut self) {
        if self.text.get() != self.buffer.text() {
            self.text.set(self.buffer.text().to_string());
        }
    }

    fn reset_caret_blink(&mut self) {
        self.cursor_visible = true;
        self.cursor_timer_ms = 0.0;
    }

    fn content_width(&self, rect: LayoutRect) -> f32 {
        (rect.width - self.style.padding.horizontal()).max(1.0)
    }

    fn content_height(&self, rect: LayoutRect) -> f32 {
        (rect.height - self.style.padding.vertical()).max(self.line_height)
    }

    fn ensure_shaper(&self) -> Option<TextShaper> {
        if self.cached_shaper.borrow().is_none() {
            if let Ok(shaper) = TextShaper::default_system_font() {
                *self.cached_shaper.borrow_mut() = Some(shaper);
            }
        }
        self.cached_shaper.borrow().clone()
    }

    fn update_cache_entry(
        cache: &mut LayoutCache,
        text: &str,
        font_size: f32,
        line_height: f32,
        max_width: f32,
        shaper: &TextShaper,
    ) {
        let quantized_w = (max_width.max(1.0) * 4.0).round() / 4.0;
        let fs_bits = font_size.to_bits();
        let lh_bits = line_height.to_bits();
        let mw_bits = quantized_w.to_bits();

        if cache.valid
            && cache.text == text
            && cache.font_size_bits == fs_bits
            && cache.line_height_bits == lh_bits
            && cache.max_width_bits == mw_bits
        {
            return;
        }

        cache.lines = compute_wrapped_lines(text, quantized_w, font_size, line_height, shaper);
        cache.text.clear();
        cache.text.push_str(text);
        cache.font_size_bits = fs_bits;
        cache.line_height_bits = lh_bits;
        cache.max_width_bits = mw_bits;
        cache.valid = true;
    }

    fn ensure_committed_cache(&self, shaper: &TextShaper, max_width: f32) {
        let mut cache = self.committed_cache.borrow_mut();
        Self::update_cache_entry(
            &mut cache,
            self.text.get(),
            self.font_size,
            self.line_height,
            max_width,
            shaper,
        );
    }

    fn compose_display_string(&self) -> Option<(String, usize, usize, usize)> {
        let pre = self.preedit.as_ref()?;
        if pre.text.is_empty() {
            return None;
        }
        let committed = self.text.get();
        let committed_len = committed.chars().count();
        let pre_start_char = self.cursor_pos().min(committed_len);
        let pre_byte = committed
            .char_indices()
            .nth(pre_start_char)
            .map(|(b, _)| b)
            .unwrap_or(committed.len());
        let pre_char_len = pre.text.chars().count();
        let pre_end_char = pre_start_char + pre_char_len;
        let active_char = if pre.cursor_begin >= 0 {
            pre_start_char + byte_to_char_clamped(&pre.text, pre.cursor_begin as usize)
        } else {
            pre_end_char
        };
        let mut combined = String::with_capacity(committed.len() + pre.text.len());
        combined.push_str(&committed[..pre_byte]);
        combined.push_str(&pre.text);
        combined.push_str(&committed[pre_byte..]);
        Some((combined, pre_start_char, pre_end_char, active_char))
    }

    fn sync_scroll_for_cursor(
        &self,
        lines: &[LineLayout],
        active_cursor_char: usize,
        upstream_affinity: bool,
        viewport_h: f32,
    ) -> (usize, f32, f32) {
        let (line_idx, cursor_x) =
            line_and_x_for_char(lines, active_cursor_char, upstream_affinity);
        let line_y = lines
            .get(line_idx)
            .map(|l| l.y)
            .unwrap_or(line_idx as f32 * self.line_height);
        let total_h = (lines.len().max(1) as f32) * self.line_height;
        let max_scroll = (total_h - viewport_h).max(0.0);
        let mut sy = self.scroll_y.get().clamp(0.0, max_scroll);

        if self.scroll_to_cursor_pending.get() {
            if line_y < sy {
                sy = line_y;
            } else if line_y + self.line_height > sy + viewport_h {
                sy = line_y + self.line_height - viewport_h;
            }
            sy = sy.clamp(0.0, max_scroll);
            self.scroll_to_cursor_pending.set(false);
        }

        self.scroll_y.set(sy);
        (line_idx, cursor_x, sy)
    }

    fn apply_scroll_now(&self) {
        let rect = self.cached_rect.get();
        let content_w = self.content_width(rect);
        let viewport_h = self.content_height(rect);
        let Some(shaper) = self.ensure_shaper() else {
            return;
        };
        if let Some((combined, _, _, active_char)) = self.compose_display_string() {
            let mut dcache = self.display_cache.borrow_mut();
            Self::update_cache_entry(
                &mut dcache,
                &combined,
                self.font_size,
                self.line_height,
                content_w,
                &shaper,
            );
            let _ = self.sync_scroll_for_cursor(&dcache.lines, active_char, false, viewport_h);
        } else {
            self.ensure_committed_cache(&shaper, content_w);
            let cache = self.committed_cache.borrow();
            let _ = self.sync_scroll_for_cursor(
                &cache.lines,
                self.cursor_pos(),
                self.upstream_affinity,
                viewport_h,
            );
        }
    }

    fn emit_primary_selection_if_any(&mut self) {
        if let Some(sel) = self.buffer.selected_text() {
            self.pending_clipboard_action = Some(ClipboardAction::SetPrimary(sel));
        }
    }
}

impl Widget for TextArea {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn measure(&self, shaper: &TextShaper) -> Option<MeasureFunc> {
        if self.cached_shaper.borrow().is_none() {
            *self.cached_shaper.borrow_mut() = Some(shaper.clone());
        }
        None
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
            self.sync_from_property();
            self.buffer.clear_selection();
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

    fn is_focusable(&self) -> bool {
        true
    }

    fn on_paste(&mut self, text: &str, _is_primary: bool) {
        if !self.is_focused {
            return;
        }
        self.sync_from_property();
        self.preedit = None;
        let normalized = text.replace("\r\n", "\n").replace('\r', "\n");
        let filtered: String = normalized
            .chars()
            .filter(|&c| !c.is_control() || c == '\n' || c == '\t')
            .collect();
        if !filtered.is_empty() {
            self.buffer.insert(&filtered);
            self.sync_to_property();
            self.upstream_affinity = false;
            self.preferred_x = None;
            self.scroll_to_cursor_pending.set(true);
            self.apply_scroll_now();
        }
        self.reset_caret_blink();
    }

    fn on_ime_event(&mut self, event: &ImeEvent) -> EventResponse {
        if !self.is_focused {
            return EventResponse::Ignored;
        }
        self.sync_from_property();
        match event {
            ImeEvent::Preedit {
                text,
                cursor_begin,
                cursor_end,
            } => {
                if text.is_empty() {
                    self.preedit = None;
                } else {
                    if self.buffer.delete_selection() {
                        self.sync_to_property();
                    }
                    self.preedit = Some(PreeditState {
                        text: text.clone(),
                        cursor_begin: *cursor_begin,
                        cursor_end: *cursor_end,
                    });
                }
                self.upstream_affinity = false;
                self.preferred_x = None;
                self.reset_caret_blink();
                self.scroll_to_cursor_pending.set(true);
                self.apply_scroll_now();
                EventResponse::Handled
            }
            ImeEvent::Commit(committed) => {
                self.preedit = None;
                let normalized = committed.replace("\r\n", "\n").replace('\r', "\n");
                let filtered: String = normalized
                    .chars()
                    .filter(|&c| !c.is_control() || c == '\n' || c == '\t')
                    .collect();
                if !filtered.is_empty() {
                    self.buffer.insert(&filtered);
                    self.sync_to_property();
                }
                self.upstream_affinity = false;
                self.preferred_x = None;
                self.reset_caret_blink();
                self.scroll_to_cursor_pending.set(true);
                self.apply_scroll_now();
                EventResponse::Handled
            }
            ImeEvent::DeleteSurrounding { before, after } => {
                if self
                    .buffer
                    .delete_surrounding_bytes(*before as usize, *after as usize)
                {
                    self.sync_to_property();
                }
                self.upstream_affinity = false;
                self.preferred_x = None;
                self.reset_caret_blink();
                self.scroll_to_cursor_pending.set(true);
                self.apply_scroll_now();
                EventResponse::Handled
            }
        }
    }

    fn ime_cursor_rect(&self, rect: LayoutRect) -> Option<LayoutRect> {
        if !self.is_focused {
            return None;
        }
        self.cached_rect.set(rect);
        let content_x = rect.x + self.style.padding.left;
        let content_y = rect.y + self.style.padding.top;
        let content_w = self.content_width(rect);
        let viewport_h = self.content_height(rect);

        let Some(shaper) = self.ensure_shaper() else {
            return Some(LayoutRect::new(content_x, content_y, 2.0, self.line_height));
        };

        let (line_idx, cursor_x, sy) =
            if let Some((combined, _, _, active_char)) = self.compose_display_string() {
                let mut dcache = self.display_cache.borrow_mut();
                Self::update_cache_entry(
                    &mut dcache,
                    &combined,
                    self.font_size,
                    self.line_height,
                    content_w,
                    &shaper,
                );
                self.sync_scroll_for_cursor(&dcache.lines, active_char, false, viewport_h)
            } else {
                self.ensure_committed_cache(&shaper, content_w);
                let cache = self.committed_cache.borrow();
                self.sync_scroll_for_cursor(
                    &cache.lines,
                    self.cursor_pos(),
                    self.upstream_affinity,
                    viewport_h,
                )
            };

        let logical_x = (content_x + cursor_x).clamp(content_x, content_x + content_w);
        let logical_y = (content_y + line_idx as f32 * self.line_height - sy)
            .clamp(rect.y, (rect.y + rect.height - self.line_height).max(rect.y));
        Some(LayoutRect::new(logical_x, logical_y, 2.0, self.line_height))
    }

    fn has_ime_preedit(&self) -> bool {
        self.preedit.is_some()
    }

    fn on_pointer_event(&mut self, event: &PointerEvent, rect: LayoutRect) -> EventResponse {
        self.cached_rect.set(rect);
        self.sync_from_property();

        match *event {
            PointerEvent::Scroll { x, y, dy, .. } => {
                if rect.contains(x, y) {
                    let content_w = self.content_width(rect);
                    let viewport_h = self.content_height(rect);
                    let max_scroll = if let Some(shaper) = self.ensure_shaper() {
                        self.ensure_committed_cache(&shaper, content_w);
                        let lines_len = self.committed_cache.borrow().lines.len().max(1);
                        ((lines_len as f32) * self.line_height - viewport_h).max(0.0)
                    } else {
                        0.0
                    };
                    let prev_sy = self.scroll_y.get();
                    let next_sy = (prev_sy + dy * 36.0).clamp(0.0, max_scroll);
                    if (next_sy - prev_sy).abs() > f32::EPSILON {
                        self.scroll_y.set(next_sy);
                        return EventResponse::Handled;
                    }
                }
            }
            PointerEvent::Down { x, y, button } => {
                if rect.contains(x, y) {
                    if button == MouseButton::Left {
                        self.is_focused = true;
                        self.preedit = None;
                        self.preferred_x = None;
                        self.reset_caret_blink();

                        let is_double = if let Some(last) = self.last_click_instant {
                            Instant::now().duration_since(last).as_millis() < 350
                        } else {
                            false
                        };
                        if is_double {
                            self.click_count = (self.click_count % 3) + 1;
                        } else {
                            self.click_count = 1;
                        }

                        let (raw_idx, affinity) = self.char_index_and_affinity_at(x, y, rect);
                        let char_idx = raw_idx.min(self.buffer.char_len());
                        self.last_click_instant = Some(Instant::now());

                        if self.click_count == 2 {
                            let content_w = self.content_width(rect);
                            if let Some(shaper) = self.ensure_shaper() {
                                self.ensure_committed_cache(&shaper, content_w);
                            }
                            let (w_start, w_end, w_aff) = {
                                let cache = self.committed_cache.borrow();
                                let (li, _) =
                                    line_and_x_for_char(&cache.lines, char_idx, affinity);
                                if let Some(l) = cache.lines.get(li) {
                                    if l.char_start == l.char_end {
                                        (l.char_start, l.char_start, false)
                                    } else {
                                        let probe = if char_idx >= l.char_end {
                                            l.char_end - 1
                                        } else {
                                            char_idx
                                        };
                                        let (ws, we) = self.buffer.find_word_boundaries(probe);
                                        let aff = l.ends_with_soft_wrap
                                            && !l.ends_with_whitespace
                                            && we == l.char_end;
                                        (ws, we, aff)
                                    }
                                } else {
                                    let (ws, we) = self.buffer.find_word_boundaries(char_idx);
                                    (ws, we, false)
                                }
                            };
                            self.buffer.set_selection(w_start, w_end);
                            self.upstream_affinity = w_aff;
                            self.emit_primary_selection_if_any();
                        } else if self.click_count == 3 {
                            let content_w = self.content_width(rect);
                            if let Some(shaper) = self.ensure_shaper() {
                                self.ensure_committed_cache(&shaper, content_w);
                            }
                            let (l_start, l_end, l_aff) = {
                                let cache = self.committed_cache.borrow();
                                let (li, _) =
                                    line_and_x_for_char(&cache.lines, char_idx, affinity);
                                cache
                                    .lines
                                    .get(li)
                                    .map(|l| {
                                        let aff = l.ends_with_soft_wrap
                                            && !l.ends_with_whitespace
                                            && l.char_end > l.char_start;
                                        (l.char_start, l.char_end, aff)
                                    })
                                    .unwrap_or((0, self.buffer.char_len(), false))
                            };
                            self.buffer.set_selection(l_start, l_end);
                            self.upstream_affinity = l_aff;
                            self.emit_primary_selection_if_any();
                        } else {
                            self.buffer.set_selection(char_idx, char_idx);
                            self.upstream_affinity = affinity;
                            self.is_dragging_selection = true;
                        }
                        self.scroll_to_cursor_pending.set(true);
                        self.apply_scroll_now();
                        return EventResponse::Handled;
                    } else if button == MouseButton::Middle {
                        self.is_focused = true;
                        self.preedit = None;
                        self.preferred_x = None;
                        self.reset_caret_blink();
                        let (raw_idx, affinity) = self.char_index_and_affinity_at(x, y, rect);
                        let char_idx = raw_idx.min(self.buffer.char_len());
                        self.buffer.set_cursor(char_idx);
                        self.upstream_affinity = affinity;
                        self.scroll_to_cursor_pending.set(true);
                        self.apply_scroll_now();
                        self.pending_clipboard_action = Some(ClipboardAction::RequestPaste(
                            crate::platform::clipboard::SelectionTarget::Primary,
                        ));
                        return EventResponse::Handled;
                    }
                }
            }
            PointerEvent::Move { x, y } => {
                if self.is_dragging_selection {
                    let (raw_idx, affinity) = self.char_index_and_affinity_at(x, y, rect);
                    let char_idx = raw_idx.min(self.buffer.char_len());
                    let anchor = self
                        .buffer
                        .anchor()
                        .unwrap_or(self.buffer.cursor())
                        .min(self.buffer.char_len());
                    self.buffer.set_selection(anchor, char_idx);
                    self.upstream_affinity = affinity;
                    self.preferred_x = None;
                    self.reset_caret_blink();
                    self.scroll_to_cursor_pending.set(true);
                    self.apply_scroll_now();
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Up { button, .. } => {
                if button == MouseButton::Left && self.is_dragging_selection {
                    self.is_dragging_selection = false;
                    if let Some((a, f)) = self.buffer.selection_pair() {
                        if a == f {
                            self.buffer.clear_selection();
                        } else {
                            self.emit_primary_selection_if_any();
                        }
                    }
                    return EventResponse::Handled;
                }
            }
        }
        EventResponse::Ignored
    }

    fn on_key_event(&mut self, event: &KeyEvent) -> EventResponse {
        if !self.is_focused {
            return EventResponse::Ignored;
        }
        self.sync_from_property();

        let KeyEvent::KeyDown {
            key,
            keysym,
            text,
            modifiers,
            is_repeat,
        } = event
        else {
            return EventResponse::Ignored;
        };

        if self.preedit.is_some() {
            return if crate::text::editing::is_modifier_keysym(key, *keysym) {
                EventResponse::Ignored
            } else {
                EventResponse::Handled
            };
        }

        if modifiers.ctrl && !modifiers.alt && !modifiers.logo {
            if key == "a" || key == "A" || *keysym == 0x61 || *keysym == 0x41 {
                if !*is_repeat {
                    self.buffer.select_all();
                    self.upstream_affinity = false;
                    self.preferred_x = None;
                    self.emit_primary_selection_if_any();
                    self.reset_caret_blink();
                }
                return EventResponse::Handled;
            } else if key == "c" || key == "C" || *keysym == 0x63 || *keysym == 0x43 {
                if !*is_repeat {
                    if let Some(sel) = self.buffer.selected_text() {
                        self.pending_clipboard_action = Some(ClipboardAction::SetClipboard(sel));
                    }
                }
                return EventResponse::Handled;
            } else if key == "x" || key == "X" || *keysym == 0x78 || *keysym == 0x58 {
                if !*is_repeat {
                    if let Some(sel) = self.buffer.selected_text() {
                        self.pending_clipboard_action = Some(ClipboardAction::SetClipboard(sel));
                        self.buffer.delete_selection();
                        self.sync_to_property();
                        self.upstream_affinity = false;
                        self.preferred_x = None;
                        self.reset_caret_blink();
                        self.scroll_to_cursor_pending.set(true);
                        self.apply_scroll_now();
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

        if key == "Return" || key == "KP_Enter" || *keysym == 0xff0d || *keysym == 0xff8d {
            self.buffer.insert("\n");
            self.sync_to_property();
            self.upstream_affinity = false;
            self.preferred_x = None;
            self.reset_caret_blink();
            self.scroll_to_cursor_pending.set(true);
            self.apply_scroll_now();
            return EventResponse::Handled;
        } else if key == "Backspace" || *keysym == 0xff08 {
            let changed = if modifiers.ctrl {
                self.buffer.delete_word_backward()
            } else {
                self.buffer.backspace()
            };
            if changed {
                self.sync_to_property();
            }
            self.upstream_affinity = false;
            self.preferred_x = None;
            self.reset_caret_blink();
            self.scroll_to_cursor_pending.set(true);
            self.apply_scroll_now();
            return EventResponse::Handled;
        } else if key == "Delete" || *keysym == 0xffff {
            let changed = if modifiers.ctrl {
                self.buffer.delete_word_forward()
            } else {
                self.buffer.delete_forward()
            };
            if changed {
                self.sync_to_property();
            }
            self.upstream_affinity = false;
            self.preferred_x = None;
            self.reset_caret_blink();
            self.scroll_to_cursor_pending.set(true);
            self.apply_scroll_now();
            return EventResponse::Handled;
        } else if key == "Left" || *keysym == 0xff51 {
            if modifiers.ctrl {
                self.buffer.move_word_left(modifiers.shift);
            } else {
                self.buffer.move_left(modifiers.shift);
            }
            if modifiers.shift {
                self.emit_primary_selection_if_any();
            }
            self.upstream_affinity = false;
            self.preferred_x = None;
            self.reset_caret_blink();
            self.scroll_to_cursor_pending.set(true);
            self.apply_scroll_now();
            return EventResponse::Handled;
        } else if key == "Right" || *keysym == 0xff53 {
            if modifiers.ctrl {
                self.buffer.move_word_right(modifiers.shift);
            } else {
                self.buffer.move_right(modifiers.shift);
            }
            if modifiers.shift {
                self.emit_primary_selection_if_any();
            }
            self.upstream_affinity = false;
            self.preferred_x = None;
            self.reset_caret_blink();
            self.scroll_to_cursor_pending.set(true);
            self.apply_scroll_now();
            return EventResponse::Handled;
        } else if key == "Home" || *keysym == 0xff50 {
            let target = if modifiers.ctrl {
                0
            } else {
                let content_w = self.content_width(self.cached_rect.get());
                if let Some(shaper) = self.ensure_shaper() {
                    self.ensure_committed_cache(&shaper, content_w);
                }
                let cache = self.committed_cache.borrow();
                let (li, _) = line_and_x_for_char(
                    &cache.lines,
                    self.buffer.cursor(),
                    self.upstream_affinity,
                );
                cache.lines.get(li).map(|l| l.char_start).unwrap_or(0)
            };
            self.buffer.move_to(target, modifiers.shift);
            if modifiers.shift {
                self.emit_primary_selection_if_any();
            }
            self.upstream_affinity = false;
            self.preferred_x = None;
            self.reset_caret_blink();
            self.scroll_to_cursor_pending.set(true);
            self.apply_scroll_now();
            return EventResponse::Handled;
        } else if key == "End" || *keysym == 0xff57 {
            let (target, aff) = if modifiers.ctrl {
                (self.buffer.char_len(), false)
            } else {
                let content_w = self.content_width(self.cached_rect.get());
                if let Some(shaper) = self.ensure_shaper() {
                    self.ensure_committed_cache(&shaper, content_w);
                }
                let cache = self.committed_cache.borrow();
                let (li, _) = line_and_x_for_char(
                    &cache.lines,
                    self.buffer.cursor(),
                    self.upstream_affinity,
                );
                cache
                    .lines
                    .get(li)
                    .map(|l| {
                        if l.ends_with_soft_wrap && l.char_end > l.char_start {
                            if l.ends_with_whitespace {
                                (l.char_end - 1, false)
                            } else {
                                (l.char_end, true)
                            }
                        } else {
                            (l.char_end, false)
                        }
                    })
                    .unwrap_or((self.buffer.char_len(), false))
            };
            self.buffer.move_to(target, modifiers.shift);
            self.upstream_affinity = aff;
            if modifiers.shift {
                self.emit_primary_selection_if_any();
            }
            self.preferred_x = None;
            self.reset_caret_blink();
            self.scroll_to_cursor_pending.set(true);
            self.apply_scroll_now();
            return EventResponse::Handled;
        } else if key == "Up"
            || *keysym == 0xff52
            || key == "Down"
            || *keysym == 0xff54
            || key == "Page_Up"
            || key == "Prior"
            || *keysym == 0xff55
            || key == "Page_Down"
            || key == "Next"
            || *keysym == 0xff56
        {
            let rect = self.cached_rect.get();
            let content_w = self.content_width(rect);
            let viewport_h = self.content_height(rect);
            if let Some(shaper) = self.ensure_shaper() {
                self.ensure_committed_cache(&shaper, content_w);
            }
            let is_up = key == "Up" || *keysym == 0xff52;
            let is_down = key == "Down" || *keysym == 0xff54;
            let is_page_up = key == "Page_Up" || key == "Prior" || *keysym == 0xff55;
            let page_lines = ((viewport_h / self.line_height).floor() as usize).max(1);

            let (target, aff) = {
                let cache = self.committed_cache.borrow();
                let lines = &cache.lines;
                let (cur_line, cur_x) =
                    line_and_x_for_char(lines, self.buffer.cursor(), self.upstream_affinity);
                let sticky_x = *self.preferred_x.get_or_insert(cur_x);

                if is_up {
                    if cur_line == 0 {
                        self.preferred_x = None;
                        (0, false)
                    } else {
                        char_and_affinity_for_x_in_line(&lines[cur_line - 1], sticky_x)
                    }
                } else if is_down {
                    if cur_line + 1 >= lines.len() {
                        self.preferred_x = None;
                        (self.buffer.char_len(), false)
                    } else {
                        char_and_affinity_for_x_in_line(&lines[cur_line + 1], sticky_x)
                    }
                } else if is_page_up {
                    let dest_line = cur_line.saturating_sub(page_lines);
                    char_and_affinity_for_x_in_line(&lines[dest_line], sticky_x)
                } else {
                    let dest_line = (cur_line + page_lines).min(lines.len().saturating_sub(1));
                    char_and_affinity_for_x_in_line(&lines[dest_line], sticky_x)
                }
            };

            self.buffer.move_to(target, modifiers.shift);
            self.upstream_affinity = aff;
            if modifiers.shift {
                self.emit_primary_selection_if_any();
            }
            self.reset_caret_blink();
            self.scroll_to_cursor_pending.set(true);
            self.apply_scroll_now();
            return EventResponse::Handled;
        } else if let Some(t) = text {
            if !modifiers.ctrl && !modifiers.alt && !modifiers.logo {
                let filtered: String = t
                    .chars()
                    .filter(|&c| !c.is_control() || c == '\n' || c == '\t')
                    .collect();
                if !filtered.is_empty() {
                    self.buffer.insert(&filtered);
                    self.sync_to_property();
                    self.upstream_affinity = false;
                    self.preferred_x = None;
                    self.reset_caret_blink();
                    self.scroll_to_cursor_pending.set(true);
                    self.apply_scroll_now();
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
        _vector_atlas: &mut VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        self.cached_rect.set(rect);
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
            [
                rect.x * scale,
                rect.y * scale,
                rect.width * scale,
                rect.height * scale,
            ],
            theme.colors.surface,
            radii,
            Border::new(border_color, (if self.is_focused { 1.5 } else { 1.0 }) * scale),
        ));

        let content_x = rect.x + self.style.padding.left;
        let content_y = rect.y + self.style.padding.top;
        let content_w = self.content_width(rect);
        let content_h = self.content_height(rect);

        prims.push(RenderPrimitive::ClipPush {
            rect: [
                (rect.x + 2.0) * scale,
                (rect.y + 2.0) * scale,
                (rect.width - 4.0).max(0.0) * scale,
                (rect.height - 4.0).max(0.0) * scale,
            ],
        });

        let composed = self.compose_display_string();
        let has_preedit = composed.is_some();
        let is_empty = self.text.get().is_empty() && !has_preedit;

        if is_empty {
            self.scroll_y.set(0.0);
            self.scroll_to_cursor_pending.set(false);
            if !self.placeholder.is_empty() {
                let mut pcache = self.placeholder_cache.borrow_mut();
                Self::update_cache_entry(
                    &mut pcache,
                    &self.placeholder,
                    self.font_size,
                    self.line_height,
                    content_w,
                    shaper,
                );
                let baseline_offset = self.font_size * 0.85;
                for line in &pcache.lines {
                    if line.y > content_h {
                        break;
                    }
                    crate::text::emit_text_glyphs(
                        &line.shaped,
                        content_x,
                        content_y + line.y + baseline_offset,
                        self.font_size,
                        theme.colors.text_secondary,
                        scale,
                        atlas,
                        rasterizer,
                        &mut prims,
                    );
                }
            }

            if self.is_focused && self.cursor_visible {
                prims.push(RenderPrimitive::solid_rect(
                    [
                        content_x * scale,
                        content_y * scale,
                        2.0 * scale,
                        self.line_height * scale,
                    ],
                    theme.colors.cursor,
                    1.0 * scale,
                ));
            }

            prims.push(RenderPrimitive::ClipPop);
            return prims;
        }

        if let Some((ref combined, _, _, _)) = composed {
            let mut dcache = self.display_cache.borrow_mut();
            Self::update_cache_entry(
                &mut dcache,
                combined,
                self.font_size,
                self.line_height,
                content_w,
                shaper,
            );
        } else {
            self.ensure_committed_cache(shaper, content_w);
        }

        let lines_guard = if has_preedit {
            self.display_cache.borrow()
        } else {
            self.committed_cache.borrow()
        };
        let lines = &lines_guard.lines;

        let active_cursor_char = if let Some((_, _, _, active_char)) = composed {
            active_char
        } else {
            self.cursor_pos()
        };
        let active_affinity = if has_preedit {
            false
        } else {
            self.upstream_affinity
        };

        let (cursor_line_idx, cursor_rel_x, sy) =
            self.sync_scroll_for_cursor(lines, active_cursor_char, active_affinity, content_h);

        let selection = if !has_preedit {
            self.selection_range()
        } else {
            None
        };
        let baseline_offset = self.font_size * 0.85;

        for line in lines {
            if line.y + self.line_height < sy {
                continue;
            }
            if line.y > sy + content_h {
                break;
            }

            let draw_y = content_y + line.y - sy;

            if let Some((sel_start, sel_end)) = selection {
                let s = sel_start.max(line.char_start);
                let e = sel_end.min(line.char_end);
                let includes_newline = !line.ends_with_soft_wrap
                    && sel_start <= line.char_end
                    && sel_end > line.char_end;

                if s < e || includes_newline {
                    let x_start = line
                        .char_x
                        .get(s.saturating_sub(line.char_start))
                        .copied()
                        .unwrap_or(0.0);
                    let mut x_end = line
                        .char_x
                        .get(e.saturating_sub(line.char_start))
                        .copied()
                        .unwrap_or(line.shaped.width);
                    if includes_newline {
                        x_end += self.font_size * 0.4;
                    }
                    prims.push(RenderPrimitive::solid_rect(
                        [
                            (content_x + x_start) * scale,
                            draw_y * scale,
                            ((x_end - x_start).max(1.5)) * scale,
                            self.line_height * scale,
                        ],
                        theme.colors.selection,
                        2.0 * scale,
                    ));
                }
            }

            crate::text::emit_text_glyphs(
                &line.shaped,
                content_x,
                draw_y + baseline_offset,
                self.font_size,
                theme.colors.text,
                scale,
                atlas,
                rasterizer,
                &mut prims,
            );

            if let Some((_, pre_start, pre_end, _)) = composed {
                let s = pre_start.max(line.char_start);
                let e = pre_end.min(line.char_end);
                let underline_y = (draw_y + baseline_offset + 2.0) * scale;
                if s < e {
                    let x_start = line
                        .char_x
                        .get(s - line.char_start)
                        .copied()
                        .unwrap_or(0.0);
                    let x_end = line
                        .char_x
                        .get(e - line.char_start)
                        .copied()
                        .unwrap_or(line.shaped.width);
                    prims.push(RenderPrimitive::solid_rect(
                        [
                            (content_x + x_start) * scale,
                            underline_y,
                            ((x_end - x_start).max(1.0)) * scale,
                            1.5 * scale,
                        ],
                        theme.colors.text,
                        0.5 * scale,
                    ));
                }

                if let Some(pre) = &self.preedit {
                    if pre.cursor_begin >= 0 && pre.cursor_end > pre.cursor_begin {
                        let clause_start =
                            pre_start + byte_to_char_clamped(&pre.text, pre.cursor_begin as usize);
                        let clause_end =
                            pre_start + byte_to_char_clamped(&pre.text, pre.cursor_end as usize);
                        let cs = clause_start.max(line.char_start);
                        let ce = clause_end.min(line.char_end);
                        if cs < ce {
                            let cx_start = line
                                .char_x
                                .get(cs - line.char_start)
                                .copied()
                                .unwrap_or(0.0);
                            let cx_end = line
                                .char_x
                                .get(ce - line.char_start)
                                .copied()
                                .unwrap_or(line.shaped.width);
                            prims.push(RenderPrimitive::solid_rect(
                                [
                                    (content_x + cx_start) * scale,
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
            }
        }

        if !has_preedit && self.is_focused && self.cursor_visible {
            let caret_y = content_y + cursor_line_idx as f32 * self.line_height - sy;
            prims.push(RenderPrimitive::solid_rect(
                [
                    (content_x + cursor_rel_x) * scale,
                    caret_y * scale,
                    2.0 * scale,
                    self.line_height * scale,
                ],
                theme.colors.cursor,
                1.0 * scale,
            ));
        }

        let total_h = (lines.len() as f32) * self.line_height;
        if total_h > content_h {
            let track_h = (rect.height - 8.0).max(8.0);
            let ratio = (content_h / total_h).clamp(0.1, 1.0);
            let thumb_h = (track_h * ratio).max(16.0);
            let max_scroll = (total_h - content_h).max(1.0);
            let thumb_y = rect.y + 4.0 + (track_h - thumb_h) * (sy / max_scroll).clamp(0.0, 1.0);
            let thumb_x = rect.x + rect.width - 6.0;
            prims.push(RenderPrimitive::solid_rect(
                [
                    thumb_x * scale,
                    thumb_y * scale,
                    3.0 * scale,
                    thumb_h * scale,
                ],
                [
                    theme.colors.text_secondary[0],
                    theme.colors.text_secondary[1],
                    theme.colors.text_secondary[2],
                    0.45,
                ],
                1.5 * scale,
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

    fn key_down(key: &str, keysym: u32, text: Option<&str>, modifiers: Modifiers) -> KeyEvent {
        KeyEvent::KeyDown {
            key: key.to_string(),
            keysym,
            text: text.map(|s| s.to_string()),
            modifiers,
            is_repeat: false,
        }
    }

    #[test]
    fn test_text_area_wrapping_words_long_tokens_and_newlines() {
        let shaper = TextShaper::default_system_font().expect("Font load");

        // Word-boundary wrapping + hard newline + empty line + trailing newline
        let prefix_w = shaper.shape("hello world ", 14.0).width;
        let full_w = shaper.shape("hello world again", 14.0).width;
        let wrap_w = (prefix_w + full_w) * 0.5;
        let area = TextArea::new("Notes")
            .with_size(wrap_w + 32.0, 100.0)
            .with_text("hello world again\n\ntrailing\n");
        area.layout_lines(&shaper, wrap_w);
        let lines = area.visual_lines();
        assert_eq!(
            lines,
            vec!["hello world ", "again", "", "trailing", ""],
            "Expected word-boundary wrap + empty line + trailing + final empty line"
        );

        // Single unbroken word wider than max_width falls back to character-level wrapping
        let long_word = "supercalifragilisticexpialidocious";
        let area_long = TextArea::new("")
            .with_size(90.0, 100.0)
            .with_text(long_word);
        let max_w = area_long.content_width(area_long.cached_rect.get());
        area_long.layout_lines(&shaper, max_w);
        let wrapped = area_long.visual_lines();
        assert!(wrapped.len() >= 3, "Long word must soft-wrap across multiple lines");
        let rejoined: String = wrapped.concat();
        assert_eq!(rejoined, long_word);
        for seg in &wrapped {
            assert!(!seg.is_empty());
        }
    }

    #[test]
    fn test_text_area_up_down_sticky_column_and_home_end() {
        let mut area = TextArea::new("")
            .with_size(360.0, 120.0)
            .with_text("0123456789abcdef\nhi\n0123456789abcdef");
        area.on_focus_change(true);

        // Place cursor at index 10 on line 0 ("0123456789|abcdef")
        area.set_cursor_pos(10);

        // Down -> line 1 ("hi") is shorter, clamps to end of line 1 (index 19: 17 + 2)
        area.on_key_event(&key_down("Down", 0xff54, None, Modifiers::default()));
        assert_eq!(area.cursor_pos(), 19);

        // Down again -> sticky column restores horizontal position on line 2 (near index 20 + 10 = 30)
        area.on_key_event(&key_down("Down", 0xff54, None, Modifiers::default()));
        let col_on_line2 = area.cursor_pos() - 20;
        assert!(
            (9..=11).contains(&col_on_line2),
            "Expected sticky x to land near column 10 on line 2, got {}",
            col_on_line2
        );

        // Home moves to start of line 2 (index 20), End moves to end of line 2 (index 36)
        area.on_key_event(&key_down("Home", 0xff50, None, Modifiers::default()));
        assert_eq!(area.cursor_pos(), 20);
        area.on_key_event(&key_down("End", 0xff57, None, Modifiers::default()));
        assert_eq!(area.cursor_pos(), 36);
    }

    #[test]
    fn test_text_area_scroll_to_cursor_and_hit_testing() {
        // Height 60pt with 20pt padding => 40pt viewport (2 visible lines of 20pt each)
        let mut area = TextArea::new("").with_size(260.0, 60.0);
        let rect = LayoutRect::new(0.0, 0.0, 260.0, 60.0);
        area.on_focus_change(true);

        for i in 1..=6 {
            let s = if i < 6 {
                format!("Line number {}\n", i)
            } else {
                format!("Line number {}", i)
            };
            area.on_paste(&s, false);
        }
        assert!(
            area.scroll_y.get() > 0.0,
            "Viewport must scroll down automatically to keep bottom cursor visible"
        );

        // Ctrl+Home jumps to index 0 and scrolls back to top (scroll_y == 0)
        area.on_key_event(&key_down(
            "Home",
            0xff50,
            None,
            Modifiers {
                ctrl: true,
                ..Default::default()
            },
        ));
        assert_eq!(area.cursor_pos(), 0);
        assert_eq!(area.scroll_y.get(), 0.0);

        // Hit-testing line 0 vs line 1 via pointer click
        let idx_top = area.char_index_at(14.0, 15.0, rect);
        let idx_second = area.char_index_at(14.0, 35.0, rect);
        assert_eq!(idx_top, 0);
        assert!(
            idx_second >= 14,
            "Clicking second line should map to line 2 start, got {}",
            idx_second
        );
    }

    #[test]
    fn test_text_area_ime_preedit_commit_and_cursor_rect() {
        let mut area = TextArea::new("Type here...").with_size(260.0, 100.0);
        let rect = LayoutRect::new(20.0, 30.0, 260.0, 100.0);

        assert!(area.ime_cursor_rect(rect).is_none());
        area.on_focus_change(true);

        // Insert a first line, press Return to move to line 2
        area.on_key_event(&key_down("a", 0x61, Some("a"), Modifiers::default()));
        let r_line1 = area.ime_cursor_rect(rect).expect("rect on line 1");
        area.on_key_event(&key_down("Return", 0xff0d, None, Modifiers::default()));
        let r_line2 = area.ime_cursor_rect(rect).expect("rect on line 2");
        assert!(
            r_line2.y > r_line1.y,
            "IME cursor rectangle Y must advance to second line after Return"
        );

        // Start preedit on line 2
        area.on_ime_event(&ImeEvent::Preedit {
            text: "かんじ".into(),
            cursor_begin: 9,
            cursor_end: 9,
        });
        assert_eq!(area.text.get(), "a\n");
        let r_pre = area.ime_cursor_rect(rect).expect("rect during preedit");
        assert!(r_pre.x > r_line2.x);

        // Commit replaces preedit
        area.on_ime_event(&ImeEvent::Commit("漢字".into()));
        assert_eq!(area.preedit_text(), None);
        assert_eq!(area.text.get(), "a\n漢字");
        assert_eq!(area.cursor_pos(), 4);
    }

    #[test]
    fn test_text_area_flex_stretch_and_grow_with_auto_dimensions() {
        let shaper = TextShaper::default_system_font().expect("Font load");
        let mut engine = crate::layout::LayoutEngine::new();
        let mut tree = crate::widget::WidgetTree::new();

        let root = crate::widget::Container::new(FlexStyle {
            direction: FlexDirection::Column,
            align_items: AlignItems::Stretch,
            width: Dimension::Points(800.0),
            height: Dimension::Points(600.0),
            ..Default::default()
        });
        let root_id = tree.add_widget(&mut engine, Box::new(root), None);
        tree.set_root(root_id);

        let mut area = TextArea::new("Auto stretch & grow");
        area.style.width = Dimension::Auto;
        area.style.height = Dimension::Auto;
        area.style.flex_grow = 1.0;
        let area_id = tree.add_widget(&mut engine, Box::new(area), Some(root_id));

        tree.sync_layout(&mut engine, &shaper);
        let root_layout = tree.nodes[root_id.0].as_ref().unwrap().layout_node;
        let area_layout = tree.nodes[area_id.0].as_ref().unwrap().layout_node;
        engine.compute(root_layout, 800.0, 600.0);

        let computed = engine.get_rect(area_layout);
        assert!(
            (computed.width - 800.0).abs() < 0.01,
            "Expected TextArea width to stretch to 800.0, got {}",
            computed.width
        );
        assert!(
            (computed.height - 600.0).abs() < 0.01,
            "Expected TextArea height to flex-grow to 600.0, got {}",
            computed.height
        );
    }

    #[test]
    fn test_text_area_character_wrap_end_click_drag_and_up_down() {
        let shaper = TextShaper::default_system_font().expect("Font load");
        let long_word = "supercalifragilisticexpialidocious";
        let rect = LayoutRect::new(0.0, 0.0, 90.0, 120.0);
        let mut area = TextArea::new("").with_size(90.0, 120.0).with_text(long_word);
        area.on_focus_change(true);
        area.layout_lines(&shaper, area.content_width(rect));

        let lines = area.visual_lines();
        assert!(lines.len() >= 3);
        let line0_len = lines[0].chars().count();
        let line1_len = lines[1].chars().count();

        area.set_cursor_pos(0);
        area.on_key_event(&key_down(
            "End",
            0xff57,
            None,
            Modifiers {
                shift: true,
                ..Default::default()
            },
        ));
        assert_eq!(area.selection_range(), Some((0, line0_len)));
        assert_eq!(area.selected_text().as_deref(), Some(lines[0].as_str()));

        let r_end0 = area.ime_cursor_rect(rect).expect("IME rect at end of line 0");
        let content_y = rect.y + area.style.padding.top;
        assert!(
            (r_end0.y - content_y).abs() < 0.01,
            "End on character-wrapped line 0 must keep caret visually on line 0"
        );

        area.on_key_event(&key_down("Down", 0xff54, None, Modifiers::default()));
        assert_eq!(area.cursor_pos(), line0_len + line1_len);
        let r_end1 = area.ime_cursor_rect(rect).expect("IME rect at end of line 1");
        assert!(
            (r_end1.y - (content_y + area.line_height)).abs() < 0.01,
            "Down from end of line 0 should move to end of line 1"
        );

        area.on_key_event(&key_down("Up", 0xff52, None, Modifiers::default()));
        assert_eq!(area.cursor_pos(), line0_len);
        area.on_key_event(&key_down("Up", 0xff52, None, Modifiers::default()));
        assert_eq!(
            area.cursor_pos(),
            0,
            "Second Up at line 0 boundary must reach index 0 instead of looping"
        );

        area.on_pointer_event(
            &PointerEvent::Down {
                x: 88.0,
                y: content_y + 5.0,
                button: MouseButton::Left,
            },
            rect,
        );
        area.on_pointer_event(
            &PointerEvent::Up {
                x: 88.0,
                y: content_y + 5.0,
                button: MouseButton::Left,
            },
            rect,
        );
        assert_eq!(area.cursor_pos(), line0_len);
        area.on_key_event(&key_down("X", 0x58, Some("X"), Modifiers::default()));
        let mut expected = String::new();
        expected.push_str(&long_word[..lines[0].len()]);
        expected.push('X');
        expected.push_str(&long_word[lines[0].len()..]);
        assert_eq!(area.text.get(), &expected);
    }

    #[test]
    fn test_text_area_double_click_before_blank_line_drag_bounds_and_scroll_bubble() {
        let rect = LayoutRect::new(10.0, 20.0, 260.0, 120.0);
        let mut area = TextArea::new("")
            .with_size(260.0, 120.0)
            .with_text("hello\n\nworld");
        area.on_focus_change(true);

        let content_y = rect.y + area.style.padding.top;
        area.on_pointer_event(
            &PointerEvent::Down {
                x: 220.0,
                y: content_y + 5.0,
                button: MouseButton::Left,
            },
            rect,
        );
        area.on_pointer_event(
            &PointerEvent::Up {
                x: 220.0,
                y: content_y + 5.0,
                button: MouseButton::Left,
            },
            rect,
        );
        area.on_pointer_event(
            &PointerEvent::Down {
                x: 220.0,
                y: content_y + 5.0,
                button: MouseButton::Left,
            },
            rect,
        );
        area.on_pointer_event(
            &PointerEvent::Up {
                x: 220.0,
                y: content_y + 5.0,
                button: MouseButton::Left,
            },
            rect,
        );
        assert_eq!(area.selected_text().as_deref(), Some("hello"));

        area.last_click_instant = None;
        area.on_pointer_event(
            &PointerEvent::Down {
                x: 60.0,
                y: content_y + area.line_height + 5.0,
                button: MouseButton::Left,
            },
            rect,
        );
        area.on_pointer_event(
            &PointerEvent::Move {
                x: 120.0,
                y: rect.y - 15.0,
            },
            rect,
        );
        assert_eq!(area.selection_range(), Some((0, 6)));

        area.on_pointer_event(
            &PointerEvent::Move {
                x: 30.0,
                y: content_y + 95.0,
            },
            rect,
        );
        assert_eq!(area.selection_range(), Some((6, 12)));
        area.on_pointer_event(
            &PointerEvent::Up {
                x: 30.0,
                y: content_y + 95.0,
                button: MouseButton::Left,
            },
            rect,
        );

        let scroll_resp = area.on_pointer_event(
            &PointerEvent::Scroll {
                x: rect.x + 20.0,
                y: rect.y + 20.0,
                dx: 0.0,
                dy: 1.0,
            },
            rect,
        );
        assert_eq!(
            scroll_resp,
            EventResponse::Ignored,
            "Wheel scroll must bubble when TextArea has no scrollable overflow"
        );
    }

    #[test]
    fn test_text_area_preedit_suppresses_raw_keys_tab_and_modal_clears_preedit() {
        let shaper = TextShaper::default_system_font().expect("Font load");
        let mut engine = crate::layout::LayoutEngine::new();
        let mut tree = crate::widget::WidgetTree::new();
        let mut dispatcher = crate::event::EventDispatcher::new();

        let root = crate::widget::Container::new(FlexStyle {
            direction: FlexDirection::Column,
            width: Dimension::Points(400.0),
            height: Dimension::Points(300.0),
            ..Default::default()
        });
        let root_id = tree.add_widget(&mut engine, Box::new(root), None);
        tree.set_root(root_id);

        let area = TextArea::new("").with_size(300.0, 100.0).with_text("base");
        let area_id = tree.add_widget(&mut engine, Box::new(area), Some(root_id));
        let other = crate::widget::TextInput::new("other");
        let _other_id = tree.add_widget(&mut engine, Box::new(other), Some(root_id));

        tree.sync_layout(&mut engine, &shaper);
        let root_layout = tree.nodes[root_id.0].as_ref().unwrap().layout_node;
        engine.compute(root_layout, 400.0, 300.0);

        dispatcher.set_focus(Some(area_id));
        tree.nodes[area_id.0]
            .as_mut()
            .unwrap()
            .widget
            .on_focus_change(true);

        tree.dispatch_ime_event(
            &dispatcher,
            &ImeEvent::Preedit {
                text: "へんかん".into(),
                cursor_begin: 0,
                cursor_end: 6,
            },
        );
        assert!(tree.focused_has_ime_preedit(&dispatcher));

        tree.dispatch_key_event(&mut dispatcher, &key_down("Tab", 0xff09, None, Modifiers::default()));
        assert_eq!(
            dispatcher.focus,
            Some(area_id),
            "Tab must not yank focus away during active IME preedit"
        );

        tree.dispatch_key_event(&mut dispatcher, &key_down("Left", 0xff51, None, Modifiers::default()));
        tree.dispatch_key_event(&mut dispatcher, &key_down("z", 0x7a, Some("z"), Modifiers::default()));

        {
            let area_ref = tree.nodes[area_id.0]
                .as_ref()
                .unwrap()
                .widget
                .as_any()
                .downcast_ref::<TextArea>()
                .unwrap();
            assert_eq!(area_ref.text.get(), "base");
            assert_eq!(area_ref.cursor_pos(), 4);

            let theme = Theme::dark();
            let mut atlas = GlyphAtlas::new(512, 512, 1);
            let rasterizer = GlyphRasterizer::new(shaper.font_db().clone());
            let mut vector_atlas = VectorAtlas::new(512, 512);
            let prims = area_ref.paint(
                LayoutRect::new(0.0, 0.0, 300.0, 100.0),
                &theme,
                &mut atlas,
                &rasterizer,
                &shaper,
                &mut vector_atlas,
                1.0,
            );
            let has_clause_highlight = prims.iter().any(|p| match p {
                RenderPrimitive::Rect {
                    rect: r,
                    border: None,
                    use_texture: false,
                    ..
                } => (r[3] - 2.5).abs() < 0.01 && r[2] > 5.0,
                _ => false,
            });
            assert!(
                has_clause_highlight,
                "TextArea::paint must render active-clause preedit underline highlight"
            );
        }

        tree.open_modal(crate::widget::Dialog::alert("Confirm", "Discard?"));
        assert!(
            !tree.focused_has_ime_preedit(&dispatcher),
            "Opening a modal dialog must clear active IME preedit on underlying widgets"
        );
    }
}
