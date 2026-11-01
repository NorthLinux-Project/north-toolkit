//! Input event definitions, modifier tracking, cursor shapes, and event dispatching.

/// Mouse buttons received from Wayland pointer inputs.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MouseButton {
    /// Left mouse button (primary click).
    Left,
    /// Middle mouse button (wheel click or primary selection paste).
    Middle,
    /// Right mouse button (secondary/context menu click).
    Right,
    /// Other button indexed by Linux input code.
    Other(u32),
}

/// Pointer input events routed to widgets.
#[derive(Debug, Clone, PartialEq)]
pub enum PointerEvent {
    /// Pointer moved to logical coordinates `(x, y)`.
    Move {
        /// Logical X coordinate.
        x: f32,
        /// Logical Y coordinate.
        y: f32,
    },
    /// Mouse button pressed down at `(x, y)`.
    Down {
        /// Logical X coordinate.
        x: f32,
        /// Logical Y coordinate.
        y: f32,
        /// The button pressed.
        button: MouseButton,
    },
    /// Mouse button released at `(x, y)`.
    Up {
        /// Logical X coordinate.
        x: f32,
        /// Logical Y coordinate.
        y: f32,
        /// The button released.
        button: MouseButton,
    },
    /// Mouse wheel or touchpad scroll delta at `(x, y)`.
    Scroll {
        /// Logical X coordinate.
        x: f32,
        /// Logical Y coordinate.
        y: f32,
        /// Horizontal scroll delta.
        dx: f32,
        /// Vertical scroll delta.
        dy: f32,
    },
}

impl PointerEvent {
    /// Returns the logical `(x, y)` position of this pointer event.
    pub fn position(&self) -> (f32, f32) {
        match *self {
            PointerEvent::Move { x, y } => (x, y),
            PointerEvent::Down { x, y, .. } => (x, y),
            PointerEvent::Up { x, y, .. } => (x, y),
            PointerEvent::Scroll { x, y, .. } => (x, y),
        }
    }
}

/// Keyboard modifier keys state.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct Modifiers {
    /// Shift key is active.
    pub shift: bool,
    /// Control key is active.
    pub ctrl: bool,
    /// Alt key is active.
    pub alt: bool,
    /// Logo/Super key is active.
    pub logo: bool,
}

/// Keyboard input events decoded via XKB.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum KeyEvent {
    /// Key was pressed down.
    KeyDown {
        /// Human-readable key name.
        key: String,
        /// XKB keysym code.
        keysym: u32,
        /// Optional composed UTF-8 text string.
        text: Option<String>,
        /// State of modifier keys.
        modifiers: Modifiers,
        /// Whether this event was synthesized by key auto-repeat.
        is_repeat: bool,
    },
    /// Key was released.
    KeyUp {
        /// Human-readable key name.
        key: String,
        /// XKB keysym code.
        keysym: u32,
        /// State of modifier keys.
        modifiers: Modifiers,
    },
}

impl KeyEvent {
    /// Returns the key name.
    pub fn key(&self) -> &str {
        match self {
            KeyEvent::KeyDown { key, .. } | KeyEvent::KeyUp { key, .. } => key,
        }
    }

    /// Returns the XKB keysym code.
    pub fn keysym(&self) -> u32 {
        match self {
            KeyEvent::KeyDown { keysym, .. } | KeyEvent::KeyUp { keysym, .. } => *keysym,
        }
    }

    /// Returns the modifier state during this key event.
    pub fn modifiers(&self) -> Modifiers {
        match self {
            KeyEvent::KeyDown { modifiers, .. } | KeyEvent::KeyUp { modifiers, .. } => *modifiers,
        }
    }

    /// Returns UTF-8 text composed by this key event, if any.
    pub fn text(&self) -> Option<&str> {
        match self {
            KeyEvent::KeyDown { text, .. } => text.as_deref(),
            KeyEvent::KeyUp { .. } => None,
        }
    }

    /// Returns `true` if this event is a synthesized key repeat.
    pub fn is_repeat(&self) -> bool {
        match self {
            KeyEvent::KeyDown { is_repeat, .. } => *is_repeat,
            KeyEvent::KeyUp { .. } => false,
        }
    }

