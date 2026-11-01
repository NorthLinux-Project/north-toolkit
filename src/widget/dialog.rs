//! Modal dialog system with animated scrim backdrops, focus management, and keyboard handling.

use std::cell::{Cell, RefCell};
use std::sync::{Arc, Mutex};

use crate::animation::{Easing, Tween};
use crate::event::{CursorIcon, KeyEvent, MouseButton, PointerEvent};
use crate::layout::LayoutRect;
use crate::render::{Border, CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// Response value produced when a modal dialog is dismissed or confirmed.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DialogResponse {
    /// Primary or confirm action was selected.
    Confirm,
    /// Cancel or dismiss action was triggered (e.g. via backdrop click or Escape).
    Cancel,
    /// Custom button action selected by button index.
    Custom(usize),
}

/// Action button definition for modal dialogs.
#[derive(Debug, Clone, PartialEq)]
pub struct DialogButton {
    /// Button label text.
    pub label: String,
    /// Whether this button represents the primary affirmative action.
    pub is_primary: bool,
    /// Response value produced when this button is clicked.
    pub response: DialogResponse,
}

/// Thread-safe handle for opening and polling pending modal dialogs from widgets.
#[derive(Clone, Default)]
pub struct ModalHandle {
    pub(crate) pending: Arc<Mutex<Option<Dialog>>>,
}

impl ModalHandle {
    /// Creates a new empty modal handle.
    pub fn new() -> Self {
        Self {
            pending: Arc::new(Mutex::new(None)),
        }
    }

    /// Queues a dialog to be opened by the application runtime.
    pub fn open(&self, dialog: Dialog) {
        if let Ok(mut lock) = self.pending.lock() {
            *lock = Some(dialog);
        }
    }

    /// Takes the queued dialog if one is present.
    pub fn take(&self) -> Option<Dialog> {
        if let Ok(mut lock) = self.pending.lock() {
            lock.take()
        } else {
            None
        }
    }
}

/// Wraps text into multiple lines bounded by `max_width` points using font shaping metrics.
pub fn wrap_text(text: &str, max_width: f32, font_size: f32, shaper: &TextShaper) -> Vec<String> {
    if text.is_empty() {
        return Vec::new();
    }

    let mut result = Vec::new();

    for paragraph in text.lines() {
        if paragraph.trim().is_empty() {
            result.push(String::new());
            continue;
        }

        let paragraph_shaped = shaper.shape(paragraph, font_size);
        if paragraph_shaped.width <= max_width {
            result.push(paragraph.to_string());
            continue;
        }

        let mut current_line = String::new();
        for word in paragraph.split_whitespace() {
            if current_line.is_empty() {
                let word_shaped = shaper.shape(word, font_size);
                if word_shaped.width <= max_width {
                    current_line = word.to_string();
                } else {
                    let mut partial = String::new();
                    for ch in word.chars() {
                        let mut test_str = partial.clone();
                        test_str.push(ch);
                        if shaper.shape(&test_str, font_size).width > max_width && !partial.is_empty() {
                            result.push(partial);
                            partial = ch.to_string();
                        } else {
                            partial = test_str;
                        }
                    }
                    current_line = partial;
                }
            } else {
                let candidate = format!("{} {}", current_line, word);
                if shaper.shape(&candidate, font_size).width <= max_width {
                    current_line = candidate;
                } else {
                    result.push(current_line);
                    let word_shaped = shaper.shape(word, font_size);
                    if word_shaped.width <= max_width {
                        current_line = word.to_string();
                    } else {
                        let mut partial = String::new();
                        for ch in word.chars() {
                            let mut test_str = partial.clone();
                            test_str.push(ch);
                            if shaper.shape(&test_str, font_size).width > max_width && !partial.is_empty() {
                                result.push(partial);
                                partial = ch.to_string();
                            } else {
                                partial = test_str;
                            }
                        }
                        current_line = partial;
                    }
                }
            }
        }

        if !current_line.is_empty() {
            result.push(current_line);
        }
    }

    result
}

