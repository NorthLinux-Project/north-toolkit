//! Retained-mode toast notification overlay system, auto-dismiss timers, and thread-safe handle.

use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};

use super::icon::IconName;
use crate::animation::{Easing, Tween};
use crate::event::{CursorIcon, EventResponse, MouseButton, PointerEvent};
use crate::layout::LayoutRect;
use crate::render::vector::{StrokeStyle, VectorAtlas, VectorPath, VectorRasterizer};
use crate::render::{Border, CornerRadii, GlyphAtlas, RenderPrimitive};
use crate::text::{emit_text_glyphs, GlyphRasterizer, TextShaper};
use crate::theme::Theme;

static NEXT_TOAST_ID: AtomicU64 = AtomicU64::new(1);

/// Severity level for a toast notification card.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum ToastVariant {
    /// General informational notice.
    #[default]
    Info,
    /// Confirmation of a successful operation.
    Success,
    /// Alert for a potentially unsafe or unexpected condition.
    Warning,
    /// Error notification for a failed operation.
    Error,
}

impl ToastVariant {
    /// Returns the primary accent RGBA color for this toast variant.
    pub fn accent_color(&self, theme: &Theme) -> [f32; 4] {
        match *self {
            ToastVariant::Info => theme.colors.primary,
            ToastVariant::Success => [0.18, 0.80, 0.44, 1.0],
            ToastVariant::Warning => [0.96, 0.65, 0.14, 1.0],
            ToastVariant::Error => [0.92, 0.26, 0.21, 1.0],
        }
    }

    /// Returns the optional icon associated with this variant.
    pub fn icon(&self) -> Option<IconName> {
        match *self {
            ToastVariant::Info => None,
            ToastVariant::Success => Some(IconName::Check),
            ToastVariant::Warning => None,
            ToastVariant::Error => Some(IconName::Close),
        }
    }
}

/// Screen corner anchor position for stacking toasts.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum ToastPosition {
    /// Stack toasts from the top-right corner downward.
    #[default]
    TopRight,
    /// Stack toasts from the top-left corner downward.
    TopLeft,
    /// Stack toasts from the bottom-right corner upward.
    BottomRight,
    /// Stack toasts from the bottom-left corner upward.
    BottomLeft,
}

/// Lifecycle progression states of an individual toast.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ToastPhase {
    /// Slide-in and opacity fade-in transition.
    Entering,
    /// Fully visible and active.
    Visible,
    /// Slide-out and opacity fade-out transition.
    Exiting,
    /// Completely transitioned out and ready for removal.
    Dismissed,
}

/// Retained toast notification card.
pub struct Toast {
    /// Unique identifier for this toast instance.
    pub id: u64,
    /// Primary title heading text.
    pub title: String,
    /// Optional supplementary body message.
    pub body: Option<String>,
    /// Severity variant.
    pub variant: ToastVariant,
    /// Time until auto-dismiss in milliseconds, or `None` if persistent.
    pub duration_ms: Option<f32>,
    /// Whether user can manually dismiss via close button.
    pub closable: bool,
    /// Optional action button label.
    pub action_label: Option<String>,
    /// Optional action button click handler.
    pub on_action: Option<Box<dyn FnMut() + Send + Sync>>,
    /// Elapsed display time in milliseconds.
    pub elapsed_ms: f32,
    /// Current presentation phase.
    pub phase: ToastPhase,
    enter_tween: Tween<f32>,
    exit_tween: Tween<f32>,
}

impl Toast {
    /// Default toast display duration before auto-dismissal (4000 milliseconds).
    pub const DEFAULT_DURATION_MS: f32 = 4000.0;
    /// Duration of enter slide/fade animation in milliseconds.
    pub const ENTER_DURATION_MS: f32 = 200.0;
    /// Duration of exit slide/fade animation in milliseconds.
    pub const EXIT_DURATION_MS: f32 = 180.0;

