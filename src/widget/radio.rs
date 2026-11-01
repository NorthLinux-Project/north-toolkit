//! Radio button and mutually exclusive radio group widgets with animated dot indicators.

use std::any::Any;
use std::cell::RefCell;

use super::state::Property;
use super::tooltip::Tooltip;
use super::tree::Widget;
use crate::animation::{Easing, Tween};
use crate::event::{CursorIcon, EventResponse, KeyEvent, MouseButton, PointerEvent};
use crate::layout::{
    AlignItems, Dimension, EdgeInsets, FlexDirection, FlexStyle, LayoutRect, MeasureFunc,
};
use crate::render::{Border, CornerRadii, GlyphAtlas, RenderPrimitive, VectorAtlas};
use crate::text::{emit_text_glyphs, GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// Layout orientation for multi-option radio button groups.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum RadioGroupOrientation {
    /// Items stacked vertically one per row.
    #[default]
    Vertical,
    /// Items laid out horizontally side-by-side.
    Horizontal,
}

/// A single circular radio button widget with label and animated selection dot.
pub struct RadioButton {
    /// Label text displayed beside the circular indicator.
    pub label: String,
    /// Reactive selection state.
    pub selected: Property<bool>,
    /// Font size in logical points.
    pub font_size: f32,
    /// Flexbox layout configuration.
    pub style: FlexStyle,
    /// Whether pointer is hovering over this button.
    pub is_hovered: bool,
    /// Whether this button holds keyboard focus.
    pub is_focused: bool,
    /// Callback triggered when this button is selected.
    pub on_select: Option<Box<dyn FnMut()>>,
    /// Optional hover tooltip.
    pub tooltip: Option<Tooltip>,
    dot_tween: Tween<f32>,
}

impl RadioButton {
    /// Diameter of the outer circular radio indicator in logical points.
    pub const INDICATOR_SIZE: f32 = 18.0;

    /// Creates an unselected radio button with the given label.
    pub fn new<S: Into<String>>(label: S) -> Self {
        Self {
            label: label.into(),
            selected: Property::new(false),
            font_size: 14.0,
            style: FlexStyle {
                direction: FlexDirection::Row,
                align_items: AlignItems::Center,
                padding: EdgeInsets::symmetric(4.0, 8.0),
                width: Dimension::Auto,
                height: Dimension::Auto,
                ..Default::default()
            },
            is_hovered: false,
            is_focused: false,
            on_select: None,
            tooltip: None,
            dot_tween: Tween::new(0.0),
        }
    }

    /// Sets the initial selection state.
    pub fn with_selected(mut self, selected: bool) -> Self {
        self.selected.set(selected);
        self.dot_tween = Tween::new(if selected { 1.0 } else { 0.0 });
        self
    }

    /// Attaches a hover tooltip to the radio button.
    pub fn with_tooltip<S: Into<String>>(mut self, text: S) -> Self {
        self.tooltip = Some(Tooltip::new(text));
        self
    }

    /// Registers a selection callback handler.
    pub fn on_select<F: FnMut() + 'static>(mut self, cb: F) -> Self {
        self.on_select = Some(Box::new(cb));
        self
    }

    /// Selects this radio button, initiating the dot expansion animation.
    pub fn select(&mut self) {
        if !*self.selected.get() {
            self.selected.set(true);
            self.dot_tween.set_target(1.0, 160.0, Easing::EaseOut);
            if let Some(cb) = &mut self.on_select {
                cb();
            }
        }
    }

    /// Sets the selection state directly, animating the dot accordingly.
    pub fn set_selected(&mut self, selected: bool) {
        self.selected.set(selected);
        let target = if selected { 1.0 } else { 0.0 };
        self.dot_tween.set_target(target, 160.0, Easing::EaseOut);
    }
}

impl Widget for RadioButton {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn measure(&self, shaper: &TextShaper) -> Option<MeasureFunc> {
        let shaped = shaper.shape(&self.label, self.font_size);
        let box_size = Self::INDICATOR_SIZE;
        let gap = 8.0;
        let pad_h = self.style.padding.horizontal();
        let pad_v = self.style.padding.vertical();

