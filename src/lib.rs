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
pub use event::{CursorIcon, EventDispatcher, EventResponse, KeyEvent, Modifiers, MouseButton, PointerEvent, WidgetId};
pub use layout::{
    AlignItems, AlignSelf, Dimension, EdgeInsets, FlexDirection, FlexStyle, FlexWrap,
    JustifyContent, LayoutEngine, LayoutNodeId, LayoutRect, Overflow,
};
pub use platform::{ResizeEdge, WaylandState, WaylandWindow, WindowDecorations};
pub use render::{Border, GlyphAtlas, RenderPrimitive, StrokeStyle, VectorAtlas, VectorPath, VectorRasterizer};
pub use theme::Theme;
pub use text::{emit_text_glyphs, GlyphRasterizer, RasterizedGlyph, ShapedGlyph, ShapedText, TextShaper};
pub use widget::{
    Button, Checkbox, Container, Dialog, DialogButton, DialogResponse, Dropdown, Icon, IconName,
    Image, ItemState, Label, ListItem, MenuItem, ModalHandle, PopupMenu, Property, ScrollArea, Slider,
    SortDirection, Svg, Switch, TableColumn, TableView, TextInput, Titlebar, TitlebarControl, Tooltip,
    TooltipPlacement, VirtualList, Widget, WidgetTree, WindowAction,
};

pub fn version() -> &'static str {
    env!("CARGO_PKG_VERSION")
}
