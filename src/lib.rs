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
pub use event::{
    CursorIcon, EventDispatcher, EventResponse, ImeEvent, KeyEvent, KeyRepeat, Modifiers,
    MouseButton, PointerEvent, WidgetId,
};
pub use layout::{
    AlignItems, AlignSelf, Dimension, EdgeInsets, FlexDirection, FlexStyle, FlexWrap,
    JustifyContent, LayoutEngine, LayoutNodeId, LayoutRect, Overflow,
};
pub use platform::{ResizeEdge, WaylandState, WaylandWindow, WindowDecorations};
pub use render::{Border, GlyphAtlas, RenderPrimitive, StrokeStyle, VectorAtlas, VectorPath, VectorRasterizer};
pub use theme::Theme;
pub use text::{
    emit_text_glyphs, EditBuffer, GlyphRasterizer, RasterizedGlyph, ShapedGlyph, ShapedText,
    TextShaper,
};
pub use widget::{
    Button, Checkbox, Container, Dialog, DialogButton, DialogResponse, Dropdown, Icon, IconName,
    Image, ItemState, Label, LineLayout, ListItem, MenuItem, ModalHandle, PopupMenu, ProgressBar,
    Property, RadioButton, RadioGroup, RadioGroupOrientation, ScrollArea, Slider, SortDirection,
    Spinner, SplitOrientation, SplitView, Svg, Switch, TableColumn, TableView, TabBar,
    TabBarOrientation, TabItem, Tabs, TextArea, TextInput, Titlebar, TitlebarControl, Toast,
    ToastHandle, ToastManager, ToastPhase, ToastPosition, ToastVariant, Tooltip, TooltipPlacement,
    VirtualList, Widget, WidgetTree, WindowAction,
};

pub fn version() -> &'static str {
    env!("CARGO_PKG_VERSION")
}
