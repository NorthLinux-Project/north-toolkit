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
