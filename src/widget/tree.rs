//! Flat widget arena, node hierarchy, event bubbling, hit testing, and overlay coordination.

use std::any::Any;

use crate::event::{
    CursorIcon, EventDispatcher, EventResponse, ImeEvent, KeyEvent, MouseButton, PointerEvent,
    WidgetId,
};
use crate::layout::{FlexStyle, LayoutEngine, LayoutNodeId, LayoutRect, MeasureFunc};
use crate::render::{Border, CornerRadii, GlyphAtlas, RenderPrimitive, VectorAtlas};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// The foundational retained widget trait implemented by all toolkit UI elements.
pub trait Widget: Any {
    /// Returns the flexbox style configuration for layout computation.
    fn layout_style(&self) -> FlexStyle;
    /// Optional intrinsic measurement function for text or custom shapes.
    fn measure(&self, _shaper: &TextShaper) -> Option<MeasureFunc> {
        None
    }
    /// Emits visual primitives rendered before child widgets (backgrounds, borders).
    fn paint_before(
        &self,
        rect: LayoutRect,
        theme: &Theme,
        atlas: &mut GlyphAtlas,
        rasterizer: &GlyphRasterizer,
        shaper: &TextShaper,
        vector_atlas: &mut VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        self.paint(rect, theme, atlas, rasterizer, shaper, vector_atlas, scale)
    }
    /// Emits visual primitives rendered after child widgets (scrollbars, foregrounds).
    fn paint_after(
        &self,
        _rect: LayoutRect,
        _theme: &Theme,
        _atlas: &mut GlyphAtlas,
        _rasterizer: &GlyphRasterizer,
        _shaper: &TextShaper,
        _vector_atlas: &mut VectorAtlas,
        _scale: f32,
    ) -> Vec<RenderPrimitive> {
        Vec::new()
    }
    /// Standard paint method for leaf widgets.
    fn paint(
        &self,
        _rect: LayoutRect,
        _theme: &Theme,
        _atlas: &mut GlyphAtlas,
        _rasterizer: &GlyphRasterizer,
        _shaper: &TextShaper,
        _vector_atlas: &mut VectorAtlas,
        _scale: f32,
    ) -> Vec<RenderPrimitive> {
        Vec::new()
    }
    /// Scroll offset `(scroll_x, scroll_y)` applied to child widgets.
    fn scroll_offset(&self) -> (f32, f32) {
        (0.0, 0.0)
    }
    /// Returns `true` if this widget accepts keyboard focus via Tab navigation.
    fn is_focusable(&self) -> bool {
        false
    }
    /// Handles pointer events (move, down, up, scroll).
    fn on_pointer_event(&mut self, _event: &PointerEvent, _rect: LayoutRect) -> EventResponse {
        EventResponse::Ignored
    }
    /// Handles keyboard events when this widget has active focus.
    fn on_key_event(&mut self, _event: &KeyEvent) -> EventResponse {
        EventResponse::Ignored
    }
    /// Handles Wayland input method composition/commit events when this widget has focus.
    fn on_ime_event(&mut self, _event: &ImeEvent) -> EventResponse {
        EventResponse::Ignored
    }
    /// Returns the surface-local logical cursor rectangle if this widget accepts IME input.
    fn ime_cursor_rect(&self, _rect: LayoutRect) -> Option<LayoutRect> {
        None
    }
    /// Returns `true` if this widget currently holds an uncommitted IME preedit composition.
    fn has_ime_preedit(&self) -> bool {
        false
    }
    /// Called when the widget gains or loses keyboard focus.
    fn on_focus_change(&mut self, _focused: bool) {}
    /// Advances internal widget animations. Returns `true` if a repaint is required.
    fn tick_animation(&mut self, _delta_ms: f32) -> bool {
        false
    }
    /// Takes any pending popup menu request triggered by this widget.
    fn take_popup_request(&mut self, _rect: LayoutRect) -> Option<super::popup::PopupMenu> {
        None
    }
    /// Called when an active popup menu initiated by this widget has closed.
    fn on_popup_closed(&mut self, _selected_index: Option<usize>) {}
    /// Takes any pending clipboard action (copy or paste request).
    fn take_clipboard_action(&mut self) -> Option<ClipboardAction> {
        None
    }
    /// Takes any pending client-side window management action.
    fn take_window_action(&mut self) -> Option<WindowAction> {
        None
    }
    /// Handles incoming text pasted from the clipboard or primary selection.
    fn on_paste(&mut self, _text: &str, _is_primary: bool) {}
    /// Returns the cursor icon shape at relative widget coordinates.
    fn cursor_icon_at(&self, _rel_x: f32, _rel_y: f32) -> CursorIcon {
        CursorIcon::Default
    }
    /// Returns tooltip configuration if hovered.
    fn tooltip(&self) -> Option<super::tooltip::Tooltip> {
        None
    }
    /// Returns `true` if this widget is visible. Inactive widgets are skipped during hit testing, focus traversal, and rendering.
    fn is_visible(&self) -> bool {
        true
    }
    /// Returns which child widget ID is active if this container manages exclusive child visibility (e.g. `Tabs`).
    fn active_child(&self) -> Option<WidgetId> {
        None
    }
    /// Allows container widgets (like `SplitView`) to override the flex layout style of their direct children.
    fn child_layout_style(
        &self,
        _child_index: usize,
        _child_id: WidgetId,
        _current_style: &FlexStyle,
    ) -> Option<FlexStyle> {
        None
    }
    /// Downcasts to `&dyn Any`.
    fn as_any(&self) -> &dyn Any;
    /// Downcasts to `&mut dyn Any`.
    fn as_any_mut(&mut self) -> &mut dyn Any;
}

/// Window management action requested by a CSD titlebar or edge hit-test.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WindowAction {
    /// Interactive window move drag.
    InteractiveMove,
    /// Interactive window resize from edge.
    InteractiveResize(crate::platform::ResizeEdge),
    /// Minimize window.
    Minimize,
    /// Toggle maximize and restore.
    ToggleMaximize,
    /// Close window.
    Close,
}

/// Performs hit testing against the 4 perimeter edges and corners for interactive window resizing.
pub fn hit_test_window_edge(
    px: f32,
    py: f32,
    win_w: f32,
    win_h: f32,
    margin: f32,
) -> crate::platform::ResizeEdge {
    let on_left = px >= 0.0 && px < margin;
    let on_right = px <= win_w && px > win_w - margin;
    let on_top = py >= 0.0 && py < margin;
    let on_bottom = py <= win_h && py > win_h - margin;

    if on_top && on_left {
        crate::platform::ResizeEdge::TopLeft
    } else if on_top && on_right {
        crate::platform::ResizeEdge::TopRight
    } else if on_bottom && on_left {
        crate::platform::ResizeEdge::BottomLeft
    } else if on_bottom && on_right {
        crate::platform::ResizeEdge::BottomRight
    } else if on_top {
        crate::platform::ResizeEdge::Top
    } else if on_bottom {
        crate::platform::ResizeEdge::Bottom
    } else if on_left {
        crate::platform::ResizeEdge::Left
    } else if on_right {
        crate::platform::ResizeEdge::Right
    } else {
        crate::platform::ResizeEdge::None
    }
}

/// Action requesting clipboard read or write.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ClipboardAction {
    /// Write text to standard clipboard.
    SetClipboard(String),
    /// Write text to primary selection.
    SetPrimary(String),
    /// Read text asynchronously from specified target buffer.
    RequestPaste(crate::platform::clipboard::SelectionTarget),
}

/// A request to open a popup menu anchored to a specific layout rect.
pub struct PopupRequest {
    /// Anchor rectangle in logical coordinates.
    pub anchor_rect: LayoutRect,
    /// Popup menu model and items.
    pub menu: super::popup::PopupMenu,
    /// Widget that initiated this popup.
    pub source_widget: WidgetId,
}

/// Node within the flat `WidgetTree` arena holding a boxed widget and its layout node reference.
pub struct WidgetNode {
    /// The boxed widget instance.
    pub widget: Box<dyn Widget>,
    /// Child widget identifiers in document order.
    pub children: Vec<WidgetId>,
    /// Parent widget identifier.
    pub parent: Option<WidgetId>,
    /// Associated node in the flexbox `LayoutEngine`.
    pub layout_node: LayoutNodeId,
    /// Whether this node requires style/measure resynchronization.
    pub dirty: bool,
    /// Whether this node is visible.
    pub visible: bool,
}

/// The flat arena storing the retained widget hierarchy, active modal, tooltips, and toasts.
pub struct WidgetTree {
    pub nodes: Vec<Option<WidgetNode>>,
    root: Option<WidgetId>,
    pub dirty: bool,
    pub pending_clipboard_actions: Vec<ClipboardAction>,
    pub pending_window_actions: Vec<WindowAction>,
    pub csd_enabled: bool,
    pub is_maximized: bool,
    pub tooltip_manager: super::tooltip::TooltipManager,
    pub active_modal: Option<super::dialog::ActiveModal>,
    pub modal_handle: super::dialog::ModalHandle,
    pub toast_manager: super::toast::ToastManager,
    pub toast_handle: super::toast::ToastHandle,
}

impl WidgetTree {
    /// Creates a new empty widget tree.
    pub fn new() -> Self {
        Self {
            nodes: Vec::new(),
            root: None,
            dirty: true,
            pending_clipboard_actions: Vec::new(),
            pending_window_actions: Vec::new(),
            csd_enabled: false,
            is_maximized: false,
            tooltip_manager: super::tooltip::TooltipManager::new(),
            active_modal: None,
            modal_handle: super::dialog::ModalHandle::new(),
            toast_manager: super::toast::ToastManager::new(),
            toast_handle: super::toast::ToastHandle::new(),
        }
    }