/// Declarative modal dialog specification with title, optional message, and action buttons.
pub struct Dialog {
    /// Dialog window title.
    pub title: String,
    /// Optional explanatory or body text.
    pub message: Option<String>,
    /// Array of action buttons displayed in the dialog footer.
    pub buttons: Vec<DialogButton>,
    /// Target width of the dialog card in logical points.
    pub width: f32,
    /// Whether clicking the dim backdrop dismisses the dialog with `DialogResponse::Cancel`.
    pub dismiss_on_backdrop: bool,
    /// Whether pressing the Escape key dismisses the dialog with `DialogResponse::Cancel`.
    pub dismiss_on_escape: bool,
    /// Callback invoked when a dialog response is selected.
    pub on_response: Option<Box<dyn FnMut(DialogResponse)>>,
}

impl Dialog {
    /// Creates a new empty dialog with the specified title and default dimensions.
    pub fn new<S: Into<String>>(title: S) -> Self {
        Self {
            title: title.into(),
            message: None,
            buttons: Vec::new(),
            width: 400.0,
            dismiss_on_backdrop: true,
            dismiss_on_escape: true,
            on_response: None,
        }
    }

    /// Convenience constructor for an alert dialog with a single "OK" button.
    pub fn alert<S: Into<String>, M: Into<String>>(title: S, message: M) -> Self {
        Self::new(title).with_message(message).with_button(
            "OK",
            true,
            DialogResponse::Confirm,
        )
    }

    /// Convenience constructor for a confirmation dialog with "Cancel" and "Confirm" buttons.
    pub fn confirm<S: Into<String>, M: Into<String>>(title: S, message: M) -> Self {
        Self::new(title)
            .with_message(message)
            .with_button("Cancel", false, DialogResponse::Cancel)
            .with_button("Confirm", true, DialogResponse::Confirm)
    }

    /// Sets the explanatory message text.
    pub fn with_message<M: Into<String>>(mut self, message: M) -> Self {
        self.message = Some(message.into());
        self
    }

    /// Appends an action button to the dialog footer.
    pub fn with_button<S: Into<String>>(
        mut self,
        label: S,
        is_primary: bool,
        response: DialogResponse,
    ) -> Self {
        self.buttons.push(DialogButton {
            label: label.into(),
            is_primary,
            response,
        });
        self
    }

    /// Overrides the dialog card width in logical points.
    pub fn with_width(mut self, width: f32) -> Self {
        self.width = width;
        self
    }

    /// Configures whether clicking the backdrop dismisses the dialog.
    pub fn with_dismiss_on_backdrop(mut self, dismiss: bool) -> Self {
        self.dismiss_on_backdrop = dismiss;
        self
    }

    /// Configures whether pressing Escape dismisses the dialog.
    pub fn with_dismiss_on_escape(mut self, dismiss: bool) -> Self {
        self.dismiss_on_escape = dismiss;
        self
    }

    /// Attaches a response callback handler.
    pub fn on_response<F: FnMut(DialogResponse) + 'static>(mut self, cb: F) -> Self {
        self.on_response = Some(Box::new(cb));
        self
    }
}

/// Lifecycle phases of an active modal animation and interaction.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ModalPhase {
    /// Dialog is animating in with scaling and backdrop fade.
    Entering,
    /// Dialog is fully visible and accepting user interaction.
    Active,
    /// Dialog is animating out before triggering dismissal callback.
    Exiting(DialogResponse),
    /// Dialog animation has finished and dialog is marked for teardown.
    Closed,
}

