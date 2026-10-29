pub mod animation;
pub mod app;
pub mod event;
pub mod layout;
pub mod platform;
pub mod render;
pub mod text;
pub mod theme;
pub mod widget;

pub use app::Application;
pub use event::{EventDispatcher, EventResponse, KeyEvent, Modifiers, MouseButton, PointerEvent, WidgetId};
pub use layout::{
    AlignItems, AlignSelf, Dimension, EdgeInsets, FlexDirection, FlexStyle, FlexWrap,
    JustifyContent, LayoutEngine, LayoutNodeId, LayoutRect, Overflow,
};
pub use render::{Border, GlyphAtlas, RenderPrimitive};
pub use theme::Theme;
pub use text::{emit_text_glyphs, GlyphRasterizer, RasterizedGlyph, ShapedGlyph, ShapedText, TextShaper};
pub use widget::{
    Button, Checkbox, Container, Dropdown, Image, Label, MenuItem, PopupMenu, Property, ScrollArea,
    Slider, Switch, TextInput, Widget, WidgetTree,
};

pub fn version() -> &'static str {
    env!("CARGO_PKG_VERSION")
}
