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
    fn paint(
        &self,
        rect: LayoutRect,
        theme: &Theme,
        atlas: &mut GlyphAtlas,
        rasterizer: &GlyphRasterizer,
        shaper: &TextShaper,
        scale: f32,
    ) -> Vec<RenderPrimitive>;
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
    fn as_any(&self) -> &dyn Any;
    fn as_any_mut(&mut self) -> &mut dyn Any;
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
        self.hit_test_recursive(root, engine, px, py)
    }

    fn hit_test_recursive(
        &self,
        current: WidgetId,
        engine: &LayoutEngine,
        px: f32,
        py: f32,
    ) -> Option<WidgetId> {
        let node = self.nodes.get(current.0)?.as_ref()?;
        let rect = engine.get_rect(node.layout_node);

        if !rect.contains(px, py) {
            return None;
        }

        for &child in node.children.iter().rev() {
            if let Some(hit) = self.hit_test_recursive(child, engine, px, py) {
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
                let (prev_hover, new_hover) = dispatcher.update_hover(hit);
                dispatcher.mouse_pos = (px, py);
                let mut resp = EventResponse::Ignored;

                if prev_hover != new_hover {
                    if let Some(prev) = prev_hover {
                        let rect = self.node_rect(prev, engine);
                        if let Some(Some(node)) = self.nodes.get_mut(prev.0) {
                            if node.widget.on_pointer_event(event, rect) == EventResponse::Handled {
                                resp = EventResponse::Handled;
                            }
                        }
                    }
                    self.dirty = true;
                }

                if let Some(target) = new_hover {
                    let rect = self.node_rect(target, engine);
                    if let Some(Some(node)) = self.nodes.get_mut(target.0) {
                        if node.widget.on_pointer_event(event, rect) == EventResponse::Handled {
                            resp = EventResponse::Handled;
                        }
                    }
                }
                return resp;
            }
            PointerEvent::Down { .. } => {
                dispatcher.pressed = hit;
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
                if let Some(target) = hit {
                    let rect = self.node_rect(target, engine);
                    if let Some(Some(node)) = self.nodes.get_mut(target.0) {
                        return node.widget.on_pointer_event(event, rect);
                    }
                }
            }
            PointerEvent::Up { .. } => {
                let pressed = dispatcher.pressed.take();
                let mut resp = EventResponse::Ignored;
                if let Some(target) = pressed.or(hit) {
                    let rect = self.node_rect(target, engine);
                    if let Some(Some(node)) = self.nodes.get_mut(target.0) {
                        resp = node.widget.on_pointer_event(event, rect);
                    }
                }
                return resp;
            }
            PointerEvent::Scroll { .. } => {
                if let Some(target) = hit {
                    let rect = self.node_rect(target, engine);
                    if let Some(Some(node)) = self.nodes.get_mut(target.0) {
                        return node.widget.on_pointer_event(event, rect);
                    }
                }
            }
        }

        EventResponse::Ignored
    }

    pub fn dispatch_key_event(
        &mut self,
        dispatcher: &EventDispatcher,
        event: &KeyEvent,
    ) -> EventResponse {
        if let Some(focus_id) = dispatcher.focus {
            if let Some(Some(node)) = self.nodes.get_mut(focus_id.0) {
                return node.widget.on_key_event(event);
            }
        }
        EventResponse::Ignored
    }

    fn node_rect(&self, id: WidgetId, engine: &LayoutEngine) -> LayoutRect {
        self.nodes
            .get(id.0)
            .and_then(|n| n.as_ref())
            .map(|n| engine.get_rect(n.layout_node))
            .unwrap_or(LayoutRect::ZERO)
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
        primitives: &mut Vec<RenderPrimitive>,
    ) {
        let node = match self.nodes.get(current.0).and_then(|n| n.as_ref()) {
            Some(n) => n,
            None => return,
        };

        let rect = engine.get_rect(node.layout_node);
        let mut widget_prims = node.widget.paint(rect, theme, atlas, rasterizer, shaper, scale);
        primitives.append(&mut widget_prims);

        for &child in &node.children {
            self.paint_recursive(
                child,
                engine,
                theme,
                atlas,
                rasterizer,
                shaper,
                scale,
                primitives,
            );
        }
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
}
