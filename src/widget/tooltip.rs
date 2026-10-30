//! Hover tooltip management and automated edge-snapped placement computation.

use crate::animation::{Easing, Tween};
use crate::event::WidgetId;
use crate::layout::LayoutRect;

/// Screen edge placement positioning rule for tooltips.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum TooltipPlacement {
    /// Automatically selects Top or Bottom based on available viewport space.
    #[default]
    Auto,
    /// Centers above target widget.
    Top,
    /// Centers below target widget.
    Bottom,
    /// Aligns to the left of the target widget.
    Left,
    /// Aligns to the right of the target widget.
    Right,
}

/// Declarative tooltip specification for hover popovers.
#[derive(Debug, Clone, PartialEq)]
pub struct Tooltip {
    /// Text content displayed inside the tooltip bubble.
    pub text: String,
    /// Preferred placement edge.
    pub placement: TooltipPlacement,
    /// Hover duration delay before initiating the fade-in animation, in milliseconds.
    pub delay_ms: f32,
}

impl Tooltip {
    /// Creates a tooltip with default auto placement and 500ms hover delay.
    pub fn new<S: Into<String>>(text: S) -> Self {
        Self {
            text: text.into(),
            placement: TooltipPlacement::Auto,
            delay_ms: 500.0,
        }
    }

    /// Sets the preferred placement anchor edge.
    pub fn with_placement(mut self, placement: TooltipPlacement) -> Self {
        self.placement = placement;
        self
    }

    /// Sets the hover delay before the tooltip appears.
    pub fn with_delay(mut self, delay_ms: f32) -> Self {
        self.delay_ms = delay_ms;
        self
    }
}

/// Lifecycle states of a tooltip presentation.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TooltipPhase {
    /// No tooltip is active or pending.
    Inactive,
    /// Pointer is hovering over a target widget, waiting for delay timer to elapse.
    Waiting,
    /// Opacity fade-in animation is running.
    FadingIn,
    /// Tooltip is fully visible.
    Visible,
    /// Opacity fade-out animation is running after pointer leaves target.
    FadingOut,
}

/// Global coordinator tracking active tooltips, hover timers, and opacity transitions.
pub struct TooltipManager {
    /// ID of the widget currently receiving pointer hover.
    pub current_target: Option<WidgetId>,
    /// Active tooltip descriptor being displayed or waited on.
    pub active_tooltip: Option<Tooltip>,
    /// Layout rectangle of the target widget.
    pub target_rect: LayoutRect,
    /// Milliseconds elapsed hovering over the current target.
    pub hover_timer_ms: f32,
    /// Current lifecycle phase.
    pub phase: TooltipPhase,
    /// Opacity animation tween between 0.0 and 1.0.
    pub alpha_tween: Tween<f32>,
}

impl Default for TooltipManager {
    fn default() -> Self {
        Self::new()
    }
}

impl TooltipManager {
    /// Default delay before opening a tooltip in milliseconds.
    pub const DEFAULT_DELAY_MS: f32 = 500.0;
    /// Duration of fade-in and fade-out animations in milliseconds.
    pub const FADE_DURATION_MS: f32 = 150.0;

    /// Creates a new inactive tooltip manager.
    pub fn new() -> Self {
        Self {
            current_target: None,
            active_tooltip: None,
            target_rect: LayoutRect::ZERO,
            hover_timer_ms: 0.0,
            phase: TooltipPhase::Inactive,
            alpha_tween: Tween::new(0.0),
        }
    }