    /// Creates a new toast notification with the given title and variant.
    pub fn new<S: Into<String>>(title: S, variant: ToastVariant) -> Self {
        let id = NEXT_TOAST_ID.fetch_add(1, Ordering::Relaxed);
        let mut enter_tween = Tween::new(0.0);
        enter_tween.set_target(1.0, Self::ENTER_DURATION_MS, Easing::EaseOut);

        Self {
            id,
            title: title.into(),
            body: None,
            variant,
            duration_ms: Some(Self::DEFAULT_DURATION_MS),
            closable: true,
            action_label: None,
            on_action: None,
            elapsed_ms: 0.0,
            phase: ToastPhase::Entering,
            enter_tween,
            exit_tween: Tween::new(1.0),
        }
    }

    /// Convenience builder for an `Info` toast.
    pub fn info<S: Into<String>>(title: S) -> Self {
        Self::new(title, ToastVariant::Info)
    }

    /// Convenience builder for a `Success` toast.
    pub fn success<S: Into<String>>(title: S) -> Self {
        Self::new(title, ToastVariant::Success)
    }

    /// Convenience builder for a `Warning` toast.
    pub fn warning<S: Into<String>>(title: S) -> Self {
        Self::new(title, ToastVariant::Warning)
    }

    /// Convenience builder for an `Error` toast.
    pub fn error<S: Into<String>>(title: S) -> Self {
        Self::new(title, ToastVariant::Error)
    }

    /// Attaches supplementary body message text.
    pub fn with_body<S: Into<String>>(mut self, body: S) -> Self {
        self.body = Some(body.into());
        self
    }

    /// Sets custom auto-dismiss duration in milliseconds.
    pub fn with_duration(mut self, duration_ms: f32) -> Self {
        self.duration_ms = Some(duration_ms.max(1.0));
        self
    }

    /// Configures this toast to remain visible until manually dismissed.
    pub fn with_persistent(mut self) -> Self {
        self.duration_ms = None;
        self
    }

    /// Configures whether this toast displays a close button.
    pub fn with_closable(mut self, closable: bool) -> Self {
        self.closable = closable;
        self
    }

    /// Attaches an interactive action button with callback.
    pub fn with_action<S: Into<String>, F: FnMut() + Send + Sync + 'static>(
        mut self,
        label: S,
        cb: F,
    ) -> Self {
        self.action_label = Some(label.into());
        self.on_action = Some(Box::new(cb));
        self
    }

    /// Initiates dismiss exit animation.
    pub fn dismiss(&mut self) {
        if self.phase != ToastPhase::Exiting && self.phase != ToastPhase::Dismissed {
            self.phase = ToastPhase::Exiting;
            self.exit_tween.set_target(0.0, Self::EXIT_DURATION_MS, Easing::EaseIn);
        }
    }

    /// Returns current opacity factor in range `[0.0, 1.0]`.
    pub fn current_alpha(&self) -> f32 {
        match self.phase {
            ToastPhase::Entering => self.enter_tween.current.clamp(0.0, 1.0),
            ToastPhase::Visible => 1.0,
            ToastPhase::Exiting => self.exit_tween.current.clamp(0.0, 1.0),
            ToastPhase::Dismissed => 0.0,
        }
    }

    /// Returns horizontal slide offset in logical points.
    pub fn current_slide_offset(&self) -> f32 {
        match self.phase {
            ToastPhase::Entering => (1.0 - self.enter_tween.current) * 60.0,
            ToastPhase::Visible => 0.0,
            ToastPhase::Exiting => (1.0 - self.exit_tween.current) * 60.0,
            ToastPhase::Dismissed => 60.0,
        }
    }
}

/// Thread-safe, cloneable handle for enqueuing toast notifications from any widget or thread.
#[derive(Clone, Default)]
pub struct ToastHandle {
    pending: Arc<Mutex<Vec<Toast>>>,
}

impl ToastHandle {
    /// Creates a new empty toast handle.
    pub fn new() -> Self {
        Self {
            pending: Arc::new(Mutex::new(Vec::new())),
        }
    }

    /// Enqueues a pre-configured toast notification.
    pub fn push(&self, toast: Toast) {
        if let Ok(mut lock) = self.pending.lock() {
            lock.push(toast);
        }
    }