        let w = box_size + gap + shaped.width + pad_h;
        let h = box_size.max(shaped.height) + pad_v;
        Some(Box::new(move |_avail_w, _avail_h| (w, h)))
    }

    fn is_focusable(&self) -> bool {
        true
    }

    fn on_focus_change(&mut self, focused: bool) {
        self.is_focused = focused;
    }

    fn tooltip(&self) -> Option<Tooltip> {
        self.tooltip.clone()
    }

    fn cursor_icon_at(&self, _rel_x: f32, _rel_y: f32) -> CursorIcon {
        CursorIcon::Pointer
    }

    fn tick_animation(&mut self, delta_ms: f32) -> bool {
        if !self.dot_tween.is_complete() {
            self.dot_tween.tick(delta_ms);
            true
        } else {
            false
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
                    self.select();
                    return EventResponse::Handled;
                }
            }
            _ => {}
        }
        EventResponse::Ignored
    }

    fn on_key_event(&mut self, event: &KeyEvent) -> EventResponse {
        if self.is_focused {
            if let KeyEvent::KeyDown { key, keysym, is_repeat, .. } = event {
                if !*is_repeat
                    && (key == "Return"
                        || key == "Enter"
                        || key == "Space"
                        || *keysym == 0xff0d
                        || *keysym == 0xff8d
                        || *keysym == 0x20)
                {
                    self.select();
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
        _vector_atlas: &mut VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let mut prims = Vec::new();
        let center_y = rect.y + rect.height * 0.5;
        let box_size = Self::INDICATOR_SIZE;
        let pad_left = self.style.padding.left;
        let box_x = rect.x + pad_left;
        let box_y = center_y - box_size * 0.5;

        let is_sel = *self.selected.get();
        let border_color = if self.is_focused {
            theme.colors.border_focused
        } else if is_sel {
            theme.colors.primary
        } else if self.is_hovered {
            theme.colors.primary_hover
        } else {
            theme.colors.border
        };

        let border_w = if self.is_focused || is_sel {
            2.0 * scale
        } else {
            1.5 * scale
        };

        let outer_bg = if self.is_hovered {
            theme.colors.surface_hover
        } else {
            theme.colors.surface
        };

        let radii = CornerRadii::all(box_size * 0.5 * scale);

        // Outer circular border
        prims.push(RenderPrimitive::bordered_rect(
            [
                box_x * scale,
                box_y * scale,
                box_size * scale,
                box_size * scale,
            ],
            outer_bg,
            radii,
            Border::new(border_color, border_w),
        ));

        // Inner animated dot
        let dot_progress = self.dot_tween.current.clamp(0.0, 1.0);
        if dot_progress > 0.01 {
            let max_dot_diameter = 8.0;
            let current_diameter = max_dot_diameter * dot_progress;
            let dot_x = box_x + (box_size - current_diameter) * 0.5;
            let dot_y = box_y + (box_size - current_diameter) * 0.5;
            let dot_radii = CornerRadii::all(current_diameter * 0.5 * scale);

            let mut dot_color = theme.colors.primary;
            dot_color[3] *= dot_progress;

            prims.push(RenderPrimitive::solid_rect(
                [
                    dot_x * scale,
                    dot_y * scale,
                    current_diameter * scale,
                    current_diameter * scale,
                ],
                dot_color,
                dot_radii,
            ));
        }

        // Label text
        let shaped = shaper.shape(&self.label, self.font_size);
        let text_x = box_x + box_size + 8.0;
        let text_y = center_y - shaped.height * 0.5;
        let baseline = text_y + shaped.height * 0.78;

        let text_color = if self.is_hovered {
            theme.colors.text
        } else {
            theme.colors.text_secondary
        };

        emit_text_glyphs(
            &shaped,
            text_x,
            baseline,
            self.font_size,
            text_color,
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

/// Mutually exclusive multi-option radio group container with arrow key cycling.
pub struct RadioGroup {
    /// List of option string labels.
    pub items: Vec<String>,
    /// Reactive selected item index, if any.
    pub selected_index: Property<Option<usize>>,
    /// Layout orientation.
    pub orientation: RadioGroupOrientation,
    /// Currently hovered item index, if any.
    pub hovered_index: Option<usize>,
    /// Currently focused item index for keyboard navigation.
    pub focused_index: usize,
    /// Whether the radio group container holds keyboard focus.
    pub is_focused: bool,
    /// Font size in logical points.
    pub font_size: f32,
    /// Flexbox layout configuration.
    pub style: FlexStyle,
    /// Selection change callback handler.
    pub on_select: Option<Box<dyn FnMut(usize)>>,
    dot_tweens: Vec<Tween<f32>>,
    cached_widths: RefCell<Vec<f32>>,
}

impl RadioGroup {
    /// Creates a new empty radio group with the specified orientation.
    pub fn new(orientation: RadioGroupOrientation) -> Self {
        let dir = match orientation {
            RadioGroupOrientation::Vertical => FlexDirection::Column,
            RadioGroupOrientation::Horizontal => FlexDirection::Row,
        };

        Self {
            items: Vec::new(),
            selected_index: Property::new(None),
            orientation,
            hovered_index: None,
            focused_index: 0,
            is_focused: false,
            font_size: 14.0,
            style: FlexStyle {
                direction: dir,
                align_items: AlignItems::FlexStart,
                gap: 8.0,
                padding: EdgeInsets::symmetric(4.0, 4.0),
                width: Dimension::Auto,
                height: Dimension::Auto,
                ..Default::default()
            },
            on_select: None,
            dot_tweens: Vec::new(),
            cached_widths: RefCell::new(Vec::new()),
        }
    }

    /// Appends an option label to the group.
    pub fn with_option<S: Into<String>>(mut self, label: S) -> Self {
        self.add_option(label);
        self
    }

    /// Appends an option label dynamically.
    pub fn add_option<S: Into<String>>(&mut self, label: S) -> &mut Self {
        self.items.push(label.into());
        self.dot_tweens.push(Tween::new(0.0));
        self.cached_widths.borrow_mut().clear();
        self
    }

    /// Sets the initial selected option index.
    pub fn with_selected(mut self, index: usize) -> Self {
        if index < self.items.len() {
            self.selected_index.set(Some(index));
            self.focused_index = index;
            for (i, tween) in self.dot_tweens.iter_mut().enumerate() {
                *tween = Tween::new(if i == index { 1.0 } else { 0.0 });
            }
        }
        self
    }

    /// Registers a selection callback handler.
    pub fn on_select<F: FnMut(usize) + 'static>(mut self, cb: F) -> Self {
        self.on_select = Some(Box::new(cb));
        self
    }

    /// Selects the option at `index`, triggering the callback and dot animations.
    pub fn select(&mut self, index: usize) {
        if index >= self.items.len() {
            return;
        }

        let prev = *self.selected_index.get();
        if prev != Some(index) {
            self.selected_index.set(Some(index));
            self.focused_index = index;

            for (i, tween) in self.dot_tweens.iter_mut().enumerate() {
                let target = if i == index { 1.0 } else { 0.0 };
                tween.set_target(target, 160.0, Easing::EaseOut);
            }

            if let Some(cb) = &mut self.on_select {
                cb(index);
            }
        }
    }

    /// Computes the bounding rectangles of each option item.
    pub fn item_rects(&self, container_rect: LayoutRect, shaper: Option<&TextShaper>) -> Vec<LayoutRect> {
        let mut rects = Vec::with_capacity(self.items.len());
        let item_h = 24.0;
        let box_size = RadioButton::INDICATOR_SIZE;
        let gap_text = 8.0;
        let cached = self.cached_widths.borrow();

        match self.orientation {
            RadioGroupOrientation::Vertical => {
                let mut curr_y = container_rect.y + self.style.padding.top;
                let item_w = container_rect.width - self.style.padding.horizontal();

                for (i, item) in self.items.iter().enumerate() {
                    let text_w = match shaper {
                        Some(s) => s.shape(item, self.font_size).width,
                        None => cached
                            .get(i)
                            .copied()
                            .unwrap_or_else(|| (item.len() as f32 * self.font_size * 0.55).max(20.0)),
                    };
                    let w = (box_size + gap_text + text_w + 12.0).max(item_w);
                    rects.push(LayoutRect::new(
                        container_rect.x + self.style.padding.left,
                        curr_y,
                        w,
                        item_h,
                    ));
                    curr_y += item_h + self.style.gap;
                }
            }
            RadioGroupOrientation::Horizontal => {
                let mut curr_x = container_rect.x + self.style.padding.left;
                let item_y = container_rect.y + self.style.padding.top;

                for (i, item) in self.items.iter().enumerate() {
                    let text_w = match shaper {
                        Some(s) => s.shape(item, self.font_size).width,
                        None => cached
                            .get(i)
                            .copied()
                            .unwrap_or_else(|| (item.len() as f32 * self.font_size * 0.55).max(20.0)),
                    };
                    let w = box_size + gap_text + text_w + 12.0;
                    rects.push(LayoutRect::new(curr_x, item_y, w, item_h));
                    curr_x += w + self.style.gap;
                }
            }
        }

        rects
    }
}

impl Widget for RadioGroup {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn measure(&self, shaper: &TextShaper) -> Option<MeasureFunc> {
        let orientation = self.orientation;
        let items: Vec<String> = self.items.clone();
        let font_size = self.font_size;
        let pad_h = self.style.padding.horizontal();
        let pad_v = self.style.padding.vertical();
        let gap = self.style.gap;
        let box_size = RadioButton::INDICATOR_SIZE;

        let mut max_item_w = 0.0f32;
        let mut sum_item_w = 0.0f32;
        let mut widths = Vec::with_capacity(items.len());
        for item in &items {
            let text_w = shaper.shape(item, font_size).width;
            widths.push(text_w);
            let w = box_size + 8.0 + text_w + 12.0;
            max_item_w = max_item_w.max(w);
            sum_item_w += w;
        }
        *self.cached_widths.borrow_mut() = widths;

        let count = items.len();
        let total_gaps = if count > 1 {
            (count - 1) as f32 * gap
        } else {
            0.0
        };

        match orientation {
            RadioGroupOrientation::Vertical => {
                let total_w = max_item_w + pad_h;
                let total_h = count as f32 * 24.0 + total_gaps + pad_v;
                Some(Box::new(move |_avail_w, _avail_h| (total_w, total_h)))
            }
            RadioGroupOrientation::Horizontal => {
                let total_w = sum_item_w + total_gaps + pad_h;
                let total_h = 24.0 + pad_v;
                Some(Box::new(move |_avail_w, _avail_h| (total_w, total_h)))
            }
        }
    }

    fn is_focusable(&self) -> bool {
        true
    }

    fn on_focus_change(&mut self, focused: bool) {
        self.is_focused = focused;
    }

    fn cursor_icon_at(&self, _rel_x: f32, _rel_y: f32) -> CursorIcon {
        if self.hovered_index.is_some() {
            CursorIcon::Pointer
        } else {
            CursorIcon::Default
        }
    }

    fn tick_animation(&mut self, delta_ms: f32) -> bool {
        let mut repainting = false;
        for tween in &mut self.dot_tweens {
            if !tween.is_complete() {
                tween.tick(delta_ms);
                repainting = true;
            }
        }
        repainting
    }

    fn on_pointer_event(&mut self, event: &PointerEvent, rect: LayoutRect) -> EventResponse {
        let rects = self.item_rects(rect, None);

        match *event {
            PointerEvent::Move { x, y } => {
                let mut hit_idx = None;
                for (i, r) in rects.iter().enumerate() {
                    if r.contains(x, y) {
                        hit_idx = Some(i);
                        break;
                    }
                }
                if hit_idx != self.hovered_index {
                    self.hovered_index = hit_idx;
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Down { x, y, button } => {
                if button == MouseButton::Left {
                    for (i, r) in rects.iter().enumerate() {
                        if r.contains(x, y) {
                            self.select(i);
                            return EventResponse::Handled;
                        }
                    }
                }
            }
            _ => {}
        }

        EventResponse::Ignored
    }

    fn on_key_event(&mut self, event: &KeyEvent) -> EventResponse {
        if !self.is_focused || self.items.is_empty() {
            return EventResponse::Ignored;
        }

        if let KeyEvent::KeyDown { key, keysym, .. } = event {
            let count = self.items.len();
            match key.as_str() {
                "Up" | "Left" => {
                    let prev = if self.focused_index == 0 {
                        count - 1
                    } else {
                        self.focused_index - 1
                    };
                    self.select(prev);
                    return EventResponse::Handled;
                }
                "Down" | "Right" => {
                    let next = if self.focused_index + 1 >= count {
                        0
                    } else {
                        self.focused_index + 1
                    };
                    self.select(next);
                    return EventResponse::Handled;
                }
                "Home" => {
                    self.select(0);
                    return EventResponse::Handled;
                }
                "End" => {
                    self.select(count - 1);
                    return EventResponse::Handled;
                }
                "Space" | "Return" | "Enter" => {
                    self.select(self.focused_index);
                    return EventResponse::Handled;
                }
                _ => {
                    if *keysym == 0xff0d || *keysym == 0xff8d || *keysym == 0x20 {
                        self.select(self.focused_index);
                        return EventResponse::Handled;
                    }
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
        _vector_atlas: &mut VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let mut prims = Vec::new();
        let mut widths = Vec::with_capacity(self.items.len());
        for item in &self.items {
            widths.push(shaper.shape(item, self.font_size).width);
        }
        *self.cached_widths.borrow_mut() = widths;
        let rects = self.item_rects(rect, Some(shaper));
        let active_opt = *self.selected_index.get();
        let box_size = RadioButton::INDICATOR_SIZE;

        for (i, item_r) in rects.iter().enumerate() {
            let is_selected = active_opt == Some(i);
            let is_hovered = self.hovered_index == Some(i);
            let is_focused_item = self.is_focused && self.focused_index == i;

            let center_y = item_r.y + item_r.height * 0.5;
            let box_x = item_r.x;
            let box_y = center_y - box_size * 0.5;

            let border_color = if is_focused_item {
                theme.colors.border_focused
            } else if is_selected {
                theme.colors.primary
            } else if is_hovered {
                theme.colors.primary_hover
            } else {
                theme.colors.border
            };

            let border_w = if is_focused_item || is_selected {
                2.0 * scale
            } else {
                1.5 * scale
            };

            let outer_bg = if is_hovered {
                theme.colors.surface_hover
            } else {
                theme.colors.surface
            };

            let radii = CornerRadii::all(box_size * 0.5 * scale);

            // Outer circle
            prims.push(RenderPrimitive::bordered_rect(
                [
                    box_x * scale,
                    box_y * scale,
                    box_size * scale,
                    box_size * scale,
                ],
                outer_bg,
                radii,
                Border::new(border_color, border_w),
            ));

            // Inner animated dot
            let dot_progress = self
                .dot_tweens
                .get(i)
                .map(|t| t.current.clamp(0.0, 1.0))
                .unwrap_or(if is_selected { 1.0 } else { 0.0 });

            if dot_progress > 0.01 {
                let max_dot_diameter = 8.0;
                let current_diameter = max_dot_diameter * dot_progress;
                let dot_x = box_x + (box_size - current_diameter) * 0.5;
                let dot_y = box_y + (box_size - current_diameter) * 0.5;
                let dot_radii = CornerRadii::all(current_diameter * 0.5 * scale);

                let mut dot_color = theme.colors.primary;
                dot_color[3] *= dot_progress;

                prims.push(RenderPrimitive::solid_rect(
                    [
                        dot_x * scale,
                        dot_y * scale,
                        current_diameter * scale,
                        current_diameter * scale,
                    ],
                    dot_color,
                    dot_radii,
                ));
            }

            // Label text
            let shaped = shaper.shape(&self.items[i], self.font_size);
            let text_x = box_x + box_size + 8.0;
            let text_y = center_y - shaped.height * 0.5;
            let baseline = text_y + shaped.height * 0.78;

            let text_color = if is_hovered || is_selected {
                theme.colors.text
            } else {
                theme.colors.text_secondary
            };

            emit_text_glyphs(
                &shaped,
                text_x,
                baseline,
                self.font_size,
                text_color,
                scale,
                atlas,
                rasterizer,
                &mut prims,
            );
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_radio_button_toggle() {
        let mut radio = RadioButton::new("Option A");
        assert!(!*radio.selected.get());

        radio.select();
        assert!(*radio.selected.get());
    }

    #[test]
    fn test_radio_group_mutual_exclusion() {
        let mut group = RadioGroup::new(RadioGroupOrientation::Vertical)
            .with_option("Alpha")
            .with_option("Beta")
            .with_option("Gamma");

        assert_eq!(*group.selected_index.get(), None);

        group.select(0);
        assert_eq!(*group.selected_index.get(), Some(0));

        group.select(2);
        assert_eq!(*group.selected_index.get(), Some(2));
    }

    #[test]
    fn test_radio_group_arrow_navigation() {
        let mut group = RadioGroup::new(RadioGroupOrientation::Vertical)
            .with_option("Option 1")
            .with_option("Option 2")
            .with_selected(0);

        group.on_focus_change(true);

        let down_key = KeyEvent::KeyDown {
            key: "Down".into(),
            keysym: 0xff54,
            text: None,
            modifiers: Default::default(),
            is_repeat: false,
        };
        assert_eq!(group.on_key_event(&down_key), EventResponse::Handled);
        assert_eq!(*group.selected_index.get(), Some(1));

        // Down from 1 wraps to 0
        assert_eq!(group.on_key_event(&down_key), EventResponse::Handled);
        assert_eq!(*group.selected_index.get(), Some(0));
    }

    #[test]
    fn test_radio_group_horizontal_cached_widths() {
        let group = RadioGroup::new(RadioGroupOrientation::Horizontal)
            .with_option("Short")
            .with_option("A Much Longer Option Label Text");

        let font_db = crate::text::font_db::FontDatabase::new().unwrap();
        let shaper = TextShaper::new(font_db);

        // Calling measure populates cached_widths
        let _ = group.measure(&shaper);
        assert_eq!(group.cached_widths.borrow().len(), 2);

        let rect = LayoutRect::new(0.0, 0.0, 500.0, 50.0);
        let rects_with_shaper = group.item_rects(rect, Some(&shaper));
        let rects_without_shaper = group.item_rects(rect, None);

        // Rects must match exactly because cached widths were populated
        assert_eq!(rects_with_shaper[0], rects_without_shaper[0]);
        assert_eq!(rects_with_shaper[1], rects_without_shaper[1]);
    }

    #[test]
    fn test_radio_group_home_end_and_enter_keys() {
        let mut group = RadioGroup::new(RadioGroupOrientation::Vertical)
            .with_option("First")
            .with_option("Second")
            .with_option("Third")
            .with_selected(1);

        group.on_focus_change(true);

        // Home key selects first (0)
        let home_key = KeyEvent::KeyDown {
            key: "Home".into(),
            keysym: 0xff50,
            text: None,
            modifiers: Default::default(),
            is_repeat: false,
        };
        assert_eq!(group.on_key_event(&home_key), EventResponse::Handled);
        assert_eq!(*group.selected_index.get(), Some(0));

        // End key selects last (2)
        let end_key = KeyEvent::KeyDown {
            key: "End".into(),
            keysym: 0xff57,
            text: None,
            modifiers: Default::default(),
            is_repeat: false,
        };
        assert_eq!(group.on_key_event(&end_key), EventResponse::Handled);
        assert_eq!(*group.selected_index.get(), Some(2));

        // Enter keysym selects currently focused
        let enter_key = KeyEvent::KeyDown {
            key: "Enter".into(),
            keysym: 0xff0d,
            text: None,
            modifiers: Default::default(),
            is_repeat: false,
        };
        assert_eq!(group.on_key_event(&enter_key), EventResponse::Handled);
        assert_eq!(*group.selected_index.get(), Some(2));
    }

    #[test]
    fn test_radio_button_enter_and_space() {
        let mut btn = RadioButton::new("Test");
        btn.on_focus_change(true);

        let space_key = KeyEvent::KeyDown {
            key: "Space".into(),
            keysym: 0x20,
            text: None,
            modifiers: Default::default(),
            is_repeat: false,
        };
        assert_eq!(btn.on_key_event(&space_key), EventResponse::Handled);
        assert!(*btn.selected.get());
    }
}