/// Runtime state and rendering controller for an active modal overlay.
pub struct ActiveModal {
    /// Dialog configuration and callbacks.
    pub dialog: Dialog,
    /// Current lifecycle phase.
    pub phase: ModalPhase,
    /// Animation progress tween between 0.0 and 1.0.
    pub anim_progress: Tween<f32>,
    /// Index of the currently focused button.
    pub focused_button: usize,
    /// Index of the currently hovered button.
    pub hovered_button: Option<usize>,
    /// Computed bounds of the dialog card.
    pub dialog_rect: Cell<LayoutRect>,
    /// Computed bounds of each action button.
    pub button_rects: RefCell<Vec<LayoutRect>>,
}

impl ActiveModal {
    /// Duration of enter animation in milliseconds.
    pub const ENTER_DURATION_MS: f32 = 180.0;
    /// Duration of exit animation in milliseconds.
    pub const EXIT_DURATION_MS: f32 = 150.0;

    /// Instantiates an active modal controller for the specified dialog.
    pub fn new(dialog: Dialog) -> Self {
        let mut tween = Tween::new(0.0);
        tween.set_target(1.0, Self::ENTER_DURATION_MS, Easing::EaseOut);
        let default_focused = if dialog.buttons.is_empty() {
            0
        } else {
            dialog
                .buttons
                .iter()
                .position(|b| b.is_primary)
                .unwrap_or(0)
        };

        let pad = 24.0;
        let content_w = (dialog.width - pad * 2.0).clamp(200.0, 800.0);
        let dialog_w = content_w + pad * 2.0;
        let title_h = 24.0;
        let msg_h = if dialog.message.is_some() { 40.0 } else { 0.0 };
        let button_h = 34.0;
        let button_row_h = if dialog.buttons.is_empty() { 0.0 } else { button_h + 24.0 };
        let dialog_h = pad + title_h + msg_h + button_row_h + pad;
        let dialog_rect = LayoutRect::new((800.0 - dialog_w) * 0.5, (600.0 - dialog_h) * 0.5, dialog_w, dialog_h);

        let mut button_rects = Vec::with_capacity(dialog.buttons.len());
        let mut curr_right = dialog_rect.x + dialog_w - pad;
        let button_y = dialog_rect.y + dialog_h - pad - button_h;
        let button_gap = 10.0;
        for btn in dialog.buttons.iter().rev() {
            let btn_w = (btn.label.len() as f32 * 8.5 + 32.0).max(84.0);
            let btn_x = curr_right - btn_w;
            button_rects.push(LayoutRect::new(btn_x, button_y, btn_w, button_h));
            curr_right = btn_x - button_gap;
        }
        button_rects.reverse();

        Self {
            dialog,
            phase: ModalPhase::Entering,
            anim_progress: tween,
            focused_button: default_focused,
            hovered_button: None,
            dialog_rect: Cell::new(dialog_rect),
            button_rects: RefCell::new(button_rects),
        }
    }

    /// Updates computed dialog and button rects based on the latest window dimensions and text metrics.
    pub fn update_layout(&self, win_w: f32, win_h: f32, shaper: &TextShaper) {
        let (d_rect, b_rects) = self.calculate_layout(win_w, win_h, shaper);
        self.dialog_rect.set(d_rect);
        *self.button_rects.borrow_mut() = b_rects;
    }

    /// Initiates closing animation and records the selected response.
    pub fn close(&mut self, response: DialogResponse) {
        if self.phase == ModalPhase::Closed {
            return;
        }
        self.phase = ModalPhase::Exiting(response);
        self.anim_progress
            .set_target(0.0, Self::EXIT_DURATION_MS, Easing::EaseOut);
    }

    /// Advances the enter or exit animation timer by `delta_ms` milliseconds.
    pub fn tick(&mut self, delta_ms: f32) -> bool {
        match self.phase {
            ModalPhase::Entering => {
                self.anim_progress.tick(delta_ms);
                if self.anim_progress.is_complete() {
                    self.phase = ModalPhase::Active;
                }
                true
            }
            ModalPhase::Active => false,
            ModalPhase::Exiting(resp) => {
                self.anim_progress.tick(delta_ms);
                if self.anim_progress.is_complete() {
                    if let Some(cb) = &mut self.dialog.on_response {
                        cb(resp);
                    }
                    self.phase = ModalPhase::Closed;
                }
                true
            }
            ModalPhase::Closed => false,
        }
    }