    /// Enqueues an informational notification.
    pub fn info<S: Into<String>>(&self, title: S) {
        self.push(Toast::info(title));
    }

    /// Enqueues a success notification.
    pub fn success<S: Into<String>>(&self, title: S) {
        self.push(Toast::success(title));
    }

    /// Enqueues a warning notification.
    pub fn warning<S: Into<String>>(&self, title: S) {
        self.push(Toast::warning(title));
    }

    /// Enqueues an error notification.
    pub fn error<S: Into<String>>(&self, title: S) {
        self.push(Toast::error(title));
    }

    /// Takes all queued toasts, clearing the pending buffer.
    pub fn take(&self) -> Vec<Toast> {
        if let Ok(mut lock) = self.pending.lock() {
            std::mem::take(&mut *lock)
        } else {
            Vec::new()
        }
    }
}

/// Retained overlay manager stored in `WidgetTree` coordinating toast layout, timing, and rendering.
pub struct ToastManager {
    /// Active toast cards currently displayed or exiting.
    pub toasts: Vec<Toast>,
    /// Screen corner position anchor.
    pub position: ToastPosition,
    /// Maximum number of visible toasts before oldest are dismissed.
    pub max_toasts: usize,
    /// ID of the toast currently receiving pointer hover.
    pub hovered_toast: Option<u64>,
    /// ID of the toast whose close button is currently hovered.
    pub hovered_close: Option<u64>,
    /// ID of the toast whose action button is currently hovered.
    pub hovered_action: Option<u64>,
}

impl Default for ToastManager {
    fn default() -> Self {
        Self::new()
    }
}

impl ToastManager {
    /// Default maximum simultaneous toasts displayed.
    pub const DEFAULT_MAX_TOASTS: usize = 5;
    /// Width of toast cards in logical points.
    pub const CARD_WIDTH: f32 = 320.0;
    /// Gap between stacked cards in logical points.
    pub const CARD_GAP: f32 = 10.0;
    /// Screen margin offset in logical points.
    pub const SCREEN_MARGIN: f32 = 16.0;

    /// Creates a new empty toast manager anchored at `TopRight`.
    pub fn new() -> Self {
        Self {
            toasts: Vec::new(),
            position: ToastPosition::TopRight,
            max_toasts: Self::DEFAULT_MAX_TOASTS,
            hovered_toast: None,
            hovered_close: None,
            hovered_action: None,
        }
    }

    /// Sets screen corner anchor position.
    pub fn with_position(mut self, pos: ToastPosition) -> Self {
        self.position = pos;
        self
    }

    /// Sets maximum simultaneous toasts.
    pub fn with_max_toasts(mut self, max: usize) -> Self {
        self.max_toasts = max.max(1);
        self
    }

    /// Enqueues a toast card for presentation.
    pub fn push(&mut self, toast: Toast) {
        self.toasts.retain(|t| t.phase != ToastPhase::Dismissed);
        let active_count = self
            .toasts
            .iter()
            .filter(|t| t.phase == ToastPhase::Visible || t.phase == ToastPhase::Entering)
            .count();
        if active_count >= self.max_toasts {
            if let Some(oldest) = self
                .toasts
                .iter_mut()
                .find(|t| t.phase == ToastPhase::Visible || t.phase == ToastPhase::Entering)
            {
                oldest.dismiss();
            }
        }
        while self.toasts.len() >= self.max_toasts * 2 {
            if let Some(pos) = self.toasts.iter().position(|t| t.phase == ToastPhase::Exiting) {
                self.toasts.remove(pos);
            } else {
                self.toasts.remove(0);
            }
        }
        self.toasts.push(toast);
    }

    /// Manually dismisses a toast by its unique ID.
    pub fn dismiss(&mut self, id: u64) {
        if let Some(t) = self.toasts.iter_mut().find(|t| t.id == id) {
            t.dismiss();
        }
    }

    /// Immediately clears all active toasts.
    pub fn clear(&mut self) {
        self.toasts.clear();
        self.hovered_toast = None;
        self.hovered_close = None;
        self.hovered_action = None;
    }