    /// Enqueues a window management action (minimize, maximize, move, resize).
    pub fn queue_window_action(&mut self, action: WindowAction) {
        self.pending_window_actions.push(action);
    }

    /// Takes and clears all pending window management actions.
    pub fn take_window_actions(&mut self) -> Vec<WindowAction> {
        std::mem::take(&mut self.pending_window_actions)
    }

    /// Enables or disables client-side decorations (CSD) edge hit-testing.
    pub fn set_csd_enabled(&mut self, enabled: bool) {
        self.csd_enabled = enabled;
    }

    /// Updates window maximized state.
    pub fn set_maximized(&mut self, maximized: bool) {
        self.is_maximized = maximized;
    }

    /// Clones the thread-safe `ModalHandle` for dispatching modal dialogs.
    pub fn modal_handle(&self) -> super::dialog::ModalHandle {
        self.modal_handle.clone()
    }

    /// Checks and mounts any pending modal dialog opened via `ModalHandle`.
    pub fn check_pending_modal(&mut self) {
        if let Some(dialog) = self.modal_handle.take() {
            self.open_modal(dialog);
        }
    }

    /// Mounts and opens an active modal dialog.
    pub fn open_modal(&mut self, dialog: super::dialog::Dialog) {
        for node in self.nodes.iter_mut().flatten() {
            if node.widget.has_ime_preedit() {
                node.widget.on_ime_event(&ImeEvent::Preedit {
                    text: String::new(),
                    cursor_begin: 0,
                    cursor_end: 0,
                });
            }
        }
        self.active_modal = Some(super::dialog::ActiveModal::new(dialog));
        self.dirty = true;
    }

    /// Closes the active modal dialog with the given response.
    pub fn close_modal(&mut self, response: super::dialog::DialogResponse) {
        if let Some(modal) = &mut self.active_modal {
            modal.close(response);
            self.dirty = true;
        }
    }

    /// Returns `true` if a modal dialog is currently open.
    pub fn is_modal_active(&self) -> bool {
        self.active_modal.is_some()
    }

    /// Clones the thread-safe `ToastHandle` for dispatching toast notifications.
    pub fn toast_handle(&self) -> super::toast::ToastHandle {
        self.toast_handle.clone()
    }

    /// Checks and mounts any pending toasts queued via `ToastHandle`.
    pub fn check_pending_toasts(&mut self) {
        for toast in self.toast_handle.take() {
            self.toast_manager.push(toast);
            self.dirty = true;
        }
    }

    /// Sets whether a widget node is visible. Inactive widgets are skipped during hit testing, focus, and rendering.
    pub fn set_visible(&mut self, id: WidgetId, visible: bool) {
        if let Some(Some(node)) = self.nodes.get_mut(id.0) {
            if node.visible != visible {
                node.visible = visible;
                node.dirty = true;
                self.dirty = true;
            }
        }
    }

    /// Returns `true` if the widget node exists and is visible.
    pub fn is_visible(&self, id: WidgetId) -> bool {
        self.nodes
            .get(id.0)
            .and_then(|n| n.as_ref())
            .map(|n| n.visible && n.widget.is_visible())
            .unwrap_or(false)
    }

    /// Returns the root widget identifier, if set.
    pub fn root(&self) -> Option<WidgetId> {
        self.root
    }

    /// Sets the root widget of the tree.
    pub fn set_root(&mut self, id: WidgetId) {
        self.root = Some(id);
        self.dirty = true;
    }

    /// Adds a widget to the tree arena, linking it to the flexbox `LayoutEngine`.
    pub fn add_widget(
        &mut self,
        engine: &mut LayoutEngine,
        widget: Box<dyn Widget>,
        parent: Option<WidgetId>,
    ) -> WidgetId {
        let style = widget.layout_style();
        let layout_node = engine.create_node(style);

        let node = WidgetNode {
            widget,
            children: Vec::new(),
            parent,
            layout_node,
            dirty: true,
            visible: true,
        };

        let mut empty_index = None;
        for (i, slot) in self.nodes.iter().enumerate() {
            if slot.is_none() {
                empty_index = Some(i);
                break;
            }
        }

        let widget_id = if let Some(i) = empty_index {
            self.nodes[i] = Some(node);
            WidgetId(i)
        } else {
            let i = self.nodes.len();
            self.nodes.push(Some(node));
            WidgetId(i)
        };

        if let Some(parent_id) = parent {
            if let Some(Some(parent_node)) = self.nodes.get_mut(parent_id.0) {
                parent_node.children.push(widget_id);
                engine.add_child(parent_node.layout_node, layout_node);
            }
        } else if self.root.is_none() {
            self.root = Some(widget_id);
        }

        self.dirty = true;
        widget_id
    }

    /// Downcasts and returns a reference to widget `T` at `id`.
    pub fn get_widget<T: Widget>(&self, id: WidgetId) -> Option<&T> {
        self.nodes
            .get(id.0)
            .and_then(|n| n.as_ref())
            .and_then(|n| n.widget.as_any().downcast_ref::<T>())
    }

    /// Downcasts and returns a mutable reference to widget `T` at `id`.
    pub fn get_widget_mut<T: Widget>(&mut self, id: WidgetId) -> Option<&mut T> {
        self.nodes
            .get_mut(id.0)
            .and_then(|n| n.as_mut())
            .and_then(|n| n.widget.as_any_mut().downcast_mut::<T>())
    }

    /// Marks a specific widget as requiring layout update.
    pub fn mark_dirty(&mut self, id: WidgetId) {
        if let Some(Some(node)) = self.nodes.get_mut(id.0) {
            node.dirty = true;
        }
        self.dirty = true;
    }

    /// Returns `true` if the tree or any child node requires relayout or repaint.
    pub fn is_dirty(&self) -> bool {
        self.dirty
    }

    /// Synchronizes flex styles and intrinsic measurements to the flexbox `LayoutEngine`.
    pub fn sync_layout(&mut self, engine: &mut LayoutEngine, shaper: &TextShaper) {
        let zero_style = FlexStyle {
            width: crate::layout::Dimension::Points(0.0),
            height: crate::layout::Dimension::Points(0.0),
            min_width: Some(0.0),
            min_height: Some(0.0),
            max_width: Some(0.0),
            max_height: Some(0.0),
            padding: crate::layout::EdgeInsets::ZERO,
            margin: crate::layout::EdgeInsets::ZERO,
            gap: 0.0,
            ..Default::default()
        };

        for node in self.nodes.iter_mut().flatten() {
            if !node.visible || !node.widget.is_visible() {
                engine.set_style(node.layout_node, zero_style.clone());
                engine.set_measure(node.layout_node, Box::new(|_, _| (0.0, 0.0)));
                node.dirty = false;
            } else if node.dirty {
                engine.set_style(node.layout_node, node.widget.layout_style());
                if let Some(measure_fn) = node.widget.measure(shaper) {
                    engine.set_measure(node.layout_node, measure_fn);
                }
                node.dirty = false;
            }
        }

        let mut child_overrides = Vec::new();
        let mut active_children_to_restore = Vec::new();
        let mut inactive_children_to_zero = Vec::new();

        for slot in self.nodes.iter().flatten() {
            if let Some(active_id) = slot.widget.active_child() {
                for &child_id in &slot.children {
                    if child_id == active_id {
                        active_children_to_restore.push(child_id);
                    } else {
                        inactive_children_to_zero.push(child_id);
                    }
                }
            }

            for (child_idx, &child_id) in slot.children.iter().enumerate() {
                if let Some(Some(child_node)) = self.nodes.get(child_id.0) {
                    let base_style = child_node.widget.layout_style();
                    if let Some(style) =
                        slot.widget.child_layout_style(child_idx, child_id, &base_style)
                    {
                        child_overrides.push((child_id, style));
                    }
                }
            }
        }

        for active_id in active_children_to_restore {
            if let Some(Some(child_node)) = self.nodes.get(active_id.0) {
                if child_node.visible && child_node.widget.is_visible() {
                    engine.set_style(child_node.layout_node, child_node.widget.layout_style());
                    if let Some(measure_fn) = child_node.widget.measure(shaper) {
                        engine.set_measure(child_node.layout_node, measure_fn);
                    } else {
                        engine.clear_measure(child_node.layout_node);
                    }
                }
            }
        }

        for inactive_id in inactive_children_to_zero {
            if let Some(Some(child_node)) = self.nodes.get(inactive_id.0) {
                engine.set_style(child_node.layout_node, zero_style.clone());
                engine.clear_measure(child_node.layout_node);
            }
        }

        for (child_id, style) in child_overrides {
            if let Some(Some(child_node)) = self.nodes.get(child_id.0) {
                engine.set_style(child_node.layout_node, style);
            }
        }

        self.dirty = false;
    }

    /// Advances running animations across all widgets and overlays. Returns `true` if repaint is needed.
    pub fn tick_animations(&mut self, delta_ms: f32) -> bool {
        self.check_pending_modal();
        self.check_pending_toasts();
        let mut needs_repaint = false;
        for node in self.nodes.iter_mut().flatten() {
            if node.widget.tick_animation(delta_ms) {
                needs_repaint = true;
            }
        }
        if self.tooltip_manager.tick(delta_ms) {
            needs_repaint = true;
        }
        if self.toast_manager.tick(delta_ms) {
            needs_repaint = true;
        }
        if let Some(modal) = &mut self.active_modal {
            if modal.tick(delta_ms) {
                needs_repaint = true;
            }
            if modal.is_closed() {
                self.active_modal = None;
                needs_repaint = true;
            }
        }
        if needs_repaint {
            self.dirty = true;
        }
        needs_repaint
    }

    /// Performs reverse depth-order hit testing against computed widget rectangles.
    pub fn hit_test(&self, engine: &LayoutEngine, px: f32, py: f32) -> Option<WidgetId> {
        let root = self.root?;
        self.hit_test_recursive(root, engine, px, py, (0.0, 0.0))
    }

