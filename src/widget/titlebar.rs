//! Client-side window decorations titlebar with interactive control buttons and double-click maximize.

use std::any::Any;
use std::time::Instant;

use super::icon::IconName;
use super::state::Property;
use super::tree::{Widget, WindowAction};
use crate::event::{CursorIcon, EventResponse, MouseButton, PointerEvent};
use crate::layout::{Dimension, FlexStyle, LayoutRect, MeasureFunc};
use crate::render::vector::{StrokeStyle, VectorAtlas, VectorPath, VectorRasterizer};
use crate::render::{CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;

/// Window control button type located on the right side of the titlebar.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TitlebarControl {
    /// Minimize window to taskbar or tray.
    Minimize,
    /// Maximize or restore window dimensions.
    Maximize,
    /// Close window.
    Close,
}

/// Client-side titlebar widget handling window dragging, double-click maximize, and system buttons.
pub struct Titlebar {
    /// Window title string property.
    pub title: Property<String>,
    /// Optional leading window icon.
    pub icon: Option<IconName>,
    /// Whether to display the minimize control button.
    pub show_minimize: bool,
    /// Whether to display the maximize/restore control button.
    pub show_maximize: bool,
    /// Whether to display the close control button.
    pub show_close: bool,
    /// Reactive maximized state of the parent window.
    pub is_maximized: Property<bool>,
    /// Reactive active/focused status of the parent window.
    pub is_active: Property<bool>,
    /// Flexbox layout style.
    pub style: FlexStyle,
    hovered_control: Option<TitlebarControl>,
    pressed_control: Option<TitlebarControl>,
    last_click_instant: Option<Instant>,
    pending_action: Option<WindowAction>,
}

impl Titlebar {
    /// Default height of the titlebar in logical points.
    pub const HEIGHT: f32 = 38.0;
    /// Width of individual window control buttons in points.
    pub const BUTTON_WIDTH: f32 = 44.0;

    /// Creates a new titlebar widget with the specified window title.
    pub fn new<S: Into<String>>(title: S) -> Self {
        Self {
            title: Property::new(title.into()),
            icon: None,
            show_minimize: true,
            show_maximize: true,
            show_close: true,
            is_maximized: Property::new(false),
            is_active: Property::new(true),
            style: FlexStyle {
                width: Dimension::Percent(100.0),
                height: Dimension::Points(Self::HEIGHT),
                ..Default::default()
            },
            hovered_control: None,
            pressed_control: None,
            last_click_instant: None,
            pending_action: None,
        }
    }

    /// Sets the leading window title icon.
    pub fn with_icon(mut self, icon: IconName) -> Self {
        self.icon = Some(icon);
        self
    }

    /// Sets whether the minimize button is visible.
    pub fn with_show_minimize(mut self, show: bool) -> Self {
        self.show_minimize = show;
        self
    }

    /// Sets whether the maximize button is visible.
    pub fn with_show_maximize(mut self, show: bool) -> Self {
        self.show_maximize = show;
        self
    }

    /// Sets whether the close button is visible.
    pub fn with_show_close(mut self, show: bool) -> Self {
        self.show_close = show;
        self
    }

    /// Resolves which window control button (if any) contains the coordinates `(x, y)`.
    pub fn control_at(&self, rect: LayoutRect, x: f32, y: f32) -> Option<TitlebarControl> {
        if !rect.contains(x, y) {
            return None;
        }

        let mut curr_right = rect.x + rect.width;

        if self.show_close {
            let left = curr_right - Self::BUTTON_WIDTH;
            if x >= left && x < curr_right {
                return Some(TitlebarControl::Close);
            }
            curr_right = left;
        }

        if self.show_maximize {
            let left = curr_right - Self::BUTTON_WIDTH;
            if x >= left && x < curr_right {
                return Some(TitlebarControl::Maximize);
            }
            curr_right = left;
        }

        if self.show_minimize {
            let left = curr_right - Self::BUTTON_WIDTH;
            if x >= left && x < curr_right {
                return Some(TitlebarControl::Minimize);
            }
        }

        None
    }
}

impl Widget for Titlebar {
    fn layout_style(&self) -> FlexStyle {
        self.style.clone()
    }

    fn measure(&self, _shaper: &TextShaper) -> Option<MeasureFunc> {
        None
    }

