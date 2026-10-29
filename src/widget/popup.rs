use std::any::Any;

use super::state::Property;
use super::tree::Widget;
use crate::event::{EventResponse, KeyEvent, MouseButton, PointerEvent};
use crate::layout::{AlignItems, Dimension, EdgeInsets, FlexDirection, FlexStyle, LayoutRect, MeasureFunc};
use crate::render::{Border, CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{emit_text_glyphs, GlyphRasterizer, TextShaper};
use crate::theme::Theme;

pub enum MenuItem {
    Action {
        label: String,
        shortcut: Option<String>,
        on_select: Box<dyn FnMut()>,
    },
    Separator,
}

impl MenuItem {
    pub fn action<S: Into<String>, F: FnMut() + 'static>(label: S, on_select: F) -> Self {
        Self::Action {
            label: label.into(),
            shortcut: None,
            on_select: Box::new(on_select),
        }
    }

    pub fn action_with_shortcut<S: Into<String>, K: Into<String>, F: FnMut() + 'static>(
        label: S,
        shortcut: K,
        on_select: F,
    ) -> Self {
        Self::Action {
            label: label.into(),
            shortcut: Some(shortcut.into()),
            on_select: Box::new(on_select),
        }
    }

    pub fn separator() -> Self {
        Self::Separator
    }

    pub fn is_separator(&self) -> bool {
        matches!(self, Self::Separator)
    }

    pub fn label(&self) -> Option<&str> {
        match self {
            Self::Action { label, .. } => Some(label),
            Self::Separator => None,
        }
    }
}

pub struct PopupMenu {
    pub items: Vec<MenuItem>,
    pub font_size: f32,
    pub style: FlexStyle,
    pub hovered_index: Option<usize>,
    pub selected_index: Option<usize>,
    pub is_dismissed: bool,
}

impl PopupMenu {
    pub const ITEM_HEIGHT: f32 = 30.0;
    pub const SEPARATOR_HEIGHT: f32 = 9.0;
    pub const PADDING_V: f32 = 6.0;
    pub const PADDING_H: f32 = 6.0;

    pub fn new(items: Vec<MenuItem>) -> Self {
        Self {
            items,
            font_size: 13.0,
            style: FlexStyle {
                direction: FlexDirection::Column,
                width: Dimension::Auto,
                height: Dimension::Auto,
                ..Default::default()
            },
            hovered_index: None,
            selected_index: None,
            is_dismissed: false,
        }
    }

    pub fn with_font_size(mut self, size: f32) -> Self {
        self.font_size = size;
        self
    }

    pub fn total_height(&self) -> f32 {
        let items_h: f32 = self
            .items
            .iter()
            .map(|item| match item {
                MenuItem::Action { .. } => Self::ITEM_HEIGHT,
                MenuItem::Separator => Self::SEPARATOR_HEIGHT,
            })
            .sum();
        items_h + Self::PADDING_V * 2.0
    }

    pub fn item_at_y(&self, y: f32, rect: LayoutRect) -> Option<usize> {
        let mut curr_y = rect.y + Self::PADDING_V;
        for (i, item) in self.items.iter().enumerate() {
            let h = match item {
                MenuItem::Action { .. } => Self::ITEM_HEIGHT,
                MenuItem::Separator => Self::SEPARATOR_HEIGHT,
            };
            if y >= curr_y && y < curr_y + h {
                return Some(i);
            }
            curr_y += h;
        }
        None
    }

    pub fn item_rect(&self, index: usize, rect: LayoutRect) -> LayoutRect {
        let mut curr_y = rect.y + Self::PADDING_V;
        for (i, item) in self.items.iter().enumerate() {
            let h = match item {
                MenuItem::Action { .. } => Self::ITEM_HEIGHT,
                MenuItem::Separator => Self::SEPARATOR_HEIGHT,
            };
            if i == index {
                return LayoutRect::new(
                    rect.x + Self::PADDING_H,
                    curr_y,
                    rect.width - Self::PADDING_H * 2.0,
                    h,
                );
            }
            curr_y += h;
        }
        LayoutRect::ZERO
    }

    pub fn select_next(&mut self) {
        let count = self.items.len();
        if count == 0 {
            return;
        }
        let start = self.hovered_index.map(|i| (i + 1) % count).unwrap_or(0);
        for step in 0..count {
            let idx = (start + step) % count;
            if !self.items[idx].is_separator() {
                self.hovered_index = Some(idx);
                break;
            }
        }
    }

    pub fn select_prev(&mut self) {
        let count = self.items.len();
        if count == 0 {
            return;
        }
        let start = self
            .hovered_index
            .map(|i| if i == 0 { count - 1 } else { i - 1 })
            .unwrap_or(count - 1);
        for step in 0..count {
            let idx = (start + count - step) % count;
            if !self.items[idx].is_separator() {
                self.hovered_index = Some(idx);
                break;
            }
        }
    }