    /// Returns `true` if the exit animation has completed and the dialog is fully closed.
    pub fn is_closed(&self) -> bool {
        self.phase == ModalPhase::Closed
    }

    /// Computes layout bounds for the modal dialog card and buttons given window dimensions.
    pub fn calculate_layout(
        &self,
        win_w: f32,
        win_h: f32,
        shaper: &TextShaper,
    ) -> (LayoutRect, Vec<LayoutRect>) {
        let pad = 24.0;
        let content_w = (self.dialog.width - pad * 2.0).clamp(200.0, win_w - 48.0);
        let dialog_w = content_w + pad * 2.0;

        let title_h = 24.0;
        let line_height = 20.0;
        let mut msg_h = 0.0;
        if let Some(msg) = &self.dialog.message {
            let lines = wrap_text(msg, content_w, 14.0, shaper);
            if !lines.is_empty() {
                msg_h = lines.len() as f32 * line_height + 12.0;
            }
        }

        let button_h = 34.0;
        let button_row_h = if self.dialog.buttons.is_empty() {
            0.0
        } else {
            button_h + 24.0
        };

        let dialog_h = pad + title_h + msg_h + button_row_h + pad;
        let dialog_x = ((win_w - dialog_w) * 0.5).max(24.0);
        let dialog_y = ((win_h - dialog_h) * 0.5).max(24.0);
        let dialog_rect = LayoutRect::new(dialog_x, dialog_y, dialog_w, dialog_h);

        // Buttons laid out horizontally from right to left
        let mut button_rects = Vec::with_capacity(self.dialog.buttons.len());
        let mut curr_right = dialog_x + dialog_w - pad;
        let button_y = dialog_y + dialog_h - pad - button_h;
        let button_gap = 10.0;

        for btn in self.dialog.buttons.iter().rev() {
            let shaped_lbl = shaper.shape(&btn.label, 14.0);
            let btn_w = (shaped_lbl.width + 32.0).max(84.0);
            let btn_x = curr_right - btn_w;
            button_rects.push(LayoutRect::new(btn_x, button_y, btn_w, button_h));
            curr_right = btn_x - button_gap;
        }
        button_rects.reverse();

        (dialog_rect, button_rects)
    }

    /// Dispatches a pointer event to the modal card and action buttons.
    pub fn dispatch_pointer_event(&mut self, event: &PointerEvent) -> Option<DialogResponse> {
        let (px, py) = event.position();
        let dialog_rect = self.dialog_rect.get();
        let clicked_resp = match *event {
            PointerEvent::Move { .. } => {
                let button_rects = self.button_rects.borrow();
                self.hovered_button = None;
                for (idx, rect) in button_rects.iter().enumerate() {
                    if rect.contains(px, py) {
                        self.hovered_button = Some(idx);
                        break;
                    }
                }
                None
            }
            PointerEvent::Down { button, .. } => {
                if button == MouseButton::Left {
                    let mut found = None;
                    {
                        let button_rects = self.button_rects.borrow();
                        for (idx, rect) in button_rects.iter().enumerate() {
                            if rect.contains(px, py) {
                                found = Some(self.dialog.buttons[idx].response);
                                break;
                            }
                        }
                    }

                    if let Some(resp) = found {
                        Some(resp)
                    } else if !dialog_rect.contains(px, py) && self.dialog.dismiss_on_backdrop {
                        Some(DialogResponse::Cancel)
                    } else {
                        None
                    }
                } else {
                    None
                }
            }
            _ => None,
        };

        if let Some(resp) = clicked_resp {
            self.close(resp);
            Some(resp)
        } else {
            None
        }
    }