    /// Notifies the manager of a change in pointer hover target.
    pub fn handle_hover_change(
        &mut self,
        new_target: Option<WidgetId>,
        target_rect: LayoutRect,
        tooltip: Option<Tooltip>,
    ) {
        if self.current_target == new_target {
            self.target_rect = target_rect;
            return;
        }

        self.current_target = new_target;
        self.target_rect = target_rect;
        self.hover_timer_ms = 0.0;

        if let Some(tt) = tooltip {
            self.active_tooltip = Some(tt);
            self.phase = TooltipPhase::Waiting;
            self.alpha_tween = Tween::new(0.0);
        } else if self.phase == TooltipPhase::FadingIn || self.phase == TooltipPhase::Visible {
            self.phase = TooltipPhase::FadingOut;
            self.alpha_tween.set_target(0.0, Self::FADE_DURATION_MS, Easing::EaseOut);
        } else {
            self.phase = TooltipPhase::Inactive;
            self.active_tooltip = None;
            self.alpha_tween = Tween::new(0.0);
        }
    }

    /// Immediately cancels any active or pending tooltip on pointer press.
    pub fn handle_pointer_down(&mut self) {
        self.phase = TooltipPhase::Inactive;
        self.active_tooltip = None;
        self.hover_timer_ms = 0.0;
        self.alpha_tween = Tween::new(0.0);
    }

    /// Advances hover delay timers and opacity animation tweens.
    pub fn tick(&mut self, delta_ms: f32) -> bool {
        match self.phase {
            TooltipPhase::Inactive => false,
            TooltipPhase::Waiting => {
                let delay = self
                    .active_tooltip
                    .as_ref()
                    .map(|t| t.delay_ms)
                    .unwrap_or(Self::DEFAULT_DELAY_MS);
                self.hover_timer_ms += delta_ms;
                if self.hover_timer_ms >= delay {
                    self.phase = TooltipPhase::FadingIn;
                    self.alpha_tween.set_target(1.0, Self::FADE_DURATION_MS, Easing::EaseOut);
                }
                true
            }
            TooltipPhase::FadingIn => {
                self.alpha_tween.tick(delta_ms);
                if self.alpha_tween.is_complete() {
                    self.phase = TooltipPhase::Visible;
                }
                true
            }
            TooltipPhase::Visible => false,
            TooltipPhase::FadingOut => {
                self.alpha_tween.tick(delta_ms);
                if self.alpha_tween.is_complete() {
                    self.phase = TooltipPhase::Inactive;
                    self.active_tooltip = None;
                }
                true
            }
        }
    }

    /// Returns `true` if the tooltip has non-zero opacity and is rendering.
    pub fn is_visible(&self) -> bool {
        self.alpha_tween.value() > 0.001 && self.active_tooltip.is_some()
    }

    /// Returns current opacity multiplier between 0.0 and 1.0.
    pub fn current_alpha(&self) -> f32 {
        self.alpha_tween.value()
    }