    pub fn trigger_hovered(&mut self) -> bool {
        if let Some(idx) = self.hovered_index {
            if let Some(MenuItem::Action { on_select, .. }) = self.items.get_mut(idx) {
                on_select();
                self.selected_index = Some(idx);
                self.is_dismissed = true;
                return true;
            }
        }
        false
    }
}

impl Widget for PopupMenu {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn measure(&self, shaper: &TextShaper) -> Option<MeasureFunc> {
        let mut max_w = 140.0f32;
        for item in &self.items {
            if let MenuItem::Action { label, shortcut, .. } = item {
                let shaped_label = shaper.shape(label, self.font_size);
                let shortcut_w = shortcut
                    .as_ref()
                    .map(|s| shaper.shape(s, self.font_size * 0.9).width + 16.0)
                    .unwrap_or(0.0);
                let w = shaped_label.width + shortcut_w + Self::PADDING_H * 2.0 + 32.0;
                if w > max_w {
                    max_w = w;
                }
            }
        }
        let total_h = self.total_height();
        Some(Box::new(move |_avail_w, _avail_h| (max_w, total_h)))
    }

    fn on_pointer_event(&mut self, event: &PointerEvent, rect: LayoutRect) -> EventResponse {
        match *event {
            PointerEvent::Move { x, y } => {
                if rect.contains(x, y) {
                    if let Some(idx) = self.item_at_y(y, rect) {
                        if !self.items[idx].is_separator() {
                            self.hovered_index = Some(idx);
                            return EventResponse::Handled;
                        }
                    }
                }
                self.hovered_index = None;
                EventResponse::Handled
            }
            PointerEvent::Down { x, y, button } => {
                if button == MouseButton::Left && rect.contains(x, y) {
                    if let Some(idx) = self.item_at_y(y, rect) {
                        if let Some(MenuItem::Action { on_select, .. }) = self.items.get_mut(idx) {
                            on_select();
                            self.selected_index = Some(idx);
                            self.is_dismissed = true;
                            return EventResponse::Handled;
                        }
                    }
                }
                EventResponse::Ignored
            }
            _ => EventResponse::Ignored,
        }
    }

    fn on_key_event(&mut self, event: &KeyEvent) -> EventResponse {
        if let KeyEvent::KeyDown { key, keysym, .. } = event {
            if key == "Down" || *keysym == 0xff54 {
                self.select_next();
                return EventResponse::Handled;
            } else if key == "Up" || *keysym == 0xff52 {
                self.select_prev();
                return EventResponse::Handled;
            } else if key == "Return" || key == "Space" || *keysym == 0xff0d || *keysym == 0x20 {
                if self.trigger_hovered() {
                    return EventResponse::Handled;
                }
            } else if key == "Escape" || *keysym == 0xff1b {
                self.is_dismissed = true;
                return EventResponse::Handled;
            }
        }
        EventResponse::Ignored
    }

    fn paint(
        &self,
        rect: LayoutRect,
        theme: &Theme,
        atlas: &mut GlyphAtlas,
        rasterizer: &GlyphRasterizer,
        shaper: &TextShaper,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let mut prims = Vec::new();

        // 1. Popup background with subtle elevation shadow/border
        prims.push(RenderPrimitive::bordered_rect(
            [
                rect.x * scale,
                rect.y * scale,
                rect.width * scale,
                rect.height * scale,
            ],
            theme.colors.surface,
            CornerRadii::all(theme.border_radius_md * scale),
            Border::new(theme.colors.border, 1.0 * scale),
        ));

        // 2. Render items
        let mut curr_y = rect.y + Self::PADDING_V;
        for (i, item) in self.items.iter().enumerate() {
            match item {
                MenuItem::Separator => {
                    let sep_y = curr_y + Self::SEPARATOR_HEIGHT * 0.5;
                    prims.push(RenderPrimitive::solid_rect(
                        [
                            (rect.x + Self::PADDING_H + 4.0) * scale,
                            sep_y * scale,
                            (rect.width - (Self::PADDING_H + 4.0) * 2.0) * scale,
                            1.0 * scale,
                        ],
                        theme.colors.border,
                        CornerRadii::all(0.0),
                    ));
                    curr_y += Self::SEPARATOR_HEIGHT;
                }
                MenuItem::Action {
                    label, shortcut, ..
                } => {
                    let item_rect = LayoutRect::new(
                        rect.x + Self::PADDING_H,
                        curr_y,
                        rect.width - Self::PADDING_H * 2.0,
                        Self::ITEM_HEIGHT,
                    );

                    let is_hovered = self.hovered_index == Some(i);
                    if is_hovered {
                        prims.push(RenderPrimitive::solid_rect(
                            [
                                item_rect.x * scale,
                                item_rect.y * scale,
                                item_rect.width * scale,
                                item_rect.height * scale,
                            ],
                            theme.colors.surface_hover,
                            CornerRadii::all(theme.border_radius_sm * scale),
                        ));
                    }

                    let text_color = if is_hovered {
                        theme.colors.text
                    } else {
                        [
                            theme.colors.text[0] * 0.9,
                            theme.colors.text[1] * 0.9,
                            theme.colors.text[2] * 0.9,
                            1.0,
                        ]
                    };

                    let shaped = shaper.shape(label, self.font_size);
                    let text_x = item_rect.x + 8.0;
                    let baseline_y = item_rect.y + (item_rect.height - shaped.height) / 2.0 + shaped.ascent;

                    emit_text_glyphs(
                        &shaped,
                        text_x,
                        baseline_y,
                        self.font_size,
                        text_color,
                        scale,
                        atlas,
                        rasterizer,
                        &mut prims,
                    );

                    if let Some(sc) = shortcut {
                        let sc_shaped = shaper.shape(sc, self.font_size * 0.85);
                        let sc_x = item_rect.x + item_rect.width - sc_shaped.width - 8.0;
                        let sc_baseline = item_rect.y
                            + (item_rect.height - sc_shaped.height) / 2.0
                            + sc_shaped.ascent;
                        emit_text_glyphs(
                            &sc_shaped,
                            sc_x,
                            sc_baseline,
                            self.font_size * 0.85,
                            theme.colors.text_secondary,
                            scale,
                            atlas,
                            rasterizer,
                            &mut prims,
                        );
                    }

                    curr_y += Self::ITEM_HEIGHT;
                }
            }
        }

        prims
    }

