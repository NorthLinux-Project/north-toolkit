//! Event types, pointer and keyboard input dispatching, and hit testing.

pub mod dispatcher;

pub use dispatcher::{CursorIcon, EventDispatcher, EventResponse, KeyEvent, Modifiers, MouseButton, PointerEvent, WidgetId};