    /// Returns a clone of this key event marked with `is_repeat = true`.
    pub fn as_repeat(&self) -> Self {
        match self {
            KeyEvent::KeyDown {
                key,
                keysym,
                text,
                modifiers,
                ..
            } => KeyEvent::KeyDown {
                key: key.clone(),
                keysym: *keysym,
                text: text.clone(),
                modifiers: *modifiers,
                is_repeat: true,
            },
            KeyEvent::KeyUp {
                key,
                keysym,
                modifiers,
            } => KeyEvent::KeyUp {
                key: key.clone(),
                keysym: *keysym,
                modifiers: *modifiers,
            },
        }
    }
}

/// Input Method Editor (IME) composition and commit events from `zwp_text_input_v3`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ImeEvent {
    /// Active preedit composition string and byte-indexed cursor span (`-1` hides preedit cursor).
    Preedit {
        /// Composition string to display inline with an underline (empty clears preedit).
        text: String,
        /// Start byte offset of the cursor or highlight within `text`, or `-1`.
        cursor_begin: i32,
        /// End byte offset of the cursor or highlight within `text`, or `-1`.
        cursor_end: i32,
    },
    /// Finalized text string committed by the input method.
    Commit(String),
    /// Request to delete surrounding UTF-8 bytes around the current cursor.
    DeleteSurrounding {
        /// Number of UTF-8 bytes before the cursor to delete.
        before: u32,
        /// Number of UTF-8 bytes after the cursor to delete.
        after: u32,
    },
}

#[derive(Debug, Clone)]
struct ActiveRepeat {
    keycode: u32,
    event: KeyEvent,
    next_fire: std::time::Instant,
}

/// Pure key-repeat scheduling state machine driven by `wl_keyboard::repeat_info` and monotonic timestamps.
#[derive(Debug, Clone)]
pub struct KeyRepeat {
    /// Repeat rate in keys per second (`0` disables key repeat).
    pub rate: u32,
    /// Initial delay in milliseconds before repeating begins.
    pub delay_ms: u32,
    active: Option<ActiveRepeat>,
    generation: u64,
}

impl Default for KeyRepeat {
    fn default() -> Self {
        Self::new()
    }
}

impl KeyRepeat {
    /// Creates a key-repeat state machine with standard Wayland defaults (25 Hz, 600 ms delay).
    pub fn new() -> Self {
        Self {
            rate: 25,
            delay_ms: 600,
            active: None,
            generation: 0,
        }
    }

    /// Creates a key-repeat state machine with explicit `rate` (Hz) and `delay` (ms).
    pub fn with_repeat_info(rate: i32, delay: i32) -> Self {
        let mut kr = Self::new();
        kr.set_repeat_info(rate, delay);
        kr
    }

    /// Updates repeat parameters from `wl_keyboard::Event::RepeatInfo`. A `rate <= 0` disables repeat.
    pub fn set_repeat_info(&mut self, rate: i32, delay: i32) {
        self.delay_ms = delay.max(0) as u32;
        if rate <= 0 {
            self.rate = 0;
            self.stop();
        } else {
            self.rate = rate as u32;
            if self.active.is_some() {
                self.generation = self.generation.wrapping_add(1);
            }
        }
    }

    /// Arms key repeat for a pressed key if `key_repeats` is `true` and `rate > 0`.
    pub fn on_key_press(
        &mut self,
        keycode: u32,
        event: KeyEvent,
        key_repeats: bool,
        now: std::time::Instant,
    ) {
        if !key_repeats || self.rate == 0 || !matches!(event, KeyEvent::KeyDown { .. }) {
            self.stop();
            return;
        }
        self.generation = self.generation.wrapping_add(1);
        let next_fire = now + std::time::Duration::from_millis(self.delay_ms as u64);
        self.active = Some(ActiveRepeat {
            keycode,
            event: event.as_repeat(),
            next_fire,
        });
    }

    /// Updates the synthesized `KeyEvent` payload for an active repeat when modifier masks change.
    pub fn update_active_event(&mut self, event: KeyEvent) {
        if let Some(active) = &mut self.active {
            if matches!(event, KeyEvent::KeyDown { .. }) {
                active.event = event.as_repeat();
            }
        }
    }

    /// Stops repeating if `keycode` matches the currently repeating key.
    pub fn on_key_release(&mut self, keycode: u32) {
        if self.active.as_ref().map(|a| a.keycode) == Some(keycode) {
            self.stop();
        }
    }

    /// Cancels any active key repeat (used on keyboard leave, focus loss, or keymap change).
    pub fn stop(&mut self) {
        if self.active.take().is_some() {
            self.generation = self.generation.wrapping_add(1);
        }
    }