    fn hit_test_recursive(
        &self,
        current: WidgetId,
        engine: &LayoutEngine,
        px: f32,
        py: f32,
        offset: (f32, f32),
    ) -> Option<WidgetId> {
        let node = self.nodes.get(current.0)?.as_ref()?;
        if !node.visible || !node.widget.is_visible() {
            return None;
        }

        let rect = engine.get_rect(node.layout_node);
        let screen_rect = LayoutRect::new(
            rect.x + offset.0,
            rect.y + offset.1,
            rect.width,
            rect.height,
        );

        if !screen_rect.contains(px, py) {
            return None;
        }

        let scroll = node.widget.scroll_offset();
        let child_offset = (offset.0 - scroll.0, offset.1 - scroll.1);

        if let Some(active_id) = node.widget.active_child() {
            if node.children.contains(&active_id) {
                if let Some(hit) = self.hit_test_recursive(active_id, engine, px, py, child_offset) {
                    return Some(hit);
                }
            }
        } else {
            for &child in node.children.iter().rev() {
                if let Some(hit) = self.hit_test_recursive(child, engine, px, py, child_offset) {
                    return Some(hit);
                }
            }
        }

        Some(current)
    }

    /// Dispatches a pointer event through the tree with hit testing, hover tracking, and bubbling.
    pub fn dispatch_pointer_event(
        &mut self,
        dispatcher: &mut EventDispatcher,
        engine: &LayoutEngine,
        event: &PointerEvent,
    ) -> EventResponse {
        self.check_pending_modal();
        self.check_pending_toasts();
        let (px, py) = event.position();
        dispatcher.mouse_pos = (px, py);

        if let Some(modal) = &mut self.active_modal {
            let _ = modal.dispatch_pointer_event(event);
            self.dirty = true;
            return EventResponse::Handled;
        }

        let (win_w, win_h) = self
            .root
            .and_then(|r| self.nodes.get(r.0).and_then(|n| n.as_ref()))
            .map(|n| {
                let r = engine.get_rect(n.layout_node);
                (r.width.max(100.0), r.height.max(100.0))
            })
            .unwrap_or((800.0, 600.0));

        if !self.toast_manager.is_empty() {
            let toast_resp = self.toast_manager.dispatch_pointer_event(event, win_w, win_h);
            if toast_resp == EventResponse::Handled {
                if let Some(prev) = dispatcher.hover.take() {
                    let rect = self.node_screen_rect(prev, engine);
                    if let Some(Some(node)) = self.nodes.get_mut(prev.0) {
                        let _ = node.widget.on_pointer_event(event, rect);
                    }
                }
                self.tooltip_manager.handle_hover_change(None, LayoutRect::ZERO, None);
                self.dirty = true;
                return EventResponse::Handled;
            }
        }

        if self.csd_enabled && !self.is_maximized {
            if let Some(root) = self.root {
                let root_rect = self.node_screen_rect(root, engine);
                let edge = hit_test_window_edge(px, py, root_rect.width, root_rect.height, 6.0);
                if edge != crate::platform::ResizeEdge::None {
                    if let PointerEvent::Down { button: MouseButton::Left, .. } = *event {
                        self.queue_window_action(WindowAction::InteractiveResize(edge));
                        return EventResponse::Handled;
                    }
                }
            }
        }

        let hit = self.hit_test(engine, px, py);

        match *event {
            PointerEvent::Move { .. } => {
                let mut resp = EventResponse::Ignored;

                if let Some(pressed_id) = dispatcher.pressed {
                    let rect = self.node_screen_rect(pressed_id, engine);
                    if let Some(Some(node)) = self.nodes.get_mut(pressed_id.0) {
                        if node.widget.on_pointer_event(event, rect) == EventResponse::Handled {
                            self.dirty = true;
                            resp = EventResponse::Handled;
                        }
                    }
                }

                let (prev_hover, new_hover) = dispatcher.update_hover(hit);
                dispatcher.mouse_pos = (px, py);

                if prev_hover != new_hover {
                    if let Some(prev) = prev_hover {
                        let rect = self.node_screen_rect(prev, engine);
                        if let Some(Some(node)) = self.nodes.get_mut(prev.0) {
                            if node.widget.on_pointer_event(event, rect) == EventResponse::Handled {
                                resp = EventResponse::Handled;
                            }
                        }
                    }
                    self.dirty = true;
                }

                if let Some(target) = new_hover {
                    if Some(target) != dispatcher.pressed {
                        let rect = self.node_screen_rect(target, engine);
                        if let Some(Some(node)) = self.nodes.get_mut(target.0) {
                            if node.widget.on_pointer_event(event, rect) == EventResponse::Handled {
                                resp = EventResponse::Handled;
                            }
                        }
                    }
                }

                let (hover_rect, tooltip_opt) = if let Some(target) = new_hover {
                    let rect = self.node_screen_rect(target, engine);
                    let tt = self
                        .nodes
                        .get(target.0)
                        .and_then(|n| n.as_ref())
                        .and_then(|n| n.widget.tooltip());
                    (rect, tt)
                } else {
                    (LayoutRect::ZERO, None)
                };
                self.tooltip_manager
                    .handle_hover_change(new_hover, hover_rect, tooltip_opt);

                return resp;
            }
            PointerEvent::Down { .. } => {
                self.tooltip_manager.handle_pointer_down();

                let prev_focus = dispatcher.set_focus(hit);
                if prev_focus != hit {
                    if let Some(prev) = prev_focus {
                        if let Some(Some(node)) = self.nodes.get_mut(prev.0) {
                            node.widget.on_focus_change(false);
                        }
                    }
                    if let Some(curr) = hit {
                        if let Some(Some(node)) = self.nodes.get_mut(curr.0) {
                            node.widget.on_focus_change(true);
                        }
                    }
                    self.dirty = true;
                }

                let mut curr = hit;
                while let Some(target) = curr {
                    let rect = self.node_screen_rect(target, engine);
                    let (handled, parent, act) = if let Some(Some(node)) = self.nodes.get_mut(target.0) {
                        let r = node.widget.on_pointer_event(event, rect);
                        let act = node.widget.take_window_action();
                        (r == EventResponse::Handled, node.parent, act)
                    } else {
                        (false, None, None)
                    };

                    if let Some(action) = act {
                        self.queue_window_action(action);
                    }

                    if handled {
                        dispatcher.pressed = Some(target);
                        self.dirty = true;
                        return EventResponse::Handled;
                    }
                    curr = parent;
                }

                dispatcher.pressed = hit;
                return EventResponse::Ignored;
            }
            PointerEvent::Up { .. } => {
                let pressed = dispatcher.pressed.take();
                let mut resp = EventResponse::Ignored;
                if let Some(target) = pressed.or(hit) {
                    let rect = self.node_screen_rect(target, engine);
                    if let Some(Some(node)) = self.nodes.get_mut(target.0) {
                        resp = node.widget.on_pointer_event(event, rect);
                        if let Some(act) = node.widget.take_window_action() {
                            self.queue_window_action(act);
                        }
                    }
                }
                if resp == EventResponse::Handled || pressed.is_some() {
                    self.dirty = true;
                }
                return resp;
            }
            PointerEvent::Scroll { .. } => {
                let mut curr = hit;
                while let Some(target) = curr {
                    let rect = self.node_screen_rect(target, engine);
                    let (handled, parent) = if let Some(Some(node)) = self.nodes.get_mut(target.0) {
                        let r = node.widget.on_pointer_event(event, rect);
                        (r == EventResponse::Handled, node.parent)
                    } else {
                        (false, None)
                    };

                    if handled {
                        self.dirty = true;
                        return EventResponse::Handled;
                    }
                    curr = parent;
                }
                return EventResponse::Ignored;
            }
        }
    }

    /// Collects all focusable widget IDs in preorder depth traversal.
    pub fn focusable_widgets(&self) -> Vec<WidgetId> {
        let mut list = Vec::new();
        if let Some(root_id) = self.root {
            self.collect_focusable_recursive(root_id, &mut list);
        }
        list
    }

    fn collect_focusable_recursive(&self, current: WidgetId, list: &mut Vec<WidgetId>) {
        if let Some(Some(node)) = self.nodes.get(current.0) {
            if !node.visible || !node.widget.is_visible() {
                return;
            }
            if node.widget.is_focusable() {
                list.push(current);
            }
            if let Some(active_id) = node.widget.active_child() {
                if node.children.contains(&active_id) {
                    self.collect_focusable_recursive(active_id, list);
                }
            } else {
                for &child in &node.children {
                    self.collect_focusable_recursive(child, list);
                }
            }
        }
    }

    /// Dispatches keyboard events with Tab focus cycling and active focus routing.
    pub fn dispatch_key_event(
        &mut self,
        dispatcher: &mut EventDispatcher,
        event: &KeyEvent,
    ) -> EventResponse {
        self.check_pending_modal();
        if let Some(modal) = &mut self.active_modal {
            let _ = modal.dispatch_key_event(event);
            self.dirty = true;
            return EventResponse::Handled;
        }

        if let KeyEvent::KeyDown { key, modifiers, .. } = event {
            if key == "Tab" && !self.focused_has_ime_preedit(dispatcher) {
                let focusable = self.focusable_widgets();
                if !focusable.is_empty() {
                    let current_idx = dispatcher
                        .focus
                        .and_then(|f| focusable.iter().position(|&id| id == f));
                    let next_id = match current_idx {
                        Some(idx) => {
                            if modifiers.shift {
                                if idx == 0 {
                                    focusable[focusable.len() - 1]
                                } else {
                                    focusable[idx - 1]
                                }
                            } else {
                                focusable[(idx + 1) % focusable.len()]
                            }
                        }
                        None => focusable[0],
                    };

                    let prev_focus = dispatcher.set_focus(Some(next_id));
                    if let Some(prev) = prev_focus {
                        if let Some(Some(node)) = self.nodes.get_mut(prev.0) {
                            node.widget.on_focus_change(false);
                        }
                    }
                    if let Some(Some(node)) = self.nodes.get_mut(next_id.0) {
                        node.widget.on_focus_change(true);
                    }
                    self.dirty = true;
                    return EventResponse::Handled;
                }
            }
        }

        if let Some(focus_id) = dispatcher.focus {
            if let Some(Some(node)) = self.nodes.get_mut(focus_id.0) {
                let resp = node.widget.on_key_event(event);
                if resp == EventResponse::Handled {
                    self.dirty = true;
                }
                return resp;
            }
        }
        EventResponse::Ignored
    }