    /// Dispatches a keyboard event to handle focus navigation (Tab/Shift-Tab), activation (Enter/Space), and cancellation (Escape).
    pub fn dispatch_key_event(&mut self, event: &KeyEvent) -> Option<DialogResponse> {
        if let KeyEvent::KeyDown {
            key,
            keysym,
            modifiers,
            is_repeat,
            ..
        } = event
        {
            if !*is_repeat && (*keysym == 0xff1b || key == "Escape") {
                if self.dialog.dismiss_on_escape {
                    let resp = DialogResponse::Cancel;
                    self.close(resp);
                    return Some(resp);
                }
            } else if *keysym == 0xff09 || key == "Tab" {
                let total = self.dialog.buttons.len();
                if total > 0 {
                    if modifiers.shift {
                        if self.focused_button == 0 {
                            self.focused_button = total - 1;
                        } else {
                            self.focused_button -= 1;
                        }
                    } else {
                        self.focused_button = (self.focused_button + 1) % total;
                    }
                }
            } else if !*is_repeat
                && (*keysym == 0xff0d || *keysym == 0x20 || key == "Return" || key == "Space")
            {
                if let Some(btn) = self.dialog.buttons.get(self.focused_button) {
                    let resp = btn.response;
                    self.close(resp);
                    return Some(resp);
                }
            }
        }
        None
    }

    /// Determines the cursor icon for pointer coordinates over the dialog.
    pub fn cursor_icon_at(&self, px: f32, py: f32) -> CursorIcon {
        let button_rects = self.button_rects.borrow();
        for rect in button_rects.iter() {
            if rect.contains(px, py) {
                return CursorIcon::Pointer;
            }
        }
        CursorIcon::Default
    }