    fn as_any(&self) -> &dyn Any {
        self
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}

pub struct Dropdown<T> {
    pub options: Vec<T>,
    pub selected: Property<usize>,
    pub font_size: f32,
    pub style: FlexStyle,
    pub is_hovered: bool,
    pub is_focused: bool,
    pub is_open: bool,
    pub request_popup: bool,
    pub on_select: Option<Box<dyn FnMut(usize, T)>>,
}

impl<T: Clone + ToString + 'static> Dropdown<T> {
    pub fn new(options: Vec<T>) -> Self {
        Self {
            options,
            selected: Property::new(0),
            font_size: 13.0,
            style: FlexStyle {
                direction: FlexDirection::Row,
                align_items: AlignItems::Center,
                padding: EdgeInsets::symmetric(6.0, 12.0),
                width: Dimension::Points(180.0),
                height: Dimension::Points(32.0),
                ..Default::default()
            },
            is_hovered: false,
            is_focused: false,
            is_open: false,
            request_popup: false,
            on_select: None,
        }
    }

    pub fn with_selected(mut self, index: usize) -> Self {
        let max_idx = self.options.len().saturating_sub(1);
        self.selected.set(index.min(max_idx));
        self
    }

    pub fn with_width(mut self, width: f32) -> Self {
        self.style.width = Dimension::Points(width);
        self
    }

    pub fn on_select<F: FnMut(usize, T) + 'static>(mut self, cb: F) -> Self {
        self.on_select = Some(Box::new(cb));
        self
    }

    pub fn selected_value(&self) -> Option<T> {
        let idx = *self.selected.get();
        self.options.get(idx).cloned()
    }

    pub fn selected_label(&self) -> String {
        self.selected_value()
            .map(|v| v.to_string())
            .unwrap_or_default()
    }

    pub fn select_index(&mut self, idx: usize) {
        if idx < self.options.len() {
            self.selected.set(idx);
            let val = self.options[idx].clone();
            if let Some(cb) = &mut self.on_select {
                cb(idx, val);
            }
        }
        self.is_open = false;
    }

    pub fn build_popup_menu(&self) -> PopupMenu {
        let mut items = Vec::with_capacity(self.options.len());
        for opt in &self.options {
            let label = opt.to_string();
            items.push(MenuItem::action(label, move || {}));
        }
        PopupMenu::new(items)
    }
}