    /// Dispatches IME composition/commit events to the currently focused widget.
    pub fn dispatch_ime_event(
        &mut self,
        dispatcher: &EventDispatcher,
        event: &ImeEvent,
    ) -> EventResponse {
        self.check_pending_modal();
        if self.active_modal.is_some() {
            return EventResponse::Ignored;
        }
        if let Some(focus_id) = dispatcher.focus {
            if let Some(Some(node)) = self.nodes.get_mut(focus_id.0) {
                let resp = node.widget.on_ime_event(event);
                if resp == EventResponse::Handled {
                    self.dirty = true;
                }
                return resp;
            }
        }
        EventResponse::Ignored
    }

    /// Queries the surface-local logical cursor rectangle for the focused widget if IME-capable.
    pub fn ime_cursor_rect(
        &self,
        engine: &LayoutEngine,
        dispatcher: &EventDispatcher,
    ) -> Option<LayoutRect> {
        if self.active_modal.is_some() {
            return None;
        }
        let focus_id = dispatcher.focus?;
        let node = self.nodes.get(focus_id.0)?.as_ref()?;
        let screen_rect = self.node_screen_rect(focus_id, engine);
        node.widget.ime_cursor_rect(screen_rect)
    }

    /// Returns `true` if the currently focused widget holds an uncommitted IME preedit composition.
    pub fn focused_has_ime_preedit(&self, dispatcher: &EventDispatcher) -> bool {
        dispatcher
            .focus
            .and_then(|id| self.nodes.get(id.0))
            .and_then(|slot| slot.as_ref())
            .map(|node| node.widget.has_ime_preedit())
            .unwrap_or(false)
    }

    /// Clears any active IME preedit composition on the currently focused widget.
    pub fn clear_focused_ime_preedit(&mut self, dispatcher: &EventDispatcher) -> bool {
        if let Some(focus_id) = dispatcher.focus {
            if let Some(Some(node)) = self.nodes.get_mut(focus_id.0) {
                if node.widget.has_ime_preedit() {
                    node.widget.on_ime_event(&ImeEvent::Preedit {
                        text: String::new(),
                        cursor_begin: 0,
                        cursor_end: 0,
                    });
                    self.dirty = true;
                    return true;
                }
            }
        }
        false
    }

    /// Takes any popup request emitted by child widgets during event dispatching.
    pub fn take_pending_popup_request(&mut self, engine: &LayoutEngine) -> Option<PopupRequest> {
        for (idx, slot) in self.nodes.iter_mut().enumerate() {
            if let Some(node) = slot.as_mut() {
                let rect = engine.get_rect(node.layout_node);
                if let Some(menu) = node.widget.take_popup_request(rect) {
                    let anchor = self.node_screen_rect(WidgetId(idx), engine);
                    return Some(PopupRequest {
                        anchor_rect: anchor,
                        menu,
                        source_widget: WidgetId(idx),
                    });
                }
            }
        }
        None
    }

    /// Notifies the source widget that its active popup has been dismissed.
    pub fn notify_popup_closed(&mut self, source: WidgetId, selected_index: Option<usize>) {
        if let Some(Some(node)) = self.nodes.get_mut(source.0) {
            node.widget.on_popup_closed(selected_index);
            self.dirty = true;
        }
    }

    /// Collects and clears all pending clipboard actions across the widget tree.
    pub fn take_clipboard_actions(&mut self) -> Vec<ClipboardAction> {
        let mut actions = std::mem::take(&mut self.pending_clipboard_actions);
        for slot in &mut self.nodes {
            if let Some(node) = slot {
                if let Some(act) = node.widget.take_clipboard_action() {
                    actions.push(act);
                }
            }
        }
        actions
    }

    /// Dispatches pasted text from the clipboard or primary selection to the focused widget.
    pub fn dispatch_paste(&mut self, dispatcher: &EventDispatcher, text: &str, is_primary: bool) {
        if let Some(focus_id) = dispatcher.focus {
            if let Some(Some(node)) = self.nodes.get_mut(focus_id.0) {
                node.widget.on_paste(text, is_primary);
                self.dirty = true;
            }
        }
    }

    /// Computes the active cursor icon shape at the current mouse position.
    pub fn query_cursor_icon(&self, engine: &LayoutEngine, dispatcher: &EventDispatcher) -> CursorIcon {
        let (px, py) = dispatcher.mouse_pos;

        let (win_w, win_h) = self
            .root
            .and_then(|r| self.nodes.get(r.0).and_then(|n| n.as_ref()))
            .map(|n| {
                let r = engine.get_rect(n.layout_node);
                (r.width.max(100.0), r.height.max(100.0))
            })
            .unwrap_or((800.0, 600.0));

        if let Some(icon) = self.toast_manager.cursor_icon_at(px, py, win_w, win_h) {
            return icon;
        }

        if let Some(modal) = &self.active_modal {
            return modal.cursor_icon_at(px, py);
        }

        if self.csd_enabled && !self.is_maximized {
            if let Some(root) = self.root {
                let root_rect = self.node_screen_rect(root, engine);
                let (px, py) = dispatcher.mouse_pos;
                let edge = hit_test_window_edge(px, py, root_rect.width, root_rect.height, 6.0);
                if let Some(shape) = edge.cursor_icon() {
                    return shape;
                }
            }
        }

        if let Some(target_id) = dispatcher.pressed.or(dispatcher.hover) {
            let screen_rect = self.node_screen_rect(target_id, engine);
            if let Some(Some(node)) = self.nodes.get(target_id.0) {
                let rel_x = dispatcher.mouse_pos.0 - screen_rect.x;
                let rel_y = dispatcher.mouse_pos.1 - screen_rect.y;
                return node.widget.cursor_icon_at(rel_x, rel_y);
            }
        }
        CursorIcon::Default
    }

    /// Computes the absolute screen rectangle of a widget by traversing parent scroll offsets.
    pub fn node_screen_rect(&self, id: WidgetId, engine: &LayoutEngine) -> LayoutRect {
        let node = match self.nodes.get(id.0).and_then(|n| n.as_ref()) {
            Some(n) => n,
            None => return LayoutRect::ZERO,
        };
        let rect = engine.get_rect(node.layout_node);
        let mut offset_x = 0.0f32;
        let mut offset_y = 0.0f32;
        let mut curr_parent = node.parent;
        while let Some(pid) = curr_parent {
            if let Some(Some(parent_node)) = self.nodes.get(pid.0) {
                let (sx, sy) = parent_node.widget.scroll_offset();
                offset_x -= sx;
                offset_y -= sy;
                curr_parent = parent_node.parent;
            } else {
                break;
            }
        }
        LayoutRect::new(rect.x + offset_x, rect.y + offset_y, rect.width, rect.height)
    }

    /// Traverses the widget tree and overlays, generating a batch of render primitives for the frame.
    pub fn paint(
        &self,
        engine: &LayoutEngine,
        theme: &Theme,
        atlas: &mut GlyphAtlas,
        rasterizer: &GlyphRasterizer,
        shaper: &TextShaper,
        vector_atlas: &mut VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let mut primitives = Vec::new();
        if let Some(root_id) = self.root {
            self.paint_recursive(
                root_id,
                engine,
                theme,
                atlas,
                rasterizer,
                shaper,
                vector_atlas,
                scale,
                (0.0, 0.0),
                &mut primitives,
            );
        }

        if let Some(modal) = &self.active_modal {
            let (win_w, win_h) = self
                .root
                .and_then(|r| self.nodes.get(r.0).and_then(|n| n.as_ref()))
                .map(|n| {
                    let r = engine.get_rect(n.layout_node);
                    (r.width.max(100.0), r.height.max(100.0))
                })
                .unwrap_or((800.0, 600.0));

            let modal_prims = modal.paint(
                win_w,
                win_h,
                scale,
                theme,
                atlas,
                rasterizer,
                shaper,
            );
            primitives.extend(modal_prims);
        } else if self.tooltip_manager.is_visible() {
            if let Some(tt) = &self.tooltip_manager.active_tooltip {
                let alpha = self.tooltip_manager.current_alpha();
                let font_size = 12.0;
                let shaped = shaper.shape(&tt.text, font_size);
                let pad_h = 8.0;
                let pad_v = 4.0;
                let box_w = shaped.width + pad_h * 2.0;
                let box_h = shaped.height.max(16.0) + pad_v * 2.0;

                let (win_w, win_h) = self
                    .root
                    .and_then(|r| self.nodes.get(r.0).and_then(|n| n.as_ref()))
                    .map(|n| {
                        let r = engine.get_rect(n.layout_node);
                        (r.width.max(100.0), r.height.max(100.0))
                    })
                    .unwrap_or((800.0, 600.0));

                let box_rect =
                    self.tooltip_manager
                        .calculate_bounds(box_w, box_h, win_w, win_h);

                let bg_color = [0.10, 0.10, 0.10, 0.95 * alpha];
                let border_color = [0.25, 0.25, 0.25, alpha];
                primitives.push(RenderPrimitive::bordered_rect(
                    [
                        box_rect.x * scale,
                        box_rect.y * scale,
                        box_rect.width * scale,
                        box_rect.height * scale,
                    ],
                    bg_color,
                    CornerRadii::all(4.0 * scale),
                    Border::new(border_color, 1.0 * scale),
                ));

                let text_x = box_rect.x + pad_h;
                let cap_height = font_size * 0.70;
                let baseline = box_rect.y + box_rect.height * 0.5 + cap_height * 0.5;
                let text_color = [0.95, 0.95, 0.95, alpha];

                crate::text::emit_text_glyphs(
                    &shaped,
                    text_x,
                    baseline,
                    font_size,
                    text_color,
                    scale,
                    atlas,
                    rasterizer,
                    &mut primitives,
                );
            }
        }

        if !self.toast_manager.is_empty() {
            let (win_w, win_h) = self
                .root
                .and_then(|r| self.nodes.get(r.0).and_then(|n| n.as_ref()))
                .map(|n| {
                    let r = engine.get_rect(n.layout_node);
                    (r.width.max(100.0), r.height.max(100.0))
                })
                .unwrap_or((800.0, 600.0));

            let toast_prims = self.toast_manager.paint(
                win_w,
                win_h,
                scale,
                theme,
                atlas,
                rasterizer,
                shaper,
                vector_atlas,
            );
            primitives.extend(toast_prims);
        }

        primitives
    }