    /// Returns `true` if a key is currently armed for repeating.
    pub fn is_active(&self) -> bool {
        self.active.is_some()
    }

    /// Returns the currently repeating hardware keycode, if any.
    pub fn active_keycode(&self) -> Option<u32> {
        self.active.as_ref().map(|a| a.keycode)
    }

    /// Returns the monotonic `Instant` when the next repeat event should fire.
    pub fn next_deadline(&self) -> Option<std::time::Instant> {
        if self.rate == 0 {
            None
        } else {
            self.active.as_ref().map(|a| a.next_fire)
        }
    }

    /// Returns a monotonic generation counter incremented whenever timer registration changes.
    pub fn generation(&self) -> u64 {
        self.generation
    }

    /// Returns the repeat interval duration for the configured `rate`.
    pub fn repeat_interval(&self) -> Option<std::time::Duration> {
        if self.rate == 0 {
            None
        } else {
            Some(std::time::Duration::from_nanos(
                (1_000_000_000u64 / self.rate as u64).max(1),
            ))
        }
    }

    /// Polls the repeat timer at `now`, returning a synthesized repeat `KeyEvent` and advancing the deadline if due.
    pub fn tick(&mut self, now: std::time::Instant) -> Option<KeyEvent> {
        if self.rate == 0 {
            self.stop();
            return None;
        }
        let interval = self.repeat_interval()?;
        let active = self.active.as_mut()?;
        if now >= active.next_fire {
            let evt = active.event.clone();
            let candidate = active.next_fire + interval;
            active.next_fire = if candidate > now {
                candidate
            } else {
                now + interval
            };
            Some(evt)
        } else {
            None
        }
    }
}

/// Dynamic mouse cursor shapes mapped to Wayland cursor shape protocol.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum CursorIcon {
    /// Standard arrow cursor.
    #[default]
    Default,
    /// Hand pointer for clickable links and buttons.
    Pointer,
    /// I-beam cursor for selectable text and inputs.
    Text,
    /// Open hand cursor for grabbable elements.
    Grab,
    /// Closed hand cursor during active dragging.
    Grabbing,
    /// Column resize double arrow (`<->`).
    ColResize,
    /// Row resize double arrow.
    RowResize,
    /// North-South vertical resize double arrow.
    NsResize,
    /// East-West horizontal resize double arrow.
    EwResize,
    /// North-West to South-East diagonal resize double arrow.
    NwseResize,
    /// North-East to South-West diagonal resize double arrow.
    NeswResize,
    /// Slashed circle indicating action is forbidden.
    NotAllowed,
    /// Precise crosshair cursor.
    Crosshair,
    /// Background operation in progress.
    Progress,
    /// Hourglass or spinner indicating system wait.
    Wait,
}

impl CursorIcon {
    /// Converts this toolkit cursor enum into the Wayland `wp_cursor_shape_device_v1::Shape`.
    pub fn to_wayland_shape(self) -> wayland_protocols::wp::cursor_shape::v1::client::wp_cursor_shape_device_v1::Shape {
        use wayland_protocols::wp::cursor_shape::v1::client::wp_cursor_shape_device_v1::Shape;
        match self {
            CursorIcon::Default => Shape::Default,
            CursorIcon::Pointer => Shape::Pointer,
            CursorIcon::Text => Shape::Text,
            CursorIcon::Grab => Shape::Grab,
            CursorIcon::Grabbing => Shape::Grabbing,
            CursorIcon::ColResize => Shape::ColResize,
            CursorIcon::RowResize => Shape::RowResize,
            CursorIcon::NsResize => Shape::NsResize,
            CursorIcon::EwResize => Shape::EwResize,
            CursorIcon::NwseResize => Shape::NwseResize,
            CursorIcon::NeswResize => Shape::NeswResize,
            CursorIcon::NotAllowed => Shape::NotAllowed,
            CursorIcon::Crosshair => Shape::Crosshair,
            CursorIcon::Progress => Shape::Progress,
            CursorIcon::Wait => Shape::Wait,
        }
    }
}

/// Indicates whether an event was consumed by a widget or should bubble up to parent widgets.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum EventResponse {
    /// Event was ignored and should bubble up.
    #[default]
    Ignored,
    /// Event was consumed by the widget.
    Handled,
}

impl EventResponse {
    /// Returns `true` if the event was handled.
    pub fn is_handled(self) -> bool {
        self == EventResponse::Handled
    }
}

/// Unique generational identifier for a widget in the `WidgetTree`.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct WidgetId(pub usize);