    /// Returns `true` if there are no active toasts.
    pub fn is_empty(&self) -> bool {
        self.toasts.is_empty()
    }

    /// Computes layout rectangles for each active toast, including close and action buttons.
    pub fn toast_rects(
        &self,
        win_w: f32,
        win_h: f32,
    ) -> Vec<(u64, LayoutRect, Option<LayoutRect>, Option<LayoutRect>)> {
        let mut rects = Vec::with_capacity(self.toasts.len());
        let card_w = Self::CARD_WIDTH;
        let margin = Self::SCREEN_MARGIN;
        let gap = Self::CARD_GAP;

        let base_x = match self.position {
            ToastPosition::TopRight | ToastPosition::BottomRight => win_w - margin - card_w,
            ToastPosition::TopLeft | ToastPosition::BottomLeft => margin,
        };

        let mut curr_y = match self.position {
            ToastPosition::TopRight | ToastPosition::TopLeft => margin,
            ToastPosition::BottomRight | ToastPosition::BottomLeft => win_h - margin,
        };

        for toast in &self.toasts {
            let card_h = if toast.body.is_some() && toast.action_label.is_some() {
                74.0
            } else if toast.body.is_some() || toast.action_label.is_some() {
                64.0
            } else {
                48.0
            };

            let card_y = match self.position {
                ToastPosition::TopRight | ToastPosition::TopLeft => {
                    let y = curr_y;
                    if y + card_h > win_h - margin {
                        break;
                    }
                    curr_y += card_h + gap;
                    y
                }
                ToastPosition::BottomRight | ToastPosition::BottomLeft => {
                    curr_y -= card_h;
                    if curr_y < margin {
                        break;
                    }
                    let y = curr_y;
                    curr_y -= gap;
                    y
                }
            };

            let slide_dir = match self.position {
                ToastPosition::TopRight | ToastPosition::BottomRight => 1.0,
                ToastPosition::TopLeft | ToastPosition::BottomLeft => -1.0,
            };
            let slide_x = base_x + toast.current_slide_offset() * slide_dir;
            let card_rect = LayoutRect::new(slide_x, card_y, card_w, card_h);

            // Close button
            let close_rect = if toast.closable {
                let size = 18.0;
                let cx = card_rect.x + card_rect.width - size - 8.0;
                let cy = card_rect.y + 8.0;
                Some(LayoutRect::new(cx, cy, size, size))
            } else {
                None
            };

            // Action button
            let action_rect = if toast.action_label.is_some() {
                let btn_w = 64.0;
                let btn_h = 24.0;
                let ax = card_rect.x + card_rect.width - btn_w - (if toast.closable { 32.0 } else { 12.0 });
                let ay = card_rect.y + card_rect.height - btn_h - 8.0;
                Some(LayoutRect::new(ax, ay, btn_w, btn_h))
            } else {
                None
            };

            rects.push((toast.id, card_rect, close_rect, action_rect));
        }

        rects
    }

    /// Advances toast timers and slide/fade transitions. Returns `true` if repaint is required.
    pub fn tick(&mut self, delta_ms: f32) -> bool {
        let mut needs_repaint = false;

        for toast in &mut self.toasts {
            match toast.phase {
                ToastPhase::Entering => {
                    toast.enter_tween.tick(delta_ms);
                    needs_repaint = true;
                    if toast.enter_tween.is_complete() {
                        toast.phase = ToastPhase::Visible;
                    }
                }
                ToastPhase::Visible => {
                    // Hovering pauses auto-dismiss countdown
                    if self.hovered_toast != Some(toast.id) {
                        if let Some(dur) = toast.duration_ms {
                            toast.elapsed_ms += delta_ms;
                            if toast.elapsed_ms >= dur {
                                toast.dismiss();
                                needs_repaint = true;
                            }
                        }
                    }
                }
                ToastPhase::Exiting => {
                    toast.exit_tween.tick(delta_ms);
                    needs_repaint = true;
                    if toast.exit_tween.is_complete() {
                        toast.phase = ToastPhase::Dismissed;
                    }
                }
                ToastPhase::Dismissed => {}
            }
        }

        let prev_len = self.toasts.len();
        self.toasts.retain(|t| t.phase != ToastPhase::Dismissed);
        if self.toasts.len() != prev_len {
            needs_repaint = true;
        }

        needs_repaint
    }