    fn paint_recursive(
        &self,
        current: WidgetId,
        engine: &LayoutEngine,
        theme: &Theme,
        atlas: &mut GlyphAtlas,
        rasterizer: &GlyphRasterizer,
        shaper: &TextShaper,
        vector_atlas: &mut VectorAtlas,
        scale: f32,
        offset: (f32, f32),
        primitives: &mut Vec<RenderPrimitive>,
    ) {
        let node = match self.nodes.get(current.0).and_then(|n| n.as_ref()) {
            Some(n) => n,
            None => return,
        };

        if !node.visible || !node.widget.is_visible() {
            return;
        }

        let rect = engine.get_rect(node.layout_node);
        let screen_rect = LayoutRect::new(
            rect.x + offset.0,
            rect.y + offset.1,
            rect.width,
            rect.height,
        );

        let mut before_prims = node.widget.paint_before(
            screen_rect,
            theme,
            atlas,
            rasterizer,
            shaper,
            vector_atlas,
            scale,
        );
        primitives.append(&mut before_prims);

        let scroll = node.widget.scroll_offset();
        let child_offset = (offset.0 - scroll.0, offset.1 - scroll.1);

        if let Some(active_id) = node.widget.active_child() {
            if node.children.contains(&active_id) {
                self.paint_recursive(
                    active_id,
                    engine,
                    theme,
                    atlas,
                    rasterizer,
                    shaper,
                    vector_atlas,
                    scale,
                    child_offset,
                    primitives,
                );
            }
        } else {
            for &child in &node.children {
                self.paint_recursive(
                    child,
                    engine,
                    theme,
                    atlas,
                    rasterizer,
                    shaper,
                    vector_atlas,
                    scale,
                    child_offset,
                    primitives,
                );
            }
        }

        let mut after_prims = node.widget.paint_after(
            screen_rect,
            theme,
            atlas,
            rasterizer,
            shaper,
            vector_atlas,
            scale,
        );
        primitives.append(&mut after_prims);
    }
}

impl Default for WidgetTree {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use std::sync::atomic::{AtomicBool, Ordering};
    use std::sync::Arc;

    use super::*;
    use crate::event::MouseButton;
    use crate::layout::Dimension;
    use crate::widget::button::Button;
    use crate::widget::container::Container;
    use crate::widget::label::Label;

    #[test]
    fn test_widget_tree_layout_hit_and_click() {
        let shaper = TextShaper::default_system_font().expect("Failed font load");
        let rasterizer = GlyphRasterizer::new(shaper.font_db().clone());
        let mut atlas = GlyphAtlas::new(512, 512, 1);
        let theme = Theme::dark();

        let mut engine = LayoutEngine::new();
        let mut tree = WidgetTree::new();
        let mut dispatcher = EventDispatcher::new();

        let container = Container::new(FlexStyle {
            width: Dimension::Points(400.0),
            height: Dimension::Points(300.0),
            ..Default::default()
        });
        let root = tree.add_widget(&mut engine, Box::new(container), None);

        let clicked = Arc::new(AtomicBool::new(false));
        let clicked_clone = clicked.clone();

        let button = Button::new("Click Me").on_click(move || {
            clicked_clone.store(true, Ordering::SeqCst);
        });
        let btn_id = tree.add_widget(&mut engine, Box::new(button), Some(root));

        let label = Label::new("Hello Toolkit");
        let lbl_id = tree.add_widget(&mut engine, Box::new(label), Some(root));

        assert!(tree.is_dirty());
        tree.sync_layout(&mut engine, &shaper);
        assert!(!tree.is_dirty());

        let root_layout_node = tree.nodes[root.0].as_ref().unwrap().layout_node;
        engine.compute(root_layout_node, 400.0, 300.0);

        let btn_rect = engine.get_rect(tree.nodes[btn_id.0].as_ref().unwrap().layout_node);
        assert!(btn_rect.width > 20.0);
        assert!(btn_rect.height > 10.0);

        let hit = tree.hit_test(&engine, btn_rect.x + 5.0, btn_rect.y + 5.0);
        assert_eq!(hit, Some(btn_id));

        let hit_lbl = tree.hit_test(&engine, btn_rect.x + btn_rect.width + 5.0, btn_rect.y + 5.0);
        assert_eq!(hit_lbl, Some(lbl_id));

        let press_evt = PointerEvent::Down {
            x: btn_rect.x + 5.0,
            y: btn_rect.y + 5.0,
            button: MouseButton::Left,
        };
        tree.dispatch_pointer_event(&mut dispatcher, &engine, &press_evt);
        assert_eq!(dispatcher.focus, Some(btn_id));

        let release_evt = PointerEvent::Up {
            x: btn_rect.x + 5.0,
            y: btn_rect.y + 5.0,
            button: MouseButton::Left,
        };
        tree.dispatch_pointer_event(&mut dispatcher, &engine, &release_evt);
        assert!(clicked.load(Ordering::SeqCst), "Button click callback did not fire");

        let mut vector_atlas = VectorAtlas::new(256, 256);
        let prims = tree.paint(&engine, &theme, &mut atlas, &rasterizer, &shaper, &mut vector_atlas, 1.0);
        assert!(!prims.is_empty());
    }

    #[test]
    fn test_hello_world_layout_and_hover() {
        use crate::layout::{AlignItems, EdgeInsets, FlexDirection, JustifyContent};
        use crate::widget::TextInput;
        let shaper = TextShaper::default_system_font().expect("Failed font load");
        let mut engine = LayoutEngine::new();
        let mut tree = WidgetTree::new();
        let mut dispatcher = EventDispatcher::new();

        let root_container = Container::new(FlexStyle {
            direction: FlexDirection::Column,
            justify_content: JustifyContent::FlexStart,
            align_items: AlignItems::Stretch,
            padding: EdgeInsets::all(24.0),
            gap: 16.0,
            width: Dimension::Percent(100.0),
            height: Dimension::Percent(100.0),
            ..Default::default()
        });
        let root = tree.add_widget(&mut engine, Box::new(root_container), None);

        let title = Label::new("North Toolkit").with_font_size(24.0);
        tree.add_widget(&mut engine, Box::new(title), Some(root));

        let subtitle = Label::new("Sub").with_font_size(14.0);
        tree.add_widget(&mut engine, Box::new(subtitle), Some(root));

        let input = TextInput::new("Type...").with_width(320.0);
        let input_id = tree.add_widget(&mut engine, Box::new(input), Some(root));

        let button_row = Container::new(FlexStyle {
            direction: FlexDirection::Row,
            gap: 12.0,
            align_items: AlignItems::Center,
            ..Default::default()
        });
        let row_id = tree.add_widget(&mut engine, Box::new(button_row), Some(root));

        let button = Button::new("Click Counter");
        let btn_id = tree.add_widget(&mut engine, Box::new(button), Some(row_id));

        tree.sync_layout(&mut engine, &shaper);
        let root_layout = tree.nodes[root.0].as_ref().unwrap().layout_node;
        engine.compute(root_layout, 640.0, 480.0);

        let input_rect = engine.get_rect(tree.nodes[input_id.0].as_ref().unwrap().layout_node);
        let row_rect = engine.get_rect(tree.nodes[row_id.0].as_ref().unwrap().layout_node);
        let btn_rect = engine.get_rect(tree.nodes[btn_id.0].as_ref().unwrap().layout_node);

        eprintln!("DEBUG: input_rect: {:?}", input_rect);
        eprintln!("DEBUG: row_rect: {:?}", row_rect);
        eprintln!("DEBUG: btn_rect: {:?}", btn_rect);

        let hit_btn = tree.hit_test(&engine, btn_rect.x + 10.0, btn_rect.y + 10.0);
        assert_eq!(hit_btn, Some(btn_id));

        let move_evt = PointerEvent::Move {
            x: btn_rect.x + 10.0,
            y: btn_rect.y + 10.0,
        };
        tree.dispatch_pointer_event(&mut dispatcher, &engine, &move_evt);
        assert_eq!(dispatcher.hover, Some(btn_id));

        let down_evt = PointerEvent::Down {
            x: btn_rect.x + 10.0,
            y: btn_rect.y + 10.0,
            button: MouseButton::Left,
        };
        tree.dispatch_pointer_event(&mut dispatcher, &engine, &down_evt);
        assert_eq!(dispatcher.pressed, Some(btn_id));

        let up_evt = PointerEvent::Up {
            x: btn_rect.x + 10.0,
            y: btn_rect.y + 10.0,
            button: MouseButton::Left,
        };
        tree.dispatch_pointer_event(&mut dispatcher, &engine, &up_evt);
        assert_eq!(dispatcher.pressed, None);
    }