    /// Calculates clamped screen-space placement bounds avoiding window edge overflow.
    pub fn calculate_bounds(
        &self,
        box_width: f32,
        box_height: f32,
        window_width: f32,
        window_height: f32,
    ) -> LayoutRect {
        let placement = self
            .active_tooltip
            .as_ref()
            .map(|t| t.placement)
            .unwrap_or(TooltipPlacement::Auto);

        let gap = 6.0;
        let pad_edge = 6.0;

        let (mut x, mut y) = match placement {
            TooltipPlacement::Top => (
                self.target_rect.x + (self.target_rect.width - box_width) * 0.5,
                self.target_rect.y - box_height - gap,
            ),
            TooltipPlacement::Bottom => (
                self.target_rect.x + (self.target_rect.width - box_width) * 0.5,
                self.target_rect.y + self.target_rect.height + gap,
            ),
            TooltipPlacement::Left => (
                self.target_rect.x - box_width - gap,
                self.target_rect.y + (self.target_rect.height - box_height) * 0.5,
            ),
            TooltipPlacement::Right => (
                self.target_rect.x + self.target_rect.width + gap,
                self.target_rect.y + (self.target_rect.height - box_height) * 0.5,
            ),
            TooltipPlacement::Auto => {
                let top_y = self.target_rect.y - box_height - gap;
                if top_y < pad_edge {
                    (
                        self.target_rect.x + (self.target_rect.width - box_width) * 0.5,
                        self.target_rect.y + self.target_rect.height + gap,
                    )
                } else {
                    (
                        self.target_rect.x + (self.target_rect.width - box_width) * 0.5,
                        top_y,
                    )
                }
            }
        };

        let max_x = (window_width - box_width - pad_edge).max(pad_edge);
        let max_y = (window_height - box_height - pad_edge).max(pad_edge);
        x = x.clamp(pad_edge, max_x);
        y = y.clamp(pad_edge, max_y);

        LayoutRect::new(x, y, box_width, box_height)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_tooltip_manager_lifecycle() {
        let mut mgr = TooltipManager::new();
        let target = WidgetId(42);
        let rect = LayoutRect::new(100.0, 100.0, 80.0, 32.0);
        let tooltip = Tooltip::new("Helpful tip").with_delay(200.0);

        mgr.handle_hover_change(Some(target), rect, Some(tooltip));
        assert_eq!(mgr.phase, TooltipPhase::Waiting);
        assert_eq!(mgr.current_alpha(), 0.0);

        // Tick 100ms: still waiting
        let animating = mgr.tick(100.0);
        assert!(animating);
        assert_eq!(mgr.phase, TooltipPhase::Waiting);

        // Tick another 100ms: delay reached, transitions to FadingIn
        let animating = mgr.tick(100.0);
        assert!(animating);
        assert_eq!(mgr.phase, TooltipPhase::FadingIn);

        // Tick fade in to completion
        mgr.tick(150.0);
        assert_eq!(mgr.phase, TooltipPhase::Visible);
        assert!((mgr.current_alpha() - 1.0).abs() < 1e-4);
        assert!(mgr.is_visible());

        // Hover off: transitions to FadingOut
        mgr.handle_hover_change(None, LayoutRect::ZERO, None);
        assert_eq!(mgr.phase, TooltipPhase::FadingOut);

        // Tick fade out to completion
        mgr.tick(150.0);
        assert_eq!(mgr.phase, TooltipPhase::Inactive);
        assert!(!mgr.is_visible());
    }

    #[test]
    fn test_tooltip_click_dismissal() {
        let mut mgr = TooltipManager::new();
        let target = WidgetId(10);
        let rect = LayoutRect::new(50.0, 50.0, 60.0, 30.0);
        let tooltip = Tooltip::new("Click me");

        mgr.handle_hover_change(Some(target), rect, Some(tooltip));
        mgr.tick(500.0);
        assert_eq!(mgr.phase, TooltipPhase::FadingIn);
        mgr.tick(150.0);
        assert_eq!(mgr.phase, TooltipPhase::Visible);

        mgr.handle_pointer_down();
        assert_eq!(mgr.phase, TooltipPhase::Inactive);
        assert!(!mgr.is_visible());
    }

    #[test]
    fn test_tooltip_edge_flipping() {
        let mut mgr = TooltipManager::new();
        let target = WidgetId(1);
        // Widget close to ceiling: y = 10.0
        let rect_near_top = LayoutRect::new(100.0, 10.0, 80.0, 30.0);
        let tooltip = Tooltip::new("Top target").with_placement(TooltipPlacement::Auto);
        mgr.handle_hover_change(Some(target), rect_near_top, Some(tooltip));

        let bounds = mgr.calculate_bounds(60.0, 24.0, 800.0, 600.0);
        // Preferred top would be y = 10.0 - 24.0 - 6.0 = -20.0 < 6.0, so flips to bottom
        assert!(bounds.y >= rect_near_top.y + rect_near_top.height);

        // Widget with plenty of headroom: y = 200.0
        let rect_middle = LayoutRect::new(100.0, 200.0, 80.0, 30.0);
        mgr.handle_hover_change(Some(target), rect_middle, Some(Tooltip::new("Mid target")));
        let bounds_mid = mgr.calculate_bounds(60.0, 24.0, 800.0, 600.0);
        assert!(bounds_mid.y < rect_middle.y);
    }
}
