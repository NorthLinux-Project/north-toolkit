#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MouseButton {
    Left,
    Middle,
    Right,
    Other(u32),
}

#[derive(Debug, Clone, PartialEq)]
pub enum PointerEvent {
    Move { x: f32, y: f32 },
    Down { x: f32, y: f32, button: MouseButton },
    Up { x: f32, y: f32, button: MouseButton },
    Scroll { x: f32, y: f32, dx: f32, dy: f32 },
}

impl PointerEvent {
    pub fn position(&self) -> (f32, f32) {
        match *self {
            PointerEvent::Move { x, y } => (x, y),
            PointerEvent::Down { x, y, .. } => (x, y),
            PointerEvent::Up { x, y, .. } => (x, y),
            PointerEvent::Scroll { x, y, .. } => (x, y),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct Modifiers {
    pub shift: bool,
    pub ctrl: bool,
    pub alt: bool,
    pub logo: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum KeyEvent {
    KeyDown {
        key: String,
        keysym: u32,
        text: Option<String>,
        modifiers: Modifiers,
    },
    KeyUp {
        key: String,
        keysym: u32,
        modifiers: Modifiers,
    },
}

impl KeyEvent {
    pub fn key(&self) -> &str {
        match self {
            KeyEvent::KeyDown { key, .. } | KeyEvent::KeyUp { key, .. } => key,
        }
    }

    pub fn keysym(&self) -> u32 {
        match self {
            KeyEvent::KeyDown { keysym, .. } | KeyEvent::KeyUp { keysym, .. } => *keysym,
        }
    }

    pub fn modifiers(&self) -> Modifiers {
        match self {
            KeyEvent::KeyDown { modifiers, .. } | KeyEvent::KeyUp { modifiers, .. } => *modifiers,
        }
    }

    pub fn text(&self) -> Option<&str> {
        match self {
            KeyEvent::KeyDown { text, .. } => text.as_deref(),
            KeyEvent::KeyUp { .. } => None,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum EventResponse {
    #[default]
    Ignored,
    Handled,
}

impl EventResponse {
    pub fn is_handled(self) -> bool {
        self == EventResponse::Handled
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct WidgetId(pub usize);

#[derive(Default)]
pub struct EventDispatcher {
    pub hover: Option<WidgetId>,
    pub focus: Option<WidgetId>,
    pub pressed: Option<WidgetId>,
    pub mouse_pos: (f32, f32),
}

impl EventDispatcher {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn update_hover(&mut self, target: Option<WidgetId>) -> (Option<WidgetId>, Option<WidgetId>) {
        let prev = self.hover;
        self.hover = target;
        (prev, target)
    }

    pub fn set_focus(&mut self, target: Option<WidgetId>) -> Option<WidgetId> {
        let prev = self.focus;
        self.focus = target;
        prev
    }
}