    fn take_window_action(&mut self) -> Option<WindowAction> {
        self.pending_action.take()
    }

    fn cursor_icon_at(&self, _rel_x: f32, _rel_y: f32) -> CursorIcon {
        CursorIcon::Default
    }

    fn on_pointer_event(&mut self, event: &PointerEvent, rect: LayoutRect) -> EventResponse {
        let (px, py) = event.position();

        match *event {
            PointerEvent::Move { x, y } => {
                let new_hover = self.control_at(rect, x, y);
                if new_hover != self.hovered_control {
                    self.hovered_control = new_hover;
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Down { button, .. } => {
                if button == MouseButton::Left && rect.contains(px, py) {
                    if let Some(ctrl) = self.control_at(rect, px, py) {
                        self.pressed_control = Some(ctrl);
                        return EventResponse::Handled;
                    }

                    // Click on blank titlebar region
                    let now = Instant::now();
                    let is_double_click = if let Some(last) = self.last_click_instant {
                        now.duration_since(last).as_millis() < 300
                    } else {
                        false
                    };

                    if is_double_click {
                        self.last_click_instant = None;
                        self.pending_action = Some(WindowAction::ToggleMaximize);
                    } else {
                        self.last_click_instant = Some(now);
                        self.pending_action = Some(WindowAction::InteractiveMove);
                    }
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Up { button, .. } => {
                if button == MouseButton::Left {
                    if let Some(pressed) = self.pressed_control.take() {
                        if self.control_at(rect, px, py) == Some(pressed) {
                            match pressed {
                                TitlebarControl::Close => {
                                    self.pending_action = Some(WindowAction::Close);
                                }
                                TitlebarControl::Maximize => {
                                    self.pending_action = Some(WindowAction::ToggleMaximize);
                                }
                                TitlebarControl::Minimize => {
                                    self.pending_action = Some(WindowAction::Minimize);
                                }
                            }
                            return EventResponse::Handled;
                        }
                    }
                }
            }
            _ => {}
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
        vector_atlas: &mut VectorAtlas,
        scale: f32,
    ) -> Vec<RenderPrimitive> {
        let mut prims = Vec::new();

        let is_active = *self.is_active.get();

        // Titlebar background quad
        let bg_color = if is_active {
            [0.10, 0.10, 0.10, 1.0]
        } else {
            [0.08, 0.08, 0.08, 1.0]
        };

        prims.push(RenderPrimitive::solid_rect(
            [rect.x * scale, rect.y * scale, rect.width * scale, rect.height * scale],
            bg_color,
            CornerRadii::all(0.0),
        ));

        // Bottom border divider
        let border_color = if is_active {
            [0.20, 0.20, 0.20, 1.0]
        } else {
            [0.15, 0.15, 0.15, 1.0]
        };
        prims.push(RenderPrimitive::solid_rect(
            [
                rect.x * scale,
                (rect.y + rect.height - 1.0) * scale,
                rect.width * scale,
                1.0 * scale,
            ],
            border_color,
            CornerRadii::all(0.0),
        ));

        let center_y = rect.y + rect.height * 0.5;

        // Window icon
        let mut text_offset_x = 14.0;
        if let Some(icon) = self.icon {
            let icon_size = 16.0;
            let icon_x = rect.x + text_offset_x;
            let icon_y = center_y - icon_size * 0.5;
            let phys_size = ((icon_size * scale).round() as u32).max(1);
            let stroke_w = (1.5 * (phys_size as f32 / 24.0)).max(1.0);

            let uv = vector_atlas.get_or_insert(icon as u64, phys_size, phys_size, || {
                let path = VectorPath::parse_svg(icon.svg_path()).with_view_box(0.0, 0.0, 24.0, 24.0);
                VectorRasterizer::rasterize(
                    &path,
                    phys_size,
                    phys_size,
                    None,
                    Some(StrokeStyle {
                        color: [1.0, 1.0, 1.0, 1.0],
                        width: stroke_w,
                        ..Default::default()
                    }),
                )
            });

            if let Some(uv_rect) = uv {
                let icon_color = if is_active {
                    [0.85, 0.85, 0.85, 1.0]
                } else {
                    [0.50, 0.50, 0.50, 1.0]
                };
                prims.push(RenderPrimitive::Rect {
                    rect: [
                        icon_x * scale,
                        icon_y * scale,
                        icon_size * scale,
                        icon_size * scale,
                    ],
                    color: icon_color,
                    radii: CornerRadii::all(0.0),
                    border: None,
                    uv_rect,
                    use_texture: true,
                });
            }
            text_offset_x += icon_size + 8.0;
        }

        // Title text
        let font_size = 13.0;
        let title_str = self.title.get();
        let shaped = shaper.shape(title_str, font_size);
        let title_x = rect.x + text_offset_x;
        let title_baseline = center_y + font_size * 0.35;
        let text_color = if is_active {
            theme.colors.text
        } else {
            theme.colors.text_secondary
        };

        crate::text::emit_text_glyphs(
            &shaped,
            title_x,
            title_baseline,
            font_size,
            text_color,
            scale,
            atlas,
            rasterizer,
            &mut prims,
        );

        // Control buttons
        let mut curr_right = rect.x + rect.width;

        // Helper closure to emit vector button icon
        let emit_btn_icon = |icon: IconName,
                                 btn_x: f32,
                                 size: f32,
                                 stroke: f32,
                                 color: [f32; 4],
                                 vec_atlas: &mut VectorAtlas,
                                 prims_out: &mut Vec<RenderPrimitive>| {
            let icon_x = btn_x + (Self::BUTTON_WIDTH - size) * 0.5;
            let icon_y = center_y - size * 0.5;
            let phys_size = ((size * scale).round() as u32).max(1);
            let stroke_w = (stroke * (phys_size as f32 / 24.0)).max(1.0);

            let uv = vec_atlas.get_or_insert(icon as u64, phys_size, phys_size, || {
                let path = VectorPath::parse_svg(icon.svg_path()).with_view_box(0.0, 0.0, 24.0, 24.0);
                VectorRasterizer::rasterize(
                    &path,
                    phys_size,
                    phys_size,
                    None,
                    Some(StrokeStyle {
                        color: [1.0, 1.0, 1.0, 1.0],
                        width: stroke_w,
                        ..Default::default()
                    }),
                )
            });

            if let Some(uv_rect) = uv {
                prims_out.push(RenderPrimitive::Rect {
                    rect: [
                        icon_x * scale,
                        icon_y * scale,
                        size * scale,
                        size * scale,
                    ],
                    color,
                    radii: CornerRadii::all(0.0),
                    border: None,
                    uv_rect,
                    use_texture: true,
                });
            }
        };

        // Close button
        if self.show_close {
            let btn_x = curr_right - Self::BUTTON_WIDTH;
            let is_hover = self.hovered_control == Some(TitlebarControl::Close);
            if is_hover {
                prims.push(RenderPrimitive::solid_rect(
                    [
                        btn_x * scale,
                        rect.y * scale,
                        Self::BUTTON_WIDTH * scale,
                        rect.height * scale,
                    ],
                    [0.91, 0.15, 0.20, 1.0],
                    CornerRadii::all(0.0),
                ));
            }

            let icon_color = if is_hover {
                [1.0, 1.0, 1.0, 1.0]
            } else if is_active {
                [0.85, 0.85, 0.85, 1.0]
            } else {
                [0.50, 0.50, 0.50, 1.0]
            };

            emit_btn_icon(
                IconName::Close,
                btn_x,
                12.0,
                1.5,
                icon_color,
                vector_atlas,
                &mut prims,
            );
            curr_right = btn_x;
        }

        // Maximize button
        if self.show_maximize {
            let btn_x = curr_right - Self::BUTTON_WIDTH;
            let is_hover = self.hovered_control == Some(TitlebarControl::Maximize);
            if is_hover {
                let pad_x = 4.0;
                let pad_y = 5.0;
                prims.push(RenderPrimitive::solid_rect(
                    [
                        (btn_x + pad_x) * scale,
                        (rect.y + pad_y) * scale,
                        (Self::BUTTON_WIDTH - pad_x * 2.0) * scale,
                        (rect.height - pad_y * 2.0) * scale,
                    ],
                    [1.0, 1.0, 1.0, 0.08],
                    CornerRadii::all(4.0 * scale),
                ));
            }

            let max_icon = if *self.is_maximized.get() {
                IconName::WindowRestore
            } else {
                IconName::WindowMaximize
            };

            let icon_color = if is_active {
                [0.85, 0.85, 0.85, 1.0]
            } else {
                [0.50, 0.50, 0.50, 1.0]
            };

            emit_btn_icon(
                max_icon,
                btn_x,
                12.0,
                1.3,
                icon_color,
                vector_atlas,
                &mut prims,
            );
            curr_right = btn_x;
        }

        // Minimize button
        if self.show_minimize {
            let btn_x = curr_right - Self::BUTTON_WIDTH;
            let is_hover = self.hovered_control == Some(TitlebarControl::Minimize);
            if is_hover {
                let pad_x = 4.0;
                let pad_y = 5.0;
                prims.push(RenderPrimitive::solid_rect(
                    [
                        (btn_x + pad_x) * scale,
                        (rect.y + pad_y) * scale,
                        (Self::BUTTON_WIDTH - pad_x * 2.0) * scale,
                        (rect.height - pad_y * 2.0) * scale,
                    ],
                    [1.0, 1.0, 1.0, 0.08],
                    CornerRadii::all(4.0 * scale),
                ));
            }

            let icon_color = if is_active {
                [0.85, 0.85, 0.85, 1.0]
            } else {
                [0.50, 0.50, 0.50, 1.0]
            };

            emit_btn_icon(
                IconName::WindowMinimize,
                btn_x,
                12.0,
                1.5,
                icon_color,
                vector_atlas,
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
    fn test_titlebar_hit_test_and_actions() {
        let mut titlebar = Titlebar::new("Test Window");
        let rect = LayoutRect::new(0.0, 0.0, 800.0, 38.0);

        // Control positions
        // Close: 800 - 44 = 756..800
        // Maximize: 756 - 44 = 712..756
        // Minimize: 712 - 44 = 668..712

        assert_eq!(titlebar.control_at(rect, 770.0, 19.0), Some(TitlebarControl::Close));
        assert_eq!(titlebar.control_at(rect, 730.0, 19.0), Some(TitlebarControl::Maximize));
        assert_eq!(titlebar.control_at(rect, 680.0, 19.0), Some(TitlebarControl::Minimize));
        assert_eq!(titlebar.control_at(rect, 200.0, 19.0), None);

        // Click close
        let down_evt = PointerEvent::Down { x: 770.0, y: 19.0, button: MouseButton::Left };
        titlebar.on_pointer_event(&down_evt, rect);
        assert_eq!(titlebar.pressed_control, Some(TitlebarControl::Close));

        let up_evt = PointerEvent::Up { x: 770.0, y: 19.0, button: MouseButton::Left };
        titlebar.on_pointer_event(&up_evt, rect);
        assert_eq!(titlebar.take_window_action(), Some(WindowAction::Close));

        // Click minimize
        titlebar.on_pointer_event(&PointerEvent::Down { x: 680.0, y: 19.0, button: MouseButton::Left }, rect);
        titlebar.on_pointer_event(&PointerEvent::Up { x: 680.0, y: 19.0, button: MouseButton::Left }, rect);
        assert_eq!(titlebar.take_window_action(), Some(WindowAction::Minimize));

        // Click maximize
        titlebar.on_pointer_event(&PointerEvent::Down { x: 730.0, y: 19.0, button: MouseButton::Left }, rect);
        titlebar.on_pointer_event(&PointerEvent::Up { x: 730.0, y: 19.0, button: MouseButton::Left }, rect);
        assert_eq!(titlebar.take_window_action(), Some(WindowAction::ToggleMaximize));
    }

    #[test]
    fn test_titlebar_drag_and_double_click_toggle_maximize() {
        let mut titlebar = Titlebar::new("Test Window");
        let rect = LayoutRect::new(0.0, 0.0, 800.0, 38.0);

        // Single click on blank region -> InteractiveMove
        titlebar.on_pointer_event(&PointerEvent::Down { x: 300.0, y: 19.0, button: MouseButton::Left }, rect);
        assert_eq!(titlebar.take_window_action(), Some(WindowAction::InteractiveMove));

        // Double click on blank region within 300ms -> ToggleMaximize
        titlebar.last_click_instant = Some(Instant::now());
        titlebar.on_pointer_event(&PointerEvent::Down { x: 300.0, y: 19.0, button: MouseButton::Left }, rect);
        assert_eq!(titlebar.take_window_action(), Some(WindowAction::ToggleMaximize));
    }
}