    #[test]
    fn test_tree_tab_focus_navigation() {
        use crate::event::Modifiers;
        let mut engine = LayoutEngine::new();
        let mut tree = WidgetTree::new();
        let mut dispatcher = EventDispatcher::new();

        let root_container = Container::new(FlexStyle::default());
        let root = tree.add_widget(&mut engine, Box::new(root_container), None);

        let btn1 = Button::new("Button 1");
        let btn1_id = tree.add_widget(&mut engine, Box::new(btn1), Some(root));

        let btn2 = Button::new("Button 2");
        let btn2_id = tree.add_widget(&mut engine, Box::new(btn2), Some(root));

        let focusable = tree.focusable_widgets();
        assert_eq!(focusable, vec![btn1_id, btn2_id]);

        // Initial Tab -> Focus btn1
        let tab_evt = KeyEvent::KeyDown {
            key: "Tab".to_string(),
            keysym: 0xff09,
            text: None,
            modifiers: Modifiers::default(),
            is_repeat: false,
        };
        tree.dispatch_key_event(&mut dispatcher, &tab_evt);
        assert_eq!(dispatcher.focus, Some(btn1_id));

        // Second Tab -> Focus btn2
        tree.dispatch_key_event(&mut dispatcher, &tab_evt);
        assert_eq!(dispatcher.focus, Some(btn2_id));

        // Third Tab -> Wrap around to btn1
        tree.dispatch_key_event(&mut dispatcher, &tab_evt);
        assert_eq!(dispatcher.focus, Some(btn1_id));

        // Shift+Tab -> Backwards to btn2
        let shift_tab_evt = KeyEvent::KeyDown {
            key: "Tab".to_string(),
            keysym: 0xff09,
            text: None,
            modifiers: Modifiers {
                shift: true,
                ..Default::default()
            },
            is_repeat: false,
        };
        tree.dispatch_key_event(&mut dispatcher, &shift_tab_evt);
        assert_eq!(dispatcher.focus, Some(btn2_id));
    }

    #[test]
    fn test_tree_scroll_bubbling_and_thumb_drag() {
        use crate::widget::scroll_area::ScrollArea;
        use crate::layout::Dimension;

        let mut engine = LayoutEngine::new();
        let mut tree = WidgetTree::new();
        let mut dispatcher = EventDispatcher::new();

        let scroll_area = ScrollArea::new(FlexStyle {
            width: Dimension::Points(100.0),
            height: Dimension::Points(100.0),
            ..Default::default()
        })
        .with_max_scroll(200.0);
        let scroll_id = tree.add_widget(&mut engine, Box::new(scroll_area), None);
        tree.set_root(scroll_id);

        let card = Container::new(FlexStyle {
            width: Dimension::Points(80.0),
            height: Dimension::Points(40.0),
            ..Default::default()
        });
        let card_id = tree.add_widget(&mut engine, Box::new(card), Some(scroll_id));

        let label = Label::new("Inside Card");
        let label_id = tree.add_widget(&mut engine, Box::new(label), Some(card_id));

        engine.compute(tree.nodes[scroll_id.0].as_ref().unwrap().layout_node, 100.0, 100.0);

        // Hover over the label inside the card
        let label_rect = tree.node_screen_rect(label_id, &engine);
        let scroll_evt = PointerEvent::Scroll {
            x: label_rect.x + 2.0,
            y: label_rect.y + 2.0,
            dx: 0.0,
            dy: 2.0,
        };

        let resp = tree.dispatch_pointer_event(&mut dispatcher, &engine, &scroll_evt);
        assert_eq!(resp, EventResponse::Handled);
        assert!(tree.dirty);

        let scroll_node = tree.nodes[scroll_id.0].as_ref().unwrap();
        let scroll_widget = scroll_node.widget.as_any().downcast_ref::<ScrollArea>().unwrap();
        assert_eq!(scroll_widget.target_scroll_y, 72.0);

        // Drag scrollbar thumb
        let scroll_rect = tree.node_screen_rect(scroll_id, &engine);
        let (_track, thumb) = scroll_widget.track_and_thumb_rects(scroll_rect);

        let down_evt = PointerEvent::Down {
            x: thumb.x + thumb.width / 2.0,
            y: thumb.y + thumb.height / 2.0,
            button: MouseButton::Left,
        };
        let down_resp = tree.dispatch_pointer_event(&mut dispatcher, &engine, &down_evt);
        assert_eq!(down_resp, EventResponse::Handled);
        assert_eq!(dispatcher.pressed, Some(scroll_id));

        let move_evt = PointerEvent::Move {
            x: thumb.x + thumb.width / 2.0,
            y: thumb.y + 30.0,
        };
        let move_resp = tree.dispatch_pointer_event(&mut dispatcher, &engine, &move_evt);
        assert_eq!(move_resp, EventResponse::Handled);

        let up_evt = PointerEvent::Up {
            x: thumb.x + thumb.width / 2.0,
            y: thumb.y + 30.0,
            button: MouseButton::Left,
        };
        let up_resp = tree.dispatch_pointer_event(&mut dispatcher, &engine, &up_evt);
        assert_eq!(up_resp, EventResponse::Handled);
        assert_eq!(dispatcher.pressed, None);
    }

    #[test]
    fn test_widget_tree_cursor_icon_querying() {
        use crate::widget::text_input::TextInput;

        let mut engine = LayoutEngine::new();
        let mut tree = WidgetTree::new();
        let mut dispatcher = EventDispatcher::new();

        let root_container = Container::new(FlexStyle {
            width: Dimension::Points(400.0),
            height: Dimension::Points(300.0),
            ..Default::default()
        });
        let root = tree.add_widget(&mut engine, Box::new(root_container), None);

        let btn = Button::new("Click Me");
        let btn_id = tree.add_widget(&mut engine, Box::new(btn), Some(root));

        let input = TextInput::new("Type here");
        let input_id = tree.add_widget(&mut engine, Box::new(input), Some(root));

        let shaper = TextShaper::default_system_font().expect("Failed font load");
        tree.sync_layout(&mut engine, &shaper);

        let root_layout_node = tree.nodes[root.0].as_ref().unwrap().layout_node;
        engine.compute(root_layout_node, 400.0, 300.0);

        let btn_rect = engine.get_rect(tree.nodes[btn_id.0].as_ref().unwrap().layout_node);
        let input_rect = engine.get_rect(tree.nodes[input_id.0].as_ref().unwrap().layout_node);

        let move_btn = PointerEvent::Move {
            x: btn_rect.x + 5.0,
            y: btn_rect.y + 5.0,
        };
        tree.dispatch_pointer_event(&mut dispatcher, &engine, &move_btn);
        assert_eq!(tree.query_cursor_icon(&engine, &dispatcher), CursorIcon::Pointer);

        let move_input = PointerEvent::Move {
            x: input_rect.x + 5.0,
            y: input_rect.y + 5.0,
        };
        tree.dispatch_pointer_event(&mut dispatcher, &engine, &move_input);
        assert_eq!(tree.query_cursor_icon(&engine, &dispatcher), CursorIcon::Text);

        let move_empty = PointerEvent::Move {
            x: 350.0,
            y: 250.0,
        };
        tree.dispatch_pointer_event(&mut dispatcher, &engine, &move_empty);
        assert_eq!(tree.query_cursor_icon(&engine, &dispatcher), CursorIcon::Default);
    }

    #[test]
    fn test_widget_tree_tooltip_overlay_integration() {
        use crate::widget::tooltip::TooltipPhase;

        let mut engine = LayoutEngine::new();
        let mut tree = WidgetTree::new();
        let mut dispatcher = EventDispatcher::new();

        let root_container = Container::new(FlexStyle {
            width: Dimension::Points(400.0),
            height: Dimension::Points(300.0),
            ..Default::default()
        });
        let root = tree.add_widget(&mut engine, Box::new(root_container), None);

        let btn = Button::new("Click Me").with_tooltip("Increment Counter");
        let btn_id = tree.add_widget(&mut engine, Box::new(btn), Some(root));

        let shaper = TextShaper::default_system_font().expect("Failed font load");
        let rasterizer = GlyphRasterizer::new(shaper.font_db().clone());
        let mut atlas = GlyphAtlas::new(512, 512, 1);
        let mut vector_atlas = crate::render::VectorAtlas::new(256, 256);
        let theme = Theme::dark();

        tree.sync_layout(&mut engine, &shaper);
        let root_layout_node = tree.nodes[root.0].as_ref().unwrap().layout_node;
        engine.compute(root_layout_node, 400.0, 300.0);

        let btn_rect = engine.get_rect(tree.nodes[btn_id.0].as_ref().unwrap().layout_node);

        // Move over button -> Waiting phase
        let move_btn = PointerEvent::Move {
            x: btn_rect.x + 5.0,
            y: btn_rect.y + 5.0,
        };
        tree.dispatch_pointer_event(&mut dispatcher, &engine, &move_btn);
        assert_eq!(tree.tooltip_manager.phase, TooltipPhase::Waiting);

        // Advance 500ms -> FadingIn phase
        tree.tick_animations(500.0);
        assert_eq!(tree.tooltip_manager.phase, TooltipPhase::FadingIn);

        // Advance 150ms -> Visible phase
        tree.tick_animations(150.0);
        assert_eq!(tree.tooltip_manager.phase, TooltipPhase::Visible);
        assert!(tree.tooltip_manager.is_visible());

        // Paint should now include tooltip primitives
        let prims = tree.paint(
            &engine,
            &theme,
            &mut atlas,
            &rasterizer,
            &shaper,
            &mut vector_atlas,
            1.0,
        );
        assert!(prims.len() > 2);

        // Move to empty space -> FadingOut phase
        let move_empty = PointerEvent::Move {
            x: 350.0,
            y: 250.0,
        };
        tree.dispatch_pointer_event(&mut dispatcher, &engine, &move_empty);
        assert_eq!(tree.tooltip_manager.phase, TooltipPhase::FadingOut);

        // Advance fade out to completion
        tree.tick_animations(150.0);
        assert_eq!(tree.tooltip_manager.phase, TooltipPhase::Inactive);
        assert!(!tree.tooltip_manager.is_visible());

        // Move back to button, then click -> Instant dismiss
        tree.dispatch_pointer_event(&mut dispatcher, &engine, &move_btn);
        tree.tick_animations(500.0);
        assert_eq!(tree.tooltip_manager.phase, TooltipPhase::FadingIn);

        let down_evt = PointerEvent::Down {
            x: btn_rect.x + 5.0,
            y: btn_rect.y + 5.0,
            button: MouseButton::Left,
        };
        tree.dispatch_pointer_event(&mut dispatcher, &engine, &down_evt);
        assert_eq!(tree.tooltip_manager.phase, TooltipPhase::Inactive);
    }