    /// Dispatches pointer events to toast cards, close buttons, and actions.
    pub fn dispatch_pointer_event(
        &mut self,
        event: &PointerEvent,
        win_w: f32,
        win_h: f32,
    ) -> EventResponse {
        let rects = self.toast_rects(win_w, win_h);
        let (px, py) = event.position();

        match *event {
            PointerEvent::Move { x, y } => {
                let mut hit_toast = None;
                let mut hit_close = None;
                let mut hit_action = None;

                for &(id, card_r, close_opt, action_opt) in &rects {
                    if let Some(close_r) = close_opt {
                        if close_r.contains(x, y) {
                            hit_close = Some(id);
                        }
                    }
                    if let Some(act_r) = action_opt {
                        if act_r.contains(x, y) {
                            hit_action = Some(id);
                        }
                    }
                    if card_r.contains(x, y) {
                        hit_toast = Some(id);
                        break;
                    }
                }

                if hit_toast != self.hovered_toast
                    || hit_close != self.hovered_close
                    || hit_action != self.hovered_action
                {
                    self.hovered_toast = hit_toast;
                    self.hovered_close = hit_close;
                    self.hovered_action = hit_action;
                    return EventResponse::Handled;
                }

                if hit_toast.is_some() {
                    return EventResponse::Handled;
                }
            }
            PointerEvent::Down { button, .. } => {
                if button == MouseButton::Left {
                    for &(id, card_r, close_opt, action_opt) in &rects {
                        if let Some(close_r) = close_opt {
                            if close_r.contains(px, py) {
                                self.dismiss(id);
                                return EventResponse::Handled;
                            }
                        }
                        if let Some(act_r) = action_opt {
                            if act_r.contains(px, py) {
                                if let Some(toast) = self.toasts.iter_mut().find(|t| t.id == id) {
                                    if let Some(cb) = &mut toast.on_action {
                                        cb();
                                    }
                                    toast.dismiss();
                                }
                                return EventResponse::Handled;
                            }
                        }
                        if card_r.contains(px, py) {
                            return EventResponse::Handled;
                        }
                    }
                }
            }
            PointerEvent::Up { button, .. } => {
                if button == MouseButton::Left {
                    for &(_id, card_r, _, _) in &rects {
                        if card_r.contains(px, py) {
                            return EventResponse::Handled;
                        }
                    }
                }
            }
            _ => {}
        }

        EventResponse::Ignored
    }

    /// Queries the cursor icon if hovering over toast elements.
    pub fn cursor_icon_at(&self, x: f32, y: f32, win_w: f32, win_h: f32) -> Option<CursorIcon> {
        let rects = self.toast_rects(win_w, win_h);
        for &(_id, card_r, close_opt, action_opt) in &rects {
            if let Some(close_r) = close_opt {
                if close_r.contains(x, y) {
                    return Some(CursorIcon::Pointer);
                }
            }
            if let Some(act_r) = action_opt {
                if act_r.contains(x, y) {
                    return Some(CursorIcon::Pointer);
                }
            }
            if card_r.contains(x, y) {
                return Some(CursorIcon::Default);
            }
        }
        None
    }