impl<T: Clone + ToString + 'static> Widget for Dropdown<T> {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn is_focusable(&self) -> bool {
        true
    }

    fn on_focus_change(&mut self, focused: bool) {
        self.is_focused = focused;
    }

    fn on_popup_closed(&mut self, selected_index: Option<usize>) {
        if let Some(idx) = selected_index {
            self.select_index(idx);
        } else {
            self.is_open = false;
        }
    }

    fn take_popup_request(&mut self, _rect: LayoutRect) -> Option<PopupMenu> {
        if self.request_popup {
            self.request_popup = false;
            Some(self.build_popup_menu())
        } else {
            None
        }
    }

    fn on_pointer_event(&mut self, event: &PointerEvent, rect: LayoutRect) -> EventResponse {
        match *event {
            PointerEvent::Move { x, y } => {
                let inside = rect.contains(x, y);
                if inside != self.is_hovered {
                    self.is_hovered = inside;
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Down { x, y, button } => {
                if button == MouseButton::Left && rect.contains(x, y) {
                    self.is_open = !self.is_open;
                    if self.is_open {
                        self.request_popup = true;
                    }
                    return EventResponse::Handled;
                }
            }
            _ => {}
        }
        EventResponse::Ignored
    }

    fn on_key_event(&mut self, event: &KeyEvent) -> EventResponse {
        if self.is_focused {
            if let KeyEvent::KeyDown { key, keysym, .. } = event {
                if key == "Return" || key == "Space" || *keysym == 0xff0d || *keysym == 0x20 {
                    self.is_open = true;
                    self.request_popup = true;
                    return EventResponse::Handled;
                }
            }
        }
        EventResponse::Ignored
    }

    fn paint(
        &self,
        rect: LayoutRect,
        theme: &Theme,
        atlas: &mut GlyphAtlas,
        rasterizer: &GlyphRasterizer,
        shaper: &TextShaper,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let mut prims = Vec::new();

        let border_color = if self.is_open || self.is_focused {
            theme.colors.border_focused
        } else if self.is_hovered {
            theme.colors.border_focused
        } else {
            theme.colors.border
        };

        let bg_color = if self.is_hovered || self.is_open {
            theme.colors.surface_hover
        } else {
            theme.colors.surface
        };

        prims.push(RenderPrimitive::bordered_rect(
            [
                rect.x * scale,
                rect.y * scale,
                rect.width * scale,
                rect.height * scale,
            ],
            bg_color,
            CornerRadii::all(theme.border_radius_sm * scale),
            Border::new(border_color, 1.0 * scale),
        ));

        // Draw selected text
        let label = self.selected_label();
        let shaped = shaper.shape(&label, self.font_size);
        let text_x = rect.x + self.style.padding.left;
        let baseline_y = rect.y + (rect.height - shaped.height) / 2.0 + shaped.ascent;

        emit_text_glyphs(
            &shaped,
            text_x,
            baseline_y,
            self.font_size,
            theme.colors.text,
            scale,
            atlas,
            rasterizer,
            &mut prims,
        );

        // Draw chevron indicator (▼ character or glyph)
        let chevron_shaped = shaper.shape("▼", self.font_size * 0.7);
        let chevron_x = rect.x + rect.width - chevron_shaped.width - self.style.padding.right;
        let chevron_baseline = rect.y + (rect.height - chevron_shaped.height) / 2.0 + chevron_shaped.ascent;

        emit_text_glyphs(
            &chevron_shaped,
            chevron_x,
            chevron_baseline,
            self.font_size * 0.7,
            theme.colors.text_secondary,
            scale,
            atlas,
            rasterizer,
            &mut prims,
        );

        prims
    }

    fn as_any(&self) -> &dyn Any {
        self
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicBool, Ordering};
    use std::sync::Arc;

    #[test]
    fn test_popup_menu_selection_and_keyboard_nav() {
        let item1_clicked = Arc::new(AtomicBool::new(false));
        let c1 = item1_clicked.clone();
        let item2_clicked = Arc::new(AtomicBool::new(false));
        let c2 = item2_clicked.clone();

        let items = vec![
            MenuItem::action("Item 1", move || c1.store(true, Ordering::SeqCst)),
            MenuItem::separator(),
            MenuItem::action_with_shortcut("Item 2", "Ctrl+2", move || c2.store(true, Ordering::SeqCst)),
        ];

        let mut menu = PopupMenu::new(items);
        assert_eq!(menu.hovered_index, None);

        // Arrow down -> selects Item 1
        menu.select_next();
        assert_eq!(menu.hovered_index, Some(0));

        // Arrow down -> skips separator (idx 1) -> selects Item 2 (idx 2)
        menu.select_next();
        assert_eq!(menu.hovered_index, Some(2));

        // Arrow up -> skips separator -> selects Item 1
        menu.select_prev();
        assert_eq!(menu.hovered_index, Some(0));

        // Trigger hovered
        assert!(menu.trigger_hovered());
        assert!(item1_clicked.load(Ordering::SeqCst));
        assert!(menu.is_dismissed);
        assert_eq!(menu.selected_index, Some(0));
    }

    #[test]
    fn test_dropdown_selection_state() {
        let dropdown = Dropdown::new(vec!["Option A".to_string(), "Option B".to_string(), "Option C".to_string()])
            .with_selected(1);
        assert_eq!(dropdown.selected_label(), "Option B");
        assert_eq!(*dropdown.selected.get(), 1);
    }
}