    #[test]
    fn test_widget_tree_modal_dialog_lifecycle_and_interception() {
        use std::sync::Mutex;
        use crate::widget::dialog::{Dialog, DialogResponse};

        let mut engine = LayoutEngine::new();
        let mut tree = WidgetTree::new();
        let mut dispatcher = EventDispatcher::new();

        let root_container = Container::new(FlexStyle {
            width: Dimension::Points(600.0),
            height: Dimension::Points(400.0),
            ..Default::default()
        });
        let root = tree.add_widget(&mut engine, Box::new(root_container), None);

        let bg_clicked = Arc::new(AtomicBool::new(false));
        let bg_clicked_clone = bg_clicked.clone();
        let bg_btn = Button::new("Background Action").on_click(move || {
            bg_clicked_clone.store(true, Ordering::SeqCst);
        });
        let bg_btn_id = tree.add_widget(&mut engine, Box::new(bg_btn), Some(root));

        let shaper = TextShaper::default_system_font().expect("Failed font load");
        tree.sync_layout(&mut engine, &shaper);
        let root_layout_node = tree.nodes[root.0].as_ref().unwrap().layout_node;
        engine.compute(root_layout_node, 600.0, 400.0);

        let btn_rect = engine.get_rect(tree.nodes[bg_btn_id.0].as_ref().unwrap().layout_node);

        // Open confirm modal
        let modal_res = Arc::new(Mutex::new(None));
        let modal_res_clone = modal_res.clone();
        let dialog = Dialog::confirm("Delete Record", "Permanent action").on_response(move |resp| {
            *modal_res_clone.lock().unwrap() = Some(resp);
        });
        tree.open_modal(dialog);
        assert!(tree.is_modal_active());

        // Advance entrance animation
        tree.tick_animations(180.0);

        // Click outside on background button -> Must be intercepted by scrim!
        let down_bg = PointerEvent::Down {
            x: btn_rect.x + 2.0,
            y: btn_rect.y + 2.0,
            button: MouseButton::Left,
        };
        tree.dispatch_pointer_event(&mut dispatcher, &engine, &down_bg);
        assert!(!bg_clicked.load(Ordering::SeqCst));

        // Background click dismissed the modal (dismiss_on_backdrop: true)
        tree.tick_animations(150.0);
        assert!(!tree.is_modal_active());
        assert_eq!(*modal_res.lock().unwrap(), Some(DialogResponse::Cancel));

        // Re-open modal and click Confirm button
        let modal_res2 = Arc::new(Mutex::new(None));
        let modal_res2_clone = modal_res2.clone();
        let dialog2 = Dialog::confirm("Delete Record", "Permanent action").on_response(move |resp| {
            *modal_res2_clone.lock().unwrap() = Some(resp);
        });
        tree.open_modal(dialog2);
        tree.tick_animations(180.0);

        // Confirm button is the second button
        let confirm_rect = tree.active_modal.as_ref().unwrap().button_rects.borrow()[1];
        let down_confirm = PointerEvent::Down {
            x: confirm_rect.x + 5.0,
            y: confirm_rect.y + 5.0,
            button: MouseButton::Left,
        };
        tree.dispatch_pointer_event(&mut dispatcher, &engine, &down_confirm);
        tree.tick_animations(150.0);
        assert!(!tree.is_modal_active());
        assert_eq!(*modal_res2.lock().unwrap(), Some(DialogResponse::Confirm));
    }

    #[test]
    fn test_hit_test_window_edge_detection() {
        let (win_w, win_h, margin) = (800.0, 600.0, 6.0);
        use crate::platform::ResizeEdge;

        assert_eq!(hit_test_window_edge(2.0, 2.0, win_w, win_h, margin), ResizeEdge::TopLeft);
        assert_eq!(hit_test_window_edge(798.0, 2.0, win_w, win_h, margin), ResizeEdge::TopRight);
        assert_eq!(hit_test_window_edge(2.0, 598.0, win_w, win_h, margin), ResizeEdge::BottomLeft);
        assert_eq!(hit_test_window_edge(798.0, 598.0, win_w, win_h, margin), ResizeEdge::BottomRight);

        assert_eq!(hit_test_window_edge(400.0, 2.0, win_w, win_h, margin), ResizeEdge::Top);
        assert_eq!(hit_test_window_edge(400.0, 598.0, win_w, win_h, margin), ResizeEdge::Bottom);
        assert_eq!(hit_test_window_edge(2.0, 300.0, win_w, win_h, margin), ResizeEdge::Left);
        assert_eq!(hit_test_window_edge(798.0, 300.0, win_w, win_h, margin), ResizeEdge::Right);

        assert_eq!(hit_test_window_edge(400.0, 300.0, win_w, win_h, margin), ResizeEdge::None);
    }

    #[test]
    fn test_csd_window_edge_resizing_and_actions() {
        let mut tree = WidgetTree::new();
        let mut engine = LayoutEngine::new();
        let mut dispatcher = EventDispatcher::new();

        let root_container = Container::new(FlexStyle {
            width: Dimension::Points(800.0),
            height: Dimension::Points(600.0),
            ..Default::default()
        });
        tree.add_widget(&mut engine, Box::new(root_container), None);
        engine.compute(tree.nodes[0].as_ref().unwrap().layout_node, 800.0, 600.0);

        tree.set_csd_enabled(true);

        // Move to right edge: cursor should change to EwResize
        dispatcher.mouse_pos = (798.0, 300.0);
        assert_eq!(tree.query_cursor_icon(&engine, &dispatcher), CursorIcon::EwResize);

        // Click right edge: should queue InteractiveResize(Right)
        let down_evt = PointerEvent::Down {
            x: 798.0,
            y: 300.0,
            button: MouseButton::Left,
        };
        let resp = tree.dispatch_pointer_event(&mut dispatcher, &engine, &down_evt);
        assert_eq!(resp, EventResponse::Handled);

        let actions = tree.take_window_actions();
        assert_eq!(actions, vec![WindowAction::InteractiveResize(crate::platform::ResizeEdge::Right)]);
    }

    #[test]
    fn test_titlebar_integration_in_widget_tree() {
        use crate::widget::Titlebar;
        let mut tree = WidgetTree::new();
        let mut engine = LayoutEngine::new();
        let mut dispatcher = EventDispatcher::new();

        let root_container = Container::new(FlexStyle {
            direction: crate::layout::FlexDirection::Column,
            width: Dimension::Points(800.0),
            height: Dimension::Points(600.0),
            ..Default::default()
        });
        let root = tree.add_widget(&mut engine, Box::new(root_container), None);

        let titlebar = Titlebar::new("Test App");
        tree.add_widget(&mut engine, Box::new(titlebar), Some(root));

        engine.compute(tree.nodes[0].as_ref().unwrap().layout_node, 800.0, 600.0);

        // Click Close button at top-right
        let down_close = PointerEvent::Down {
            x: 780.0,
            y: 19.0,
            button: MouseButton::Left,
        };
        tree.dispatch_pointer_event(&mut dispatcher, &engine, &down_close);

        let up_close = PointerEvent::Up {
            x: 780.0,
            y: 19.0,
            button: MouseButton::Left,
        };
        tree.dispatch_pointer_event(&mut dispatcher, &engine, &up_close);

        let actions = tree.take_window_actions();
        assert_eq!(actions, vec![WindowAction::Close]);
    }

    #[test]
    fn test_widget_tree_toast_overlay_integration() {
        let mut tree = WidgetTree::new();
        let mut engine = LayoutEngine::new();
        let mut dispatcher = EventDispatcher::new();

        let root_container = Container::new(FlexStyle {
            width: Dimension::Points(800.0),
            height: Dimension::Points(600.0),
            ..Default::default()
        });
        tree.add_widget(&mut engine, Box::new(root_container), None);
        engine.compute(tree.nodes[0].as_ref().unwrap().layout_node, 800.0, 600.0);

        let handle = tree.toast_handle();
        handle.info("Build succeeded");

        assert!(tree.toast_manager.is_empty());
        tree.check_pending_toasts();
        assert_eq!(tree.toast_manager.toasts.len(), 1);

        // Advance through enter animation
        tree.tick_animations(250.0);
        assert_eq!(tree.toast_manager.toasts[0].phase, crate::widget::ToastPhase::Visible);

        // Click outside toast: ignored by toast manager
        let click_outside = PointerEvent::Down {
            x: 50.0,
            y: 50.0,
            button: MouseButton::Left,
        };
        let resp = tree.dispatch_pointer_event(&mut dispatcher, &engine, &click_outside);
        assert_eq!(resp, EventResponse::Ignored);

        // Click close button on toast card at top-right
        let rects = tree.toast_manager.toast_rects(800.0, 600.0);
        let close_rect = rects[0].2.unwrap();
        let click_close = PointerEvent::Down {
            x: close_rect.x + 2.0,
            y: close_rect.y + 2.0,
            button: MouseButton::Left,
        };
        let resp = tree.dispatch_pointer_event(&mut dispatcher, &engine, &click_close);
        assert_eq!(resp, EventResponse::Handled);
        assert_eq!(tree.toast_manager.toasts[0].phase, crate::widget::ToastPhase::Exiting);

        // Advance through exit animation: toast removed
        tree.tick_animations(200.0);
        assert!(tree.toast_manager.is_empty());
    }