    /// Emits visual primitives rendering the stacked toast notification overlay.
    pub fn paint(
        &self,
        win_w: f32,
        win_h: f32,
        scale: f32,
        theme: &Theme,
        atlas: &mut GlyphAtlas,
        rasterizer: &GlyphRasterizer,
        shaper: &TextShaper,
        vector_atlas: &mut VectorAtlas,
    ) -> Vec<RenderPrimitive> {
        let mut prims = Vec::new();
        let rects = self.toast_rects(win_w, win_h);

        for &(id, card_r, close_opt, action_opt) in &rects {
            let toast = match self.toasts.iter().find(|t| t.id == id) {
                Some(t) => t,
                None => continue,
            };

            let alpha = toast.current_alpha();
            if alpha <= 0.005 {
                continue;
            }

            let accent_col = toast.variant.accent_color(theme);
            let mut card_bg = [0.12, 0.12, 0.12, 0.95 * alpha];
            let mut border_col = [0.25, 0.25, 0.25, 0.9 * alpha];

            if self.hovered_toast == Some(id) {
                card_bg = [0.16, 0.16, 0.16, 0.98 * alpha];
                border_col = [0.35, 0.35, 0.35, alpha];
            }

            // Toast card container
            prims.push(RenderPrimitive::bordered_rect(
                [
                    card_r.x * scale,
                    card_r.y * scale,
                    card_r.width * scale,
                    card_r.height * scale,
                ],
                card_bg,
                CornerRadii::all(theme.border_radius_md * scale),
                Border::new(border_col, 1.0 * scale),
            ));

            // Scissor clip children within card boundaries
            prims.push(RenderPrimitive::ClipPush {
                rect: [
                    card_r.x * scale,
                    card_r.y * scale,
                    card_r.width * scale,
                    card_r.height * scale,
                ],
            });

            // Accent indicator bar along left edge
            let bar_w = 4.0;
            let mut bar_col = accent_col;
            bar_col[3] *= alpha;
            prims.push(RenderPrimitive::solid_rect(
                [
                    card_r.x * scale,
                    card_r.y * scale,
                    bar_w * scale,
                    card_r.height * scale,
                ],
                bar_col,
                CornerRadii::new(
                    theme.border_radius_md * scale,
                    0.0,
                    0.0,
                    theme.border_radius_md * scale,
                ),
            ));

            let mut text_x = card_r.x + 14.0;

            // Optional variant icon
            if let Some(icon) = toast.variant.icon() {
                let icon_size = 14.0 * scale;
                let phys_w = (icon_size.round() as u32).max(1);
                let phys_h = phys_w;
                let uv = vector_atlas.get_or_insert(icon as u64, phys_w, phys_h, || {
                    let path = VectorPath::parse_svg(icon.svg_path())
                        .with_view_box(0.0, 0.0, 24.0, 24.0);
                    VectorRasterizer::rasterize(
                        &path,
                        phys_w,
                        phys_h,
                        None,
                        Some(StrokeStyle {
                            color: [1.0, 1.0, 1.0, 1.0],
                            width: 2.0,
                            ..Default::default()
                        }),
                    )
                });

                if let Some(uv_rect) = uv {
                    let iy = card_r.y + 12.0;
                    let mut ic = accent_col;
                    ic[3] *= alpha;
                    prims.push(RenderPrimitive::Rect {
                        rect: [text_x * scale, iy * scale, icon_size, icon_size],
                        color: ic,
                        radii: CornerRadii::all(0.0),
                        border: None,
                        uv_rect,
                        use_texture: true,
                    });
                }
                text_x += 18.0;
            }

            // Title text
            let title_size = 13.0;
            let shaped_title = shaper.shape(&toast.title, title_size);
            let title_y = card_r.y + 11.0;
            let title_base = title_y + shaped_title.height * 0.75;
            let mut title_col = theme.colors.text;
            title_col[3] *= alpha;

            emit_text_glyphs(
                &shaped_title,
                text_x,
                title_base,
                title_size,
                title_col,
                scale,
                atlas,
                rasterizer,
                &mut prims,
            );

            // Optional body text
            if let Some(body) = &toast.body {
                let body_size = 11.0;
                let shaped_body = shaper.shape(body, body_size);
                let body_y = title_y + shaped_title.height + 4.0;
                let body_base = body_y + shaped_body.height * 0.75;
                let mut body_col = theme.colors.text_secondary;
                body_col[3] *= alpha;

                emit_text_glyphs(
                    &shaped_body,
                    text_x,
                    body_base,
                    body_size,
                    body_col,
                    scale,
                    atlas,
                    rasterizer,
                    &mut prims,
                );
            }

            // Close button
            if let Some(close_r) = close_opt {
                let is_close_hovered = self.hovered_close == Some(id);
                if is_close_hovered {
                    prims.push(RenderPrimitive::solid_rect(
                        [
                            close_r.x * scale,
                            close_r.y * scale,
                            close_r.width * scale,
                            close_r.height * scale,
                        ],
                        [1.0, 0.2, 0.2, 0.25 * alpha],
                        CornerRadii::all(close_r.width * 0.5 * scale),
                    ));
                }

                let icon_size = (close_r.width - 4.0) * scale;
                let phys_w = (icon_size.round() as u32).max(1);
                let phys_h = phys_w;
                let close_uv = vector_atlas.get_or_insert(
                    IconName::Close as u64,
                    phys_w,
                    phys_h,
                    || {
                        let path = VectorPath::parse_svg(IconName::Close.svg_path())
                            .with_view_box(0.0, 0.0, 24.0, 24.0);
                        VectorRasterizer::rasterize(
                            &path,
                            phys_w,
                            phys_h,
                            None,
                            Some(StrokeStyle {
                                color: [1.0, 1.0, 1.0, 1.0],
                                width: 2.0,
                                ..Default::default()
                            }),
                        )
                    },
                );

                if let Some(uv) = close_uv {
                    let mut close_col = if is_close_hovered {
                        [1.0, 0.3, 0.3, 1.0]
                    } else {
                        theme.colors.text_secondary
                    };
                    close_col[3] *= alpha;

                    prims.push(RenderPrimitive::Rect {
                        rect: [
                            (close_r.x + 2.0) * scale,
                            (close_r.y + 2.0) * scale,
                            icon_size,
                            icon_size,
                        ],
                        color: close_col,
                        radii: CornerRadii::all(0.0),
                        border: None,
                        uv_rect: uv,
                        use_texture: true,
                    });
                }
            }

            // Action button
            if let Some(act_r) = action_opt {
                if let Some(action_label) = &toast.action_label {
                    let is_act_hovered = self.hovered_action == Some(id);
                    let mut btn_bg = if is_act_hovered {
                        theme.colors.primary_hover
                    } else {
                        theme.colors.primary
                    };
                    btn_bg[3] *= alpha;

                    prims.push(RenderPrimitive::solid_rect(
                        [
                            act_r.x * scale,
                            act_r.y * scale,
                            act_r.width * scale,
                            act_r.height * scale,
                        ],
                        btn_bg,
                        CornerRadii::all(theme.border_radius_sm * scale),
                    ));

                    let act_size = 11.0;
                    let shaped_act = shaper.shape(action_label, act_size);
                    let act_x = act_r.x + (act_r.width - shaped_act.width) * 0.5;
                    let act_y = act_r.y + (act_r.height - shaped_act.height) * 0.5;
                    let act_base = act_y + shaped_act.height * 0.75;
                    let act_col = [1.0, 1.0, 1.0, alpha];

                    emit_text_glyphs(
                        &shaped_act,
                        act_x,
                        act_base,
                        act_size,
                        act_col,
                        scale,
                        atlas,
                        rasterizer,
                        &mut prims,
                    );
                }
            }

            prims.push(RenderPrimitive::ClipPop);
        }

        prims
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_toast_variants_and_accent_colors() {
        let theme = Theme::default();
        let info_toast = Toast::info("Information");
        assert_eq!(info_toast.variant, ToastVariant::Info);
        assert_eq!(info_toast.variant.accent_color(&theme), theme.colors.primary);

        let err_toast = Toast::error("Failure");
        assert_eq!(err_toast.variant, ToastVariant::Error);
        assert_eq!(err_toast.variant.accent_color(&theme), [0.92, 0.26, 0.21, 1.0]);
    }

    #[test]
    fn test_toast_manager_push_and_auto_dismiss() {
        let mut manager = ToastManager::new().with_max_toasts(3);
        assert!(manager.is_empty());

        let t1 = Toast::info("Task Started").with_duration(100.0);
        manager.push(t1);
        assert_eq!(manager.toasts.len(), 1);

        // Advance through enter phase
        manager.tick(250.0);
        assert_eq!(manager.toasts[0].phase, ToastPhase::Visible);

        // Advance through duration
        manager.tick(150.0);
        assert_eq!(manager.toasts[0].phase, ToastPhase::Exiting);

        // Advance through exit animation
        manager.tick(200.0);
        assert!(manager.is_empty());
    }

    #[test]
    fn test_toast_handle_thread_safe_queue() {
        let handle = ToastHandle::new();
        handle.info("Background complete");
        handle.success("File saved");

        let pending = handle.take();
        assert_eq!(pending.len(), 2);
        assert_eq!(pending[0].title, "Background complete");
        assert_eq!(pending[1].title, "File saved");

        assert!(handle.take().is_empty());
    }

    #[test]
    fn test_toast_manual_dismiss() {
        let mut manager = ToastManager::new();
        let toast = Toast::warning("Attention");
        let id = toast.id;
        manager.push(toast);

        manager.dismiss(id);
        assert_eq!(manager.toasts[0].phase, ToastPhase::Exiting);
    }

    #[test]
    fn test_toast_window_bounds_clamping() {
        let mut manager = ToastManager::new().with_max_toasts(10);
        for i in 0..10 {
            manager.push(Toast::info(format!("Toast {i}")));
        }

        // With small window height 120.0, margin is 16.0, top toasts can only fit at most 1 card (48.0 height)
        let rects = manager.toast_rects(800.0, 120.0);
        for (_id, card_r, _, _) in &rects {
            assert!(card_r.y >= ToastManager::SCREEN_MARGIN);
            assert!(card_r.y + card_r.height <= 120.0 - ToastManager::SCREEN_MARGIN);
        }
        assert!(rects.len() <= 1);
    }

    #[test]
    fn test_toast_pointer_absorption() {
        let mut manager = ToastManager::new();
        let toast = Toast::info("Clickable");
        manager.push(toast);

        // Tick to complete entering animation so slide_offset is 0
        manager.tick(250.0);

        let rects = manager.toast_rects(800.0, 600.0);
        assert!(!rects.is_empty());
        let (_id, card_r, _, _) = rects[0];

        // Move over toast card
        let move_over = PointerEvent::Move {
            x: card_r.x + 10.0,
            y: card_r.y + 10.0,
        };
        assert_eq!(
            manager.dispatch_pointer_event(&move_over, 800.0, 600.0),
            EventResponse::Handled
        );

        // Move outside toast card
        let move_outside = PointerEvent::Move { x: 50.0, y: 50.0 };
        assert_eq!(
            manager.dispatch_pointer_event(&move_outside, 800.0, 600.0),
            EventResponse::Handled // Hover state cleared, so handled
        );
        // Second move outside should be ignored
        assert_eq!(
            manager.dispatch_pointer_event(&move_outside, 800.0, 600.0),
            EventResponse::Ignored
        );

        // Click on toast card
        let down_event = PointerEvent::Down {
            button: MouseButton::Left,
            x: card_r.x + 10.0,
            y: card_r.y + 10.0,
        };
        assert_eq!(
            manager.dispatch_pointer_event(&down_event, 800.0, 600.0),
            EventResponse::Handled
        );

        let up_event = PointerEvent::Up {
            button: MouseButton::Left,
            x: card_r.x + 10.0,
            y: card_r.y + 10.0,
        };
        assert_eq!(
            manager.dispatch_pointer_event(&up_event, 800.0, 600.0),
            EventResponse::Handled
        );
    }

    #[test]
    fn test_toast_overflow_rapid_spawn() {
        let mut manager = ToastManager::new().with_max_toasts(3);
        for i in 0..10 {
            manager.push(Toast::info(format!("Spam {i}")));
        }

        // Excess exiting toasts must be capped at max_toasts * 2
        assert!(manager.toasts.len() <= 6);

        // Only at most 3 toasts should be in Entering or Visible phase
        let active_count = manager
            .toasts
            .iter()
            .filter(|t| t.phase == ToastPhase::Entering || t.phase == ToastPhase::Visible)
            .count();
        assert!(active_count <= 3);
    }
}