    /// Generates draw primitives for the dimmed backdrop, animated dialog card, text, and buttons.
    pub fn paint(
        &self,
        win_w: f32,
        win_h: f32,
        scale: f32,
        theme: &Theme,
        atlas: &mut GlyphAtlas,
        rasterizer: &GlyphRasterizer,
        shaper: &TextShaper,
    ) -> Vec<RenderPrimitive> {
        self.update_layout(win_w, win_h, shaper);
        let dialog_rect = self.dialog_rect.get();
        let button_rects = self.button_rects.borrow();
        let mut prims = Vec::new();
        let t = self.anim_progress.value();
        if t <= 0.001 {
            return prims;
        }

        // Full-window backdrop scrim quad
        let scrim_alpha = 0.55 * t;
        prims.push(RenderPrimitive::solid_rect(
            [0.0, 0.0, win_w * scale, win_h * scale],
            [0.0, 0.0, 0.0, scrim_alpha],
            CornerRadii::all(0.0),
        ));

        // Scale transform from center: 0.94 -> 1.0
        let scale_factor = 0.94 + 0.06 * t;
        let scaled_w = dialog_rect.width * scale_factor;
        let scaled_h = dialog_rect.height * scale_factor;
        let scaled_x = dialog_rect.x + (dialog_rect.width - scaled_w) * 0.5;
        let scaled_y = dialog_rect.y + (dialog_rect.height - scaled_h) * 0.5;

        let card_bg = [0.12, 0.12, 0.12, t];
        let card_border = [0.24, 0.24, 0.24, t];
        prims.push(RenderPrimitive::bordered_rect(
            [
                scaled_x * scale,
                scaled_y * scale,
                scaled_w * scale,
                scaled_h * scale,
            ],
            card_bg,
            CornerRadii::all(10.0 * scale),
            Border::new(card_border, 1.0 * scale),
        ));

        // Content positions relative to scaled card
        let offset_x = scaled_x - dialog_rect.x;
        let offset_y = scaled_y - dialog_rect.y;

        // Title
        let pad = 24.0;
        let title_font_size = 16.0;
        let title_shaped = shaper.shape(&self.dialog.title, title_font_size);
        let title_x = scaled_x + pad;
        let title_baseline = scaled_y + pad + title_font_size * 0.85;
        let title_color = [0.95, 0.95, 0.95, t];
        crate::text::emit_text_glyphs(
            &title_shaped,
            title_x,
            title_baseline,
            title_font_size,
            title_color,
            scale,
            atlas,
            rasterizer,
            &mut prims,
        );

        // Message
        if let Some(msg) = &self.dialog.message {
            let content_w = (dialog_rect.width - pad * 2.0).max(10.0);
            let msg_font_size = 14.0;
            let line_height = 20.0;
            let lines = wrap_text(msg, content_w, msg_font_size, shaper);
            let msg_x = scaled_x + pad;
            let msg_color = [0.72, 0.72, 0.72, t];

            for (i, line) in lines.iter().enumerate() {
                if line.is_empty() {
                    continue;
                }
                let line_shaped = shaper.shape(line, msg_font_size);
                let line_baseline = scaled_y + pad + 24.0 + 12.0 + (i as f32 * line_height) + msg_font_size * 0.85;
                crate::text::emit_text_glyphs(
                    &line_shaped,
                    msg_x,
                    line_baseline,
                    msg_font_size,
                    msg_color,
                    scale,
                    atlas,
                    rasterizer,
                    &mut prims,
                );
            }
        }

        // Action buttons
        for (idx, (btn, rect)) in self.dialog.buttons.iter().zip(button_rects.iter()).enumerate() {
            let btn_x = (rect.x + offset_x) * scale;
            let btn_y = (rect.y + offset_y) * scale;
            let btn_w = rect.width * scale;
            let btn_h = rect.height * scale;

            let is_hovered = self.hovered_button == Some(idx);
            let is_focused = self.focused_button == idx;

            let (bg_color, border) = if btn.is_primary {
                let col = if is_hovered {
                    [0.26, 0.52, 0.96, t]
                } else {
                    [0.20, 0.45, 0.90, t]
                };
                let b = if is_focused {
                    Border::new(theme.colors.border_focused, 2.0 * scale)
                } else {
                    Border::new([0.30, 0.55, 1.0, 0.4 * t], 1.0 * scale)
                };
                (col, b)
            } else {
                let col = if is_hovered {
                    [0.22, 0.22, 0.22, t]
                } else {
                    [0.16, 0.16, 0.16, t]
                };
                let b = if is_focused {
                    Border::new(theme.colors.border_focused, 2.0 * scale)
                } else {
                    Border::new([0.28, 0.28, 0.28, t], 1.0 * scale)
                };
                (col, b)
            };

            prims.push(RenderPrimitive::bordered_rect(
                [btn_x, btn_y, btn_w, btn_h],
                bg_color,
                CornerRadii::all(6.0 * scale),
                border,
            ));

            let font_size = 14.0;
            let shaped_lbl = shaper.shape(&btn.label, font_size);
            let text_x = rect.x + offset_x + (rect.width - shaped_lbl.width) * 0.5;
            let cap_height = font_size * 0.70;
            let text_baseline = rect.y + offset_y + rect.height * 0.5 + cap_height * 0.5;
            let text_color = [1.0, 1.0, 1.0, t];

            crate::text::emit_text_glyphs(
                &shaped_lbl,
                text_x,
                text_baseline,
                font_size,
                text_color,
                scale,
                atlas,
                rasterizer,
                &mut prims,
            );
        }

        prims
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_dialog_templates_and_builders() {
        let alert = Dialog::alert("Notice", "Operation succeeded");
        assert_eq!(alert.title, "Notice");
        assert_eq!(alert.message.as_deref(), Some("Operation succeeded"));
        assert_eq!(alert.buttons.len(), 1);
        assert_eq!(alert.buttons[0].response, DialogResponse::Confirm);

        let confirm = Dialog::confirm("Delete File?", "Cannot be undone");
        assert_eq!(confirm.buttons.len(), 2);
        assert_eq!(confirm.buttons[0].response, DialogResponse::Cancel);
        assert_eq!(confirm.buttons[1].response, DialogResponse::Confirm);
    }

    #[test]
    fn test_modal_lifecycle_and_transitions() {
        let dialog = Dialog::confirm("Title", "Msg");
        let mut modal = ActiveModal::new(dialog);
        assert_eq!(modal.phase, ModalPhase::Entering);

        modal.tick(180.0);
        assert_eq!(modal.phase, ModalPhase::Active);
        assert!((modal.anim_progress.value() - 1.0).abs() < 1e-4);

        modal.close(DialogResponse::Confirm);
        assert_eq!(modal.phase, ModalPhase::Exiting(DialogResponse::Confirm));

        modal.tick(150.0);
        assert_eq!(modal.phase, ModalPhase::Closed);
        assert!(modal.is_closed());
    }

    #[test]
    fn test_modal_keyboard_navigation() {
        let dialog = Dialog::confirm("Title", "Msg");
        let mut modal = ActiveModal::new(dialog);
        modal.tick(180.0);

        // Initially primary button is focused (index 1)
        assert_eq!(modal.focused_button, 1);

        // Tab moves to index 0
        let tab_evt = KeyEvent::KeyDown {
            key: "Tab".to_string(),
            keysym: 0xff09,
            text: None,
            modifiers: Default::default(),
            is_repeat: false,
        };
        modal.dispatch_key_event(&tab_evt);
        assert_eq!(modal.focused_button, 0);

        // Escape triggers cancel
        let esc_evt = KeyEvent::KeyDown {
            key: "Escape".to_string(),
            keysym: 0xff1b,
            text: None,
            modifiers: Default::default(),
            is_repeat: false,
        };
        let resp = modal.dispatch_key_event(&esc_evt);
        assert_eq!(resp, Some(DialogResponse::Cancel));
        assert!(matches!(modal.phase, ModalPhase::Exiting(DialogResponse::Cancel)));
    }

    #[test]
    fn test_wrap_text_multi_line_and_long_words() {
        let shaper = TextShaper::default_system_font().expect("Font required");
        let msg = "Are you sure you want to restart the Wayland graphics pipeline?";
        
        // At 300px width, this message must wrap into multiple lines
        
        // At 300px width, this message must wrap into multiple lines
        let lines = wrap_text(msg, 300.0, 14.0, &shaper);
        assert!(lines.len() >= 2, "Expected text to wrap into at least 2 lines, got {}", lines.len());
        for line in &lines {
            let width = shaper.shape(line, 14.0).width;
            assert!(width <= 300.0, "Line '{}' width {} exceeded max_width 300", line, width);
        }

        // Test unbroken string longer than max_width
        let long_unbroken = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        let split_lines = wrap_text(long_unbroken, 100.0, 14.0, &shaper);
        assert!(split_lines.len() >= 3);
        for line in &split_lines {
            let width = shaper.shape(line, 14.0).width;
            assert!(width <= 100.0 || line.chars().count() == 1);
        }
    }

    #[test]
    fn test_dialog_layout_message_wrapping_and_height_expansion() {
        let shaper = TextShaper::default_system_font().expect("Font required");
        
        let short_dialog = Dialog::confirm("Title", "Short message");
        let short_modal = ActiveModal::new(short_dialog);
        let (short_rect, _) = short_modal.calculate_layout(800.0, 600.0, &shaper);

        let long_dialog = Dialog::confirm(
            "Title",
            "Are you sure you want to restart the Wayland graphics pipeline? This action will reload all Vulkan swapchains.",
        );
        let long_modal = ActiveModal::new(long_dialog);
        let (long_rect, _) = long_modal.calculate_layout(800.0, 600.0, &shaper);

        assert!(
            long_rect.height > short_rect.height,
            "Long wrapped message should expand dialog height (long: {}, short: {})",
            long_rect.height,
            short_rect.height
        );
    }
}