    #[test]
    fn test_widget_tree_visibility_and_active_child() {
        use crate::layout::FlexDirection;
        let mut tree = WidgetTree::new();
        let mut engine = LayoutEngine::new();
        let font_db = crate::text::font_db::FontDatabase::new().unwrap();
        let shaper = TextShaper::new(font_db);

        let root_container = Container::new(FlexStyle {
            direction: FlexDirection::Column,
            width: Dimension::Points(800.0),
            height: Dimension::Points(600.0),
            ..Default::default()
        });
        let root = tree.add_widget(&mut engine, Box::new(root_container), None);

        let btn1 = Button::new("Button 1");
        let btn1_id = tree.add_widget(&mut engine, Box::new(btn1), Some(root));

        let btn2 = Button::new("Button 2");
        let btn2_id = tree.add_widget(&mut engine, Box::new(btn2), Some(root));

        tree.sync_layout(&mut engine, &shaper);
        assert!(tree.is_visible(btn1_id));
        assert!(tree.is_visible(btn2_id));
        let focusable = tree.focusable_widgets();
        assert_eq!(focusable.len(), 2);

        // Hide btn2
        tree.set_visible(btn2_id, false);
        assert!(!tree.is_visible(btn2_id));

        tree.sync_layout(&mut engine, &shaper);
        let focusable_after = tree.focusable_widgets();
        assert_eq!(focusable_after, vec![btn1_id]);

        let r2 = engine.get_rect(tree.nodes[btn2_id.0].as_ref().unwrap().layout_node);
        assert_eq!(r2.width, 0.0);
        assert_eq!(r2.height, 0.0);
    }

    #[test]
    fn test_tabs_sync_layout_page_switching() {
        use crate::layout::FlexDirection;
        use crate::widget::{TabBarOrientation, Tabs};

        let mut tree = WidgetTree::new();
        let mut engine = LayoutEngine::new();
        let font_db = crate::text::font_db::FontDatabase::new().unwrap();
        let shaper = TextShaper::new(font_db);

        let root_container = Container::new(FlexStyle {
            direction: FlexDirection::Column,
            width: Dimension::Points(800.0),
            height: Dimension::Points(600.0),
            ..Default::default()
        });
        let root = tree.add_widget(&mut engine, Box::new(root_container), None);

        let tabs = Tabs::new(TabBarOrientation::Horizontal);
        let tabs_id = tree.add_widget(&mut engine, Box::new(tabs), Some(root));

        let p1 = Container::new(FlexStyle {
            width: Dimension::Percent(100.0),
            height: Dimension::Points(200.0),
            ..Default::default()
        });
        let p1_id = tree.add_widget(&mut engine, Box::new(p1), Some(tabs_id));

        let p2 = Container::new(FlexStyle {
            width: Dimension::Percent(100.0),
            height: Dimension::Points(300.0),
            ..Default::default()
        });
        let p2_id = tree.add_widget(&mut engine, Box::new(p2), Some(tabs_id));

        tree.get_widget_mut::<Tabs>(tabs_id)
            .unwrap()
            .add_tab("Tab 1", p1_id)
            .add_tab("Tab 2", p2_id);

        tree.sync_layout(&mut engine, &shaper);
        let tabs_node = tree.nodes[tabs_id.0].as_ref().unwrap().layout_node;
        engine.compute(tabs_node, 800.0, 600.0);

        let r1 = engine.get_rect(tree.nodes[p1_id.0].as_ref().unwrap().layout_node);
        let r2 = engine.get_rect(tree.nodes[p2_id.0].as_ref().unwrap().layout_node);
        assert!(r1.height > 0.0);
        assert_eq!(r2.height, 0.0);

        // Switch to Tab 2
        tree.get_widget_mut::<Tabs>(tabs_id).unwrap().select_tab(1);
        tree.sync_layout(&mut engine, &shaper);
        engine.compute(tabs_node, 800.0, 600.0);

        let r1_after = engine.get_rect(tree.nodes[p1_id.0].as_ref().unwrap().layout_node);
        let r2_after = engine.get_rect(tree.nodes[p2_id.0].as_ref().unwrap().layout_node);
        assert_eq!(r1_after.height, 0.0);
        assert!(r2_after.height > 0.0);
    }

    #[test]
    fn test_showcase_split_view_inside_tabs_layout_positions() {
        use crate::layout::{AlignItems, Dimension, EdgeInsets, FlexDirection, FlexStyle, JustifyContent};
        use crate::widget::{Container, SplitOrientation, SplitView, TabBarOrientation, Tabs, Titlebar};

        let mut tree = WidgetTree::new();
        let mut engine = LayoutEngine::new();
        let font_db = crate::text::font_db::FontDatabase::new().unwrap();
        let shaper = TextShaper::new(font_db);

        let root_container = Container::new(FlexStyle {
            direction: FlexDirection::Column,
            justify_content: JustifyContent::FlexStart,
            align_items: AlignItems::Stretch,
            width: Dimension::Percent(100.0),
            height: Dimension::Percent(100.0),
            ..Default::default()
        });
        let root = tree.add_widget(&mut engine, Box::new(root_container), None);

        let titlebar = Titlebar::new("North Toolkit Showcase");
        let titlebar_id = tree.add_widget(&mut engine, Box::new(titlebar), Some(root));

        let content_container = Container::new(FlexStyle {
            direction: FlexDirection::Column,
            justify_content: JustifyContent::FlexStart,
            align_items: AlignItems::Stretch,
            padding: EdgeInsets::symmetric(16.0, 24.0),
            gap: 12.0,
            flex_grow: 1.0,
            width: Dimension::Percent(100.0),
            ..Default::default()
        });
        let content = tree.add_widget(&mut engine, Box::new(content_container), Some(root));

        let mut tabs = Tabs::new(TabBarOrientation::Horizontal);
        tabs.style.height = Dimension::Points(340.0);
        tabs.style.flex_grow = 1.0;
        let tabs_id = tree.add_widget(&mut engine, Box::new(tabs), Some(content));

        // Page 1: SplitView
        let page1 = Container::new(FlexStyle {
            direction: FlexDirection::Column,
            width: Dimension::Percent(100.0),
            height: Dimension::Percent(100.0),
            flex_grow: 1.0,
            ..Default::default()
        });
        let page1_id = tree.add_widget(&mut engine, Box::new(page1), Some(tabs_id));

        let split_view = SplitView::new(SplitOrientation::Horizontal)
            .with_split_offset(400.0)
            .with_min_sizes(200.0, 200.0)
            .with_divider_thickness(6.0);
        let split_id = tree.add_widget(&mut engine, Box::new(split_view), Some(page1_id));

        let left_col = Container::new(FlexStyle {
            direction: FlexDirection::Column,
            gap: 8.0,
            height: Dimension::Percent(100.0),
            ..Default::default()
        });
        let left_col_id = tree.add_widget(&mut engine, Box::new(left_col), Some(split_id));

        let right_col = Container::new(FlexStyle {
            direction: FlexDirection::Column,
            gap: 8.0,
            height: Dimension::Percent(100.0),
            flex_grow: 1.0,
            ..Default::default()
        });
        let right_col_id = tree.add_widget(&mut engine, Box::new(right_col), Some(split_id));

        // Page 2: Controls
        let page2 = Container::new(FlexStyle {
            direction: FlexDirection::Row,
            width: Dimension::Percent(100.0),
            height: Dimension::Percent(100.0),
            flex_grow: 1.0,
            ..Default::default()
        });
        let page2_id = tree.add_widget(&mut engine, Box::new(page2), Some(tabs_id));

        tree.get_widget_mut::<Tabs>(tabs_id)
            .unwrap()
            .add_tab("Panes", page1_id)
            .add_tab("Controls", page2_id);

        tree.sync_layout(&mut engine, &shaper);
        let root_layout = tree.nodes[root.0].as_ref().unwrap().layout_node;
        engine.compute(root_layout, 1080.0, 760.0);

        let title_r = engine.get_rect(tree.nodes[titlebar_id.0].as_ref().unwrap().layout_node);
        let left_r = engine.get_rect(tree.nodes[left_col_id.0].as_ref().unwrap().layout_node);
        let right_r = engine.get_rect(tree.nodes[right_col_id.0].as_ref().unwrap().layout_node);

        assert_eq!(title_r.y, 0.0);
        assert_eq!(title_r.height, 38.0);

        // Neither pane may be at (0, 0)!
        assert!(left_r.y > 36.0, "left_r.y was overlapping titlebar: {:?}", left_r);
        assert!(right_r.y > 36.0, "right_r.y was overlapping titlebar: {:?}", right_r);
        assert_eq!(left_r.width, 400.0);
        assert!(right_r.width > 500.0);
        assert!(left_r.height > 200.0);
        assert!(right_r.height > 200.0);
    }
}
