use std::any::Any;

use crate::event::{EventDispatcher, EventResponse, KeyEvent, PointerEvent, WidgetId};
use crate::layout::{FlexStyle, LayoutEngine, LayoutNodeId, LayoutRect, MeasureFunc};
use crate::render::{GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

pub trait Widget: Any {
    fn layout_style(&self) -> FlexStyle;
    fn measure(&self, _shaper: &TextShaper) -> Option<MeasureFunc> {
        None
    }
    fn paint_before(
        &self,
        rect: LayoutRect,
        theme: &Theme,
        atlas: &mut GlyphAtlas,
        rasterizer: &GlyphRasterizer,
        shaper: &TextShaper,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        self.paint(rect, theme, atlas, rasterizer, shaper, scale)
    }
    fn paint_after(
        &self,
        _rect: LayoutRect,
        _theme: &Theme,
        _atlas: &mut GlyphAtlas,
        _rasterizer: &GlyphRasterizer,
        _shaper: &TextShaper,
        _scale: f32,
    ) -> Vec<RenderPrimitive> {
        Vec::new()
    }
    fn paint(
        &self,
        _rect: LayoutRect,
        _theme: &Theme,
        _atlas: &mut GlyphAtlas,
        _rasterizer: &GlyphRasterizer,
        _shaper: &TextShaper,
        _scale: f32,
    ) -> Vec<RenderPrimitive> {
        Vec::new()
    }
    fn scroll_offset(&self) -> (f32, f32) {
        (0.0, 0.0)
    }
    fn is_focusable(&self) -> bool {
        false
    }
    fn on_pointer_event(&mut self, _event: &PointerEvent, _rect: LayoutRect) -> EventResponse {
        EventResponse::Ignored
    }
    fn on_key_event(&mut self, _event: &KeyEvent) -> EventResponse {
        EventResponse::Ignored
    }
    fn on_focus_change(&mut self, _focused: bool) {}
    fn tick_animation(&mut self, _delta_ms: f32) -> bool {
        false
    }
    fn take_popup_request(&mut self, _rect: LayoutRect) -> Option<super::popup::PopupMenu> {
        None
    }
    fn on_popup_closed(&mut self, _selected_index: Option<usize>) {}
    fn as_any(&self) -> &dyn Any;
    fn as_any_mut(&mut self) -> &mut dyn Any;
}

pub struct PopupRequest {
    pub anchor_rect: LayoutRect,
    pub menu: super::popup::PopupMenu,
    pub source_widget: WidgetId,
}

pub struct WidgetNode {
    pub widget: Box<dyn Widget>,
    pub children: Vec<WidgetId>,
    pub parent: Option<WidgetId>,
    pub layout_node: LayoutNodeId,
    pub dirty: bool,
}

pub struct WidgetTree {
    pub nodes: Vec<Option<WidgetNode>>,
    root: Option<WidgetId>,
    pub dirty: bool,
}

impl WidgetTree {
    pub fn new() -> Self {
        Self {
            nodes: Vec::new(),
            root: None,
            dirty: true,
        }
    }

    pub fn root(&self) -> Option<WidgetId> {
        self.root
    }

    pub fn set_root(&mut self, id: WidgetId) {
        self.root = Some(id);
        self.dirty = true;
    }

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

    pub fn get_widget<T: Widget>(&self, id: WidgetId) -> Option<&T> {
        self.nodes
            .get(id.0)
            .and_then(|n| n.as_ref())
            .and_then(|n| n.widget.as_any().downcast_ref::<T>())
    }

    pub fn get_widget_mut<T: Widget>(&mut self, id: WidgetId) -> Option<&mut T> {
        self.nodes
            .get_mut(id.0)
            .and_then(|n| n.as_mut())
            .and_then(|n| n.widget.as_any_mut().downcast_mut::<T>())
    }

    pub fn mark_dirty(&mut self, id: WidgetId) {
        if let Some(Some(node)) = self.nodes.get_mut(id.0) {
            node.dirty = true;
        }
        self.dirty = true;
    }

    pub fn is_dirty(&self) -> bool {
        self.dirty
    }

    pub fn sync_layout(&mut self, engine: &mut LayoutEngine, shaper: &TextShaper) {
        for node in self.nodes.iter_mut().flatten() {
            if node.dirty {
                engine.set_style(node.layout_node, node.widget.layout_style());
                if let Some(measure_fn) = node.widget.measure(shaper) {
                    engine.set_measure(node.layout_node, measure_fn);
                }
                node.dirty = false;
            }
        }
        self.dirty = false;
    }

    pub fn tick_animations(&mut self, delta_ms: f32) -> bool {
        let mut needs_repaint = false;
        for node in self.nodes.iter_mut().flatten() {
            if node.widget.tick_animation(delta_ms) {
                needs_repaint = true;
            }
        }
        if needs_repaint {
            self.dirty = true;
        }
        needs_repaint
    }

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

        for &child in node.children.iter().rev() {
            if let Some(hit) = self.hit_test_recursive(child, engine, px, py, child_offset) {
                return Some(hit);
            }
        }

        Some(current)
    }

    pub fn dispatch_pointer_event(
        &mut self,
        dispatcher: &mut EventDispatcher,
        engine: &LayoutEngine,
        event: &PointerEvent,
    ) -> EventResponse {
        let (px, py) = event.position();
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
                return resp;
            }
            PointerEvent::Down { .. } => {
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
                    let (handled, parent) = if let Some(Some(node)) = self.nodes.get_mut(target.0) {
                        let r = node.widget.on_pointer_event(event, rect);
                        (r == EventResponse::Handled, node.parent)
                    } else {
                        (false, None)
                    };

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

    pub fn focusable_widgets(&self) -> Vec<WidgetId> {
        let mut list = Vec::new();
        if let Some(root_id) = self.root {
            self.collect_focusable_recursive(root_id, &mut list);
        }
        list
    }

    fn collect_focusable_recursive(&self, current: WidgetId, list: &mut Vec<WidgetId>) {
        if let Some(Some(node)) = self.nodes.get(current.0) {
            if node.widget.is_focusable() {
                list.push(current);
            }
            for &child in &node.children {
                self.collect_focusable_recursive(child, list);
            }
        }
    }

    pub fn dispatch_key_event(
        &mut self,
        dispatcher: &mut EventDispatcher,
        event: &KeyEvent,
    ) -> EventResponse {
        if let KeyEvent::KeyDown { key, modifiers, .. } = event {
            if key == "Tab" {
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
                return node.widget.on_key_event(event);
            }
        }
        EventResponse::Ignored
    }

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

    pub fn notify_popup_closed(&mut self, source: WidgetId, selected_index: Option<usize>) {
        if let Some(Some(node)) = self.nodes.get_mut(source.0) {
            node.widget.on_popup_closed(selected_index);
            self.dirty = true;
        }
    }

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

    pub fn paint(
        &self,
        engine: &LayoutEngine,
        theme: &Theme,
        atlas: &mut GlyphAtlas,
        rasterizer: &GlyphRasterizer,
        shaper: &TextShaper,
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
                scale,
                (0.0, 0.0),
                &mut primitives,
            );
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
        scale: f32,
        offset: (f32, f32),
        primitives: &mut Vec<RenderPrimitive>,
    ) {
        let node = match self.nodes.get(current.0).and_then(|n| n.as_ref()) {
            Some(n) => n,
            None => return,
        };

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
            scale,
        );
        primitives.append(&mut before_prims);

        let scroll = node.widget.scroll_offset();
        let child_offset = (offset.0 - scroll.0, offset.1 - scroll.1);

        for &child in &node.children {
            self.paint_recursive(
                child,
                engine,
                theme,
                atlas,
                rasterizer,
                shaper,
                scale,
                child_offset,
                primitives,
            );
        }

        let mut after_prims = node.widget.paint_after(
            screen_rect,
            theme,
            atlas,
            rasterizer,
            shaper,
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

        let prims = tree.paint(&engine, &theme, &mut atlas, &rasterizer, &shaper, 1.0);
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
            dy: -2.0,
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
}