/// Dispatches pointer and keyboard events and tracks hovered and focused widgets.
#[derive(Default)]
pub struct EventDispatcher {
    /// Currently hovered widget identifier.
    pub hover: Option<WidgetId>,
    /// Currently focused widget identifier for keyboard input.
    pub focus: Option<WidgetId>,
    /// Currently pressed widget identifier.
    pub pressed: Option<WidgetId>,
    /// Last recorded mouse coordinates `(x, y)`.
    pub mouse_pos: (f32, f32),
}

impl EventDispatcher {
    /// Creates a new default event dispatcher.
    pub fn new() -> Self {
        Self::default()
    }

    /// Updates hovered widget and returns `(previous_hover, current_hover)`.
    pub fn update_hover(&mut self, target: Option<WidgetId>) -> (Option<WidgetId>, Option<WidgetId>) {
        let prev = self.hover;
        self.hover = target;
        (prev, target)
    }

    /// Updates focused widget and returns the previous focused widget.
    pub fn set_focus(&mut self, target: Option<WidgetId>) -> Option<WidgetId> {
        let prev = self.focus;
        self.focus = target;
        prev
    }
}

#[cfg(test)]
mod tests {
    use std::time::{Duration, Instant};

    use super::*;

    fn sample_key(ch: &str) -> KeyEvent {
        KeyEvent::KeyDown {
            key: ch.to_string(),
            keysym: ch.as_bytes()[0] as u32,
            text: Some(ch.to_string()),
            modifiers: Modifiers::default(),
            is_repeat: false,
        }
    }

    #[test]
    fn test_key_repeat_delay_then_rate_and_release() {
        let mut kr = KeyRepeat::with_repeat_info(25, 400);
        let t0 = Instant::now();
        kr.on_key_press(30, sample_key("a"), true, t0);
        assert!(kr.is_active());
        assert_eq!(kr.active_keycode(), Some(30));

        // Before delay (399 ms): no repeat
        assert!(kr.tick(t0 + Duration::from_millis(399)).is_none());

        // At delay (400 ms): first repeat fires with is_repeat = true
        let r1 = kr.tick(t0 + Duration::from_millis(400)).expect("first repeat");
        assert!(r1.is_repeat());
        assert_eq!(r1.text(), Some("a"));

        // 25 Hz => 40 ms interval: nothing at +420 ms, second repeat at +440 ms
        assert!(kr.tick(t0 + Duration::from_millis(420)).is_none());
        let r2 = kr.tick(t0 + Duration::from_millis(440)).expect("second repeat");
        assert!(r2.is_repeat());

        // Releasing a different key does not cancel active repeat
        kr.on_key_release(31);
        assert!(kr.is_active());

        // Releasing the repeating key stops repeat immediately
        kr.on_key_release(30);
        assert!(!kr.is_active());
        assert!(kr.next_deadline().is_none());
        assert!(kr.tick(t0 + Duration::from_millis(500)).is_none());
    }

    #[test]
    fn test_key_repeat_disabled_rate_focus_loss_and_non_repeating_key() {
        let t0 = Instant::now();

        // Rate 0 disables repeat completely
        let mut disabled = KeyRepeat::with_repeat_info(0, 300);
        disabled.on_key_press(30, sample_key("a"), true, t0);
        assert!(!disabled.is_active());
        assert!(disabled.next_deadline().is_none());
        assert!(disabled.tick(t0 + Duration::from_secs(1)).is_none());

        // Non-repeating key (e.g. Shift) cancels any active repeat and does not start one
        let mut kr = KeyRepeat::with_repeat_info(20, 300);
        kr.on_key_press(30, sample_key("a"), true, t0);
        assert!(kr.is_active());
        kr.on_key_press(42, sample_key("Shift_L"), false, t0 + Duration::from_millis(50));
        assert!(!kr.is_active());

        // Focus loss / keymap change stops active repeat
        kr.on_key_press(30, sample_key("b"), true, t0 + Duration::from_millis(100));
        assert!(kr.is_active());
        kr.stop();
        assert!(!kr.is_active());
        assert!(kr.tick(t0 + Duration::from_secs(2)).is_none());

        // Updating repeat_info to rate=0 mid-repeat stops active repeat
        kr.on_key_press(30, sample_key("c"), true, t0 + Duration::from_millis(200));
        assert!(kr.is_active());
        kr.set_repeat_info(0, 300);
        assert!(!kr.is_active());
    }
}
