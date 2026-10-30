//! Native Wayland client integration via `wayland-client` and `wayland-protocols`.

use std::ffi::c_void;
use std::os::fd::AsFd;
use std::ptr::NonNull;

use anyhow::{Context, Result};
use raw_window_handle::{
    RawDisplayHandle, RawWindowHandle, WaylandDisplayHandle, WaylandWindowHandle,
};
use wayland_client::protocol::{
    wl_callback::{self, WlCallback},
    wl_compositor::{self, WlCompositor},
    wl_data_device::{self, WlDataDevice},
    wl_data_device_manager::{self, WlDataDeviceManager},
    wl_data_offer::{self, WlDataOffer},
    wl_data_source::{self, WlDataSource},
    wl_keyboard::{self, WlKeyboard},
    wl_pointer::{self, WlPointer},
    wl_registry::{self, WlRegistry},
    wl_seat::{self, WlSeat},
    wl_shm::{self, WlShm},
    wl_surface::{self, WlSurface},
};
use wayland_client::{
    Connection, Dispatch, EventQueue, Proxy, QueueHandle,
};
use wayland_protocols::wp::cursor_shape::v1::client::{
    wp_cursor_shape_device_v1::{self, WpCursorShapeDeviceV1},
    wp_cursor_shape_manager_v1::{self, WpCursorShapeManagerV1},
};
use wayland_protocols::wp::primary_selection::zv1::client::{
    zwp_primary_selection_device_manager_v1::{self, ZwpPrimarySelectionDeviceManagerV1},
    zwp_primary_selection_device_v1::{self, ZwpPrimarySelectionDeviceV1},
    zwp_primary_selection_offer_v1::{self, ZwpPrimarySelectionOfferV1},
    zwp_primary_selection_source_v1::{self, ZwpPrimarySelectionSourceV1},
};
use wayland_protocols::xdg::shell::client::{
    xdg_popup::{self, XdgPopup},
    xdg_positioner::{self, Anchor, ConstraintAdjustment, Gravity, XdgPositioner},
    xdg_surface::{self, XdgSurface},
    xdg_toplevel::{self, XdgToplevel},
    xdg_wm_base::{self, XdgWmBase},
};
use wayland_protocols::xdg::decoration::zv1::client::{
    zxdg_decoration_manager_v1::{self, ZxdgDecorationManagerV1},
    zxdg_toplevel_decoration_v1::{self, ZxdgToplevelDecorationV1},
};
use wayland_protocols::wp::fractional_scale::v1::client::{
    wp_fractional_scale_manager_v1::{self, WpFractionalScaleManagerV1},
    wp_fractional_scale_v1::{self, WpFractionalScaleV1},
};
use wayland_protocols::wp::viewporter::client::{
    wp_viewport::{self, WpViewport},
    wp_viewporter::{self, WpViewporter},
};

use crate::event::{KeyEvent, Modifiers, MouseButton, PointerEvent};

/// Auxiliary Wayland popup surface state for context menus and dropdowns.
pub struct PopupSurface {
    /// Wayland surface handle.
    pub surface: WlSurface,
    /// XDG surface role.
    pub xdg_surface: XdgSurface,
    /// XDG popup role.
    pub xdg_popup: XdgPopup,
    /// Viewport for fractional scaling.
    pub viewport: Option<WpViewport>,
    /// Popup X coordinate relative to anchor.
    pub x: i32,
    /// Popup Y coordinate relative to anchor.
    pub y: i32,
    /// Width in logical pixels.
    pub width: u32,
    /// Height in logical pixels.
    pub height: u32,
    /// Indicates whether initial configure was acknowledged.
    pub configured: bool,
    /// Set when compositor dismisses the popup.
    pub is_dismissed: bool,
    /// Raw window handle for Vulkan surface attachment.
    pub window_handle: RawWindowHandle,
}

/// Window decoration mode negotiated with the compositor.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum WindowDecorations {
    /// Server-side decorations provided by compositor.
    #[default]
    ServerSide,
    /// Client-side decorations rendered by North Toolkit.
    ClientSide,
    /// Frameless borderless window.
    None,
}

/// Edges and corners for interactive window resizing.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum ResizeEdge {
    /// No edge active.
    #[default]
    None,
    /// Top edge.
    Top,
    /// Bottom edge.
    Bottom,
    /// Left edge.
    Left,
    /// Top-left corner.
    TopLeft,
    /// Bottom-left corner.
    BottomLeft,
    /// Right edge.
    Right,
    /// Top-right corner.
    TopRight,
    /// Bottom-right corner.
    BottomRight,
}

impl ResizeEdge {
    /// Converts to the Wayland `xdg_toplevel::ResizeEdge` enum.
    pub fn to_wayland(self) -> xdg_toplevel::ResizeEdge {
        match self {
            Self::None => xdg_toplevel::ResizeEdge::None,
            Self::Top => xdg_toplevel::ResizeEdge::Top,
            Self::Bottom => xdg_toplevel::ResizeEdge::Bottom,
            Self::Left => xdg_toplevel::ResizeEdge::Left,
            Self::TopLeft => xdg_toplevel::ResizeEdge::TopLeft,
            Self::BottomLeft => xdg_toplevel::ResizeEdge::BottomLeft,
            Self::Right => xdg_toplevel::ResizeEdge::Right,
            Self::TopRight => xdg_toplevel::ResizeEdge::TopRight,
            Self::BottomRight => xdg_toplevel::ResizeEdge::BottomRight,
        }
    }

    /// Maps the edge to an appropriate bidirectional resize cursor shape.
    pub fn cursor_icon(self) -> Option<crate::event::CursorIcon> {
        match self {
            Self::None => None,
            Self::Top | Self::Bottom => Some(crate::event::CursorIcon::NsResize),
            Self::Left | Self::Right => Some(crate::event::CursorIcon::EwResize),
            Self::TopLeft | Self::BottomRight => Some(crate::event::CursorIcon::NwseResize),
            Self::TopRight | Self::BottomLeft => Some(crate::event::CursorIcon::NeswResize),
        }
    }
}

/// Wayland client globals, seats, input queues, and window state.
pub struct WaylandState {
    pub compositor: Option<WlCompositor>,
    pub shm: Option<WlShm>,
    pub wm_base: Option<XdgWmBase>,
    pub decoration_manager: Option<ZxdgDecorationManagerV1>,
    pub toplevel_decoration: Option<ZxdgToplevelDecorationV1>,
    pub fractional_scale_manager: Option<WpFractionalScaleManagerV1>,
    pub fractional_scale: Option<WpFractionalScaleV1>,
    pub viewporter: Option<WpViewporter>,
    pub viewport: Option<WpViewport>,
    pub seat: Option<WlSeat>,
    pub pointer: Option<WlPointer>,
    pub keyboard: Option<WlKeyboard>,
    pub surface: Option<WlSurface>,
    pub xdg_surface: Option<XdgSurface>,
    pub xdg_toplevel: Option<XdgToplevel>,
    pub popup: Option<PopupSurface>,
    pub pointer_surface: Option<WlSurface>,
    pub last_seat_serial: u32,
    pub width: u32,
    pub height: u32,
    pub scale: f32,
    pub configured: bool,
    pub should_exit: bool,
    pub pointer_events: Vec<PointerEvent>,
    pub key_events: Vec<KeyEvent>,
    pub resize_requested: Option<(u32, u32)>,
    pub pointer_pos: (f32, f32),
    pub frame_callback: Option<WlCallback>,
    pub frame_ready: bool,
    pub xkb_context: xkbcommon::xkb::Context,
    pub xkb_keymap: Option<xkbcommon::xkb::Keymap>,
    pub xkb_state: Option<xkbcommon::xkb::State>,
    pub data_device_manager: Option<WlDataDeviceManager>,
    pub data_device: Option<WlDataDevice>,
    pub pending_offers: Vec<(WlDataOffer, Vec<String>)>,
    pub current_data_offer: Option<WlDataOffer>,
    pub current_data_offer_mimes: Vec<String>,
    pub clipboard_text: Option<String>,
    pub data_source: Option<WlDataSource>,
    pub primary_selection_device_manager: Option<ZwpPrimarySelectionDeviceManagerV1>,
    pub primary_selection_device: Option<ZwpPrimarySelectionDeviceV1>,
    pub pending_primary_offers: Vec<(ZwpPrimarySelectionOfferV1, Vec<String>)>,
    pub current_primary_offer: Option<ZwpPrimarySelectionOfferV1>,
    pub current_primary_offer_mimes: Vec<String>,
    pub primary_text: Option<String>,
    pub primary_source: Option<ZwpPrimarySelectionSourceV1>,
    pub pending_paste_pipes: Vec<crate::platform::clipboard::PipeReader>,
    pub paste_events: Vec<(String, bool)>,
    pub cursor_shape_manager: Option<WpCursorShapeManagerV1>,
    pub cursor_shape_device: Option<WpCursorShapeDeviceV1>,
    pub current_cursor_shape: crate::event::CursorIcon,
    pub pointer_enter_serial: u32,
    pub is_maximized: bool,
    pub is_active: bool,
    pub decoration_mode: WindowDecorations,
}

impl WaylandState {
    pub fn new(width: u32, height: u32) -> Self {
        let xkb_context = xkbcommon::xkb::Context::new(xkbcommon::xkb::CONTEXT_NO_FLAGS);
        Self {
            compositor: None,
            shm: None,
            wm_base: None,
            decoration_manager: None,
            toplevel_decoration: None,
            fractional_scale_manager: None,
            fractional_scale: None,
            viewporter: None,
            viewport: None,
            seat: None,
            pointer: None,
            keyboard: None,
            surface: None,
            xdg_surface: None,
            xdg_toplevel: None,
            popup: None,
            pointer_surface: None,
            last_seat_serial: 0,
            width,
            height,
            scale: 1.0,
            configured: false,
            should_exit: false,
            pointer_events: Vec::new(),
            key_events: Vec::new(),
            resize_requested: None,
            pointer_pos: (0.0, 0.0),
            is_maximized: false,
            is_active: true,
            decoration_mode: WindowDecorations::ServerSide,
            frame_callback: None,
            frame_ready: true,
            xkb_context,
            xkb_keymap: None,
            xkb_state: None,
            data_device_manager: None,
            data_device: None,
            pending_offers: Vec::new(),
            current_data_offer: None,
            current_data_offer_mimes: Vec::new(),
            clipboard_text: None,
            data_source: None,
            primary_selection_device_manager: None,
            primary_selection_device: None,
            pending_primary_offers: Vec::new(),
            current_primary_offer: None,
            current_primary_offer_mimes: Vec::new(),
            primary_text: None,
            primary_source: None,
            pending_paste_pipes: Vec::new(),
            paste_events: Vec::new(),
            cursor_shape_manager: None,
            cursor_shape_device: None,
            current_cursor_shape: crate::event::CursorIcon::Default,
            pointer_enter_serial: 0,
        }
    }
}

/// Native Wayland window handle, client event queue, and connection context.
pub struct WaylandWindow {
    /// Connection to the active Wayland display server.
    pub connection: Connection,
    /// Event queue for draining Wayland protocol events.
    pub event_queue: Option<EventQueue<WaylandState>>,
    /// Queue handle for dispatching proxy requests.
    pub qh: QueueHandle<WaylandState>,
    /// Retained Wayland client state.
    pub state: WaylandState,
    /// Raw display handle for Vulkan surface attachment.
    pub display_handle: RawDisplayHandle,
    /// Raw window handle for Vulkan surface attachment.
    pub window_handle: RawWindowHandle,
}

impl WaylandWindow {
    /// Connects to the Wayland display and creates a configured toplevel XDG window.
    pub fn new(title: &str, width: u32, height: u32) -> Result<Self> {
        let connection = Connection::connect_to_env()
            .context("Failed to connect to Wayland display")?;
        let mut event_queue: EventQueue<WaylandState> = connection.new_event_queue();
        let qh = event_queue.handle();

        let display = connection.display();
        let _registry = display.get_registry(&qh, ());

        let mut state = WaylandState::new(width, height);
        event_queue.roundtrip(&mut state)?;

        let compositor = state
            .compositor
            .as_ref()
            .context("Wayland compositor global not available")?;
        let wm_base = state
            .wm_base
            .as_ref()
            .context("xdg_wm_base global not available")?;

        let surface = compositor.create_surface(&qh, ());
        let xdg_surface = wm_base.get_xdg_surface(&surface, &qh, ());
        let xdg_toplevel = xdg_surface.get_toplevel(&qh, ());

        xdg_toplevel.set_title(title.to_string());
        xdg_toplevel.set_app_id("org.north.toolkit".to_string());
        xdg_toplevel.set_min_size(200, 150);

        if let Some(dec_mgr) = &state.decoration_manager {
            let decoration = dec_mgr.get_toplevel_decoration(&xdg_toplevel, &qh, ());
            decoration.set_mode(zxdg_toplevel_decoration_v1::Mode::ServerSide);
            state.toplevel_decoration = Some(decoration);
        }

        if let Some(viewporter) = &state.viewporter {
            let viewport = viewporter.get_viewport(&surface, &qh, ());
            viewport.set_destination(width as i32, height as i32);
            state.viewport = Some(viewport);
        }

        if let Some(frac_mgr) = &state.fractional_scale_manager {
            let frac = frac_mgr.get_fractional_scale(&surface, &qh, ());
            state.fractional_scale = Some(frac);
        }

        surface.commit();

        state.surface = Some(surface.clone());
        state.xdg_surface = Some(xdg_surface);
        state.xdg_toplevel = Some(xdg_toplevel);

        event_queue.roundtrip(&mut state)?;

        let display_ptr = connection.backend().display_ptr() as *mut c_void;
        let surface_ptr = surface.id().as_ptr() as *mut c_void;

        let disp_ptr = NonNull::new(display_ptr).context("Null Wayland display ptr")?;
        let surf_ptr = NonNull::new(surface_ptr).context("Null Wayland surface ptr")?;

        let display_handle = RawDisplayHandle::Wayland(WaylandDisplayHandle::new(disp_ptr));
        let window_handle = RawWindowHandle::Wayland(WaylandWindowHandle::new(surf_ptr));

        Ok(Self {
            connection,
            event_queue: Some(event_queue),
            qh,
            state,
            display_handle,
            window_handle,
        })
    }

    /// Requests a `wl_surface::frame` callback to synchronize rendering with the compositor refresh rate.
    pub fn request_frame(&mut self) {
        if self.state.frame_callback.is_none() {
            if let Some(surface) = &self.state.surface {
                let callback = surface.frame(&self.qh, ());
                self.state.frame_callback = Some(callback);
                self.state.frame_ready = false;
                surface.commit();
            }
        }
    }

    /// Initiates an interactive compositor window move drag.
    pub fn start_interactive_move(&self) {
        if let (Some(toplevel), Some(seat)) = (&self.state.xdg_toplevel, &self.state.seat) {
            toplevel._move(seat, self.state.last_seat_serial);
        }
    }

    /// Initiates an interactive compositor window resize from the specified edge.
    pub fn start_interactive_resize(&self, edge: ResizeEdge) {
        if let (Some(toplevel), Some(seat)) = (&self.state.xdg_toplevel, &self.state.seat) {
            toplevel.resize(seat, self.state.last_seat_serial, edge.to_wayland());
        }
    }

    /// Toggles the window between maximized and restored states.
    pub fn toggle_maximize(&mut self) {
        if let Some(toplevel) = &self.state.xdg_toplevel {
            if self.state.is_maximized {
                toplevel.unset_maximized();
            } else {
                toplevel.set_maximized();
            }
        }
    }

    /// Minimizes the window to the compositor taskbar or dock.
    pub fn minimize(&self) {
        if let Some(toplevel) = &self.state.xdg_toplevel {
            toplevel.set_minimized();
        }
    }

    /// Configures the window decoration mode (CSD vs SSD).
    pub fn set_decoration_mode(&mut self, mode: WindowDecorations) {
        self.state.decoration_mode = mode;
        if let Some(decoration) = &self.state.toplevel_decoration {
            match mode {
                WindowDecorations::ClientSide => {
                    decoration.set_mode(zxdg_toplevel_decoration_v1::Mode::ClientSide);
                }
                WindowDecorations::ServerSide => {
                    decoration.set_mode(zxdg_toplevel_decoration_v1::Mode::ServerSide);
                }
                WindowDecorations::None => {}
            }
        }
    }

    /// Opens an auxiliary Wayland popup positioned relative to an anchor rectangle.
    pub fn open_popup(
        &mut self,
        anchor_x: i32,
        anchor_y: i32,
        anchor_w: u32,
        anchor_h: u32,
        popup_w: u32,
        popup_h: u32,
    ) -> Result<()> {
        let compositor = self
            .state
            .compositor
            .as_ref()
            .context("Wayland compositor global not available")?;
        let wm_base = self
            .state
            .wm_base
            .as_ref()
            .context("xdg_wm_base global not available")?;
        let parent_xdg_surface = self
            .state
            .xdg_surface
            .as_ref()
            .context("Parent xdg_surface not available")?;

        let surface = compositor.create_surface(&self.qh, ());
        let xdg_surface = wm_base.get_xdg_surface(&surface, &self.qh, ());

        let positioner = wm_base.create_positioner(&self.qh, ());
        positioner.set_size(popup_w as i32, popup_h as i32);
        positioner.set_anchor_rect(anchor_x, anchor_y, anchor_w as i32, anchor_h as i32);
        positioner.set_anchor(Anchor::BottomLeft);
        positioner.set_gravity(Gravity::BottomRight);
        positioner.set_constraint_adjustment(
            ConstraintAdjustment::SlideX
                | ConstraintAdjustment::SlideY
                | ConstraintAdjustment::FlipY,
        );

        let xdg_popup = xdg_surface.get_popup(Some(parent_xdg_surface), &positioner, &self.qh, ());

        if let Some(seat) = &self.state.seat {
            if self.state.last_seat_serial > 0 {
                xdg_popup.grab(seat, self.state.last_seat_serial);
            }
        }

        let viewport = if let Some(viewporter) = &self.state.viewporter {
            let vp = viewporter.get_viewport(&surface, &self.qh, ());
            vp.set_destination(popup_w as i32, popup_h as i32);
            Some(vp)
        } else {
            None
        };

        surface.commit();

        let surface_ptr = surface.id().as_ptr() as *mut c_void;
        let surf_ptr = NonNull::new(surface_ptr).context("Null popup surface ptr")?;
        let window_handle = RawWindowHandle::Wayland(WaylandWindowHandle::new(surf_ptr));

        self.state.popup = Some(PopupSurface {
            surface,
            xdg_surface,
            xdg_popup,
            viewport,
            x: 0,
            y: 0,
            width: popup_w,
            height: popup_h,
            configured: false,
            is_dismissed: false,
            window_handle,
        });

        positioner.destroy();
        Ok(())
    }

    /// Dismisses and frees the active popup surface.
    pub fn dismiss_popup(&mut self) {
        if let Some(popup) = self.state.popup.take() {
            popup.xdg_popup.destroy();
            popup.xdg_surface.destroy();
            if let Some(vp) = popup.viewport {
                vp.destroy();
            }
            popup.surface.destroy();
        }
    }

    /// Returns `true` if the mouse pointer is currently hovering over the popup surface.
    pub fn is_pointer_on_popup(&self) -> bool {
        if let Some(popup) = &self.state.popup {
            self.state.pointer_surface.as_ref() == Some(&popup.surface)
        } else {
            false
        }
    }

    /// Publishes text to the standard Wayland clipboard data device (`Ctrl+C`).
    pub fn set_clipboard_text(&mut self, text: String) {
        self.state.clipboard_text = Some(text);
        if let Some(mgr) = &self.state.data_device_manager {
            let source = mgr.create_data_source(&self.qh, ());
            for &mime in crate::platform::clipboard::UTF8_MIME_TYPES {
                source.offer(mime.to_string());
            }
            if let Some(device) = &self.state.data_device {
                device.set_selection(Some(&source), self.state.last_seat_serial);
            }
            self.state.data_source = Some(source);
        }
    }

    /// Publishes text to the Wayland primary selection device (mouse highlight).
    pub fn set_primary_text(&mut self, text: String) {
        self.state.primary_text = Some(text);
        if let Some(mgr) = &self.state.primary_selection_device_manager {
            let source = mgr.create_source(&self.qh, ());
            for &mime in crate::platform::clipboard::UTF8_MIME_TYPES {
                source.offer(mime.to_string());
            }
            if let Some(device) = &self.state.primary_selection_device {
                device.set_selection(Some(&source), self.state.last_seat_serial);
            }
            self.state.primary_source = Some(source);
        }
    }

    /// Requests asynchronous text paste from either the clipboard or primary selection.
    pub fn request_paste(&mut self, target: crate::platform::clipboard::SelectionTarget) {
        match target {
            crate::platform::clipboard::SelectionTarget::Clipboard => {
                if let Some(offer) = &self.state.current_data_offer {
                    if let Some(best_mime) =
                        crate::platform::clipboard::find_best_utf8_mime(&self.state.current_data_offer_mimes)
                    {
                        if let Ok((read_fd, write_fd)) = crate::platform::clipboard::create_nonblocking_pipe() {
                            offer.receive(best_mime.to_string(), write_fd.as_fd());
                            drop(write_fd);
                            self.state.pending_paste_pipes.push(
                                crate::platform::clipboard::PipeReader::new(read_fd, target),
                            );
                        }
                    }
                }
            }
            crate::platform::clipboard::SelectionTarget::Primary => {
                if let Some(offer) = &self.state.current_primary_offer {
                    if let Some(best_mime) =
                        crate::platform::clipboard::find_best_utf8_mime(&self.state.current_primary_offer_mimes)
                    {
                        if let Ok((read_fd, write_fd)) = crate::platform::clipboard::create_nonblocking_pipe() {
                            offer.receive(best_mime.to_string(), write_fd.as_fd());
                            drop(write_fd);
                            self.state.pending_paste_pipes.push(
                                crate::platform::clipboard::PipeReader::new(read_fd, target),
                            );
                        }
                    }
                }
            }
        }
    }

    /// Updates the server-side mouse cursor shape via `wp_cursor_shape_v1`.
    pub fn set_cursor_shape(&mut self, icon: crate::event::CursorIcon) {
        if self.state.current_cursor_shape == icon {
            return;
        }
        self.state.current_cursor_shape = icon;
        if let Some(device) = &self.state.cursor_shape_device {
            if self.state.pointer_enter_serial > 0 {
                device.set_shape(
                    self.state.pointer_enter_serial,
                    icon.to_wayland_shape(),
                );
            }
        }
    }
}

impl Dispatch<XdgPositioner, ()> for WaylandState {
    fn event(
        _: &mut Self,
        _: &XdgPositioner,
        _: xdg_positioner::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {}
}

impl Dispatch<XdgPopup, ()> for WaylandState {
    fn event(
        state: &mut Self,
        _: &XdgPopup,
        event: xdg_popup::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        match event {
            xdg_popup::Event::Configure { x, y, width, height } => {
                if let Some(popup) = &mut state.popup {
                    popup.x = x;
                    popup.y = y;
                    if width > 0 && height > 0 {
                        popup.width = width as u32;
                        popup.height = height as u32;
                    }
                    popup.configured = true;
                }
            }
            xdg_popup::Event::PopupDone => {
                if let Some(popup) = &mut state.popup {
                    popup.is_dismissed = true;
                }
            }
            _ => {}
        }
    }
}

impl Dispatch<WlRegistry, ()> for WaylandState {
    fn event(
        state: &mut Self,
        registry: &WlRegistry,
        event: wl_registry::Event,
        _: &(),
        _: &Connection,
        qh: &QueueHandle<Self>,
    ) {
        if let wl_registry::Event::Global {
            name,
            interface,
            version,
        } = event
        {
            match interface.as_str() {
                "wl_compositor" => {
                    state.compositor = Some(registry.bind::<WlCompositor, _, _>(name, version.min(4), qh, ()));
                }
                "wl_shm" => {
                    state.shm = Some(registry.bind::<WlShm, _, _>(name, 1, qh, ()));
                }
                "xdg_wm_base" => {
                    state.wm_base = Some(registry.bind::<XdgWmBase, _, _>(name, 1, qh, ()));
                }
                "wl_seat" => {
                    let seat = registry.bind::<WlSeat, _, _>(name, version.min(5), qh, ());
                    if let Some(mgr) = &state.data_device_manager {
                        state.data_device = Some(mgr.get_data_device(&seat, qh, ()));
                    }
                    if let Some(mgr) = &state.primary_selection_device_manager {
                        state.primary_selection_device = Some(mgr.get_device(&seat, qh, ()));
                    }
                    state.seat = Some(seat);
                }
                "wl_data_device_manager" => {
                    let mgr = registry.bind::<WlDataDeviceManager, _, _>(name, version.min(3), qh, ());
                    if let Some(seat) = &state.seat {
                        state.data_device = Some(mgr.get_data_device(seat, qh, ()));
                    }
                    state.data_device_manager = Some(mgr);
                }
                "zwp_primary_selection_device_manager_v1" => {
                    let mgr = registry.bind::<ZwpPrimarySelectionDeviceManagerV1, _, _>(name, 1, qh, ());
                    if let Some(seat) = &state.seat {
                        state.primary_selection_device = Some(mgr.get_device(seat, qh, ()));
                    }
                    state.primary_selection_device_manager = Some(mgr);
                }
                "zxdg_decoration_manager_v1" => {
                    state.decoration_manager = Some(registry.bind::<ZxdgDecorationManagerV1, _, _>(name, 1, qh, ()));
                }
                "wp_fractional_scale_manager_v1" => {
                    state.fractional_scale_manager = Some(registry.bind::<WpFractionalScaleManagerV1, _, _>(name, 1, qh, ()));
                }
                "wp_viewporter" => {
                    state.viewporter = Some(registry.bind::<WpViewporter, _, _>(name, 1, qh, ()));
                }
                "wp_cursor_shape_manager_v1" => {
                    let mgr = registry.bind::<WpCursorShapeManagerV1, _, _>(name, version.min(1), qh, ());
                    if let Some(pointer) = &state.pointer {
                        state.cursor_shape_device = Some(mgr.get_pointer(pointer, qh, ()));
                    }
                    state.cursor_shape_manager = Some(mgr);
                }
                _ => {}
            }
        }
    }
}

impl Dispatch<XdgWmBase, ()> for WaylandState {
    fn event(
        _: &mut Self,
        wm_base: &XdgWmBase,
        event: xdg_wm_base::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        if let xdg_wm_base::Event::Ping { serial } = event {
            wm_base.pong(serial);
        }
    }
}

impl Dispatch<XdgSurface, ()> for WaylandState {
    fn event(
        state: &mut Self,
        xdg_surface: &XdgSurface,
        event: xdg_surface::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        if let xdg_surface::Event::Configure { serial } = event {
            xdg_surface.ack_configure(serial);
            if let Some(popup) = &mut state.popup {
                if &popup.xdg_surface == xdg_surface {
                    popup.configured = true;
                    popup.surface.commit();
                    return;
                }
            }
            state.configured = true;
            if let Some(surf) = &state.surface {
                surf.commit();
            }
        }
    }
}

impl Dispatch<XdgToplevel, ()> for WaylandState {
    fn event(
        state: &mut Self,
        _: &XdgToplevel,
        event: xdg_toplevel::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        match event {
            xdg_toplevel::Event::Configure { width, height, states } => {
                let mut is_maximized = false;
                let mut is_active = false;
                for chunk in states.chunks_exact(4) {
                    let val = u32::from_ne_bytes(chunk.try_into().unwrap());
                    match val {
                        1 => is_maximized = true,
                        4 => is_active = true,
                        _ => {}
                    }
                }
                state.is_maximized = is_maximized;
                state.is_active = is_active;

                if width > 0 && height > 0 {
                    let (w, h) = (width as u32, height as u32);
                    if (w, h) != (state.width, state.height) {
                        state.width = w;
                        state.height = h;
                        if let Some(vp) = &state.viewport {
                            vp.set_destination(w as i32, h as i32);
                        }
                        state.resize_requested = Some((w, h));
                    }
                }
            }
            xdg_toplevel::Event::Close => {
                state.should_exit = true;
            }
            _ => {}
        }
    }
}

impl Dispatch<WlSeat, ()> for WaylandState {
    fn event(
        state: &mut Self,
        seat: &WlSeat,
        event: wl_seat::Event,
        _: &(),
        _: &Connection,
        qh: &QueueHandle<Self>,
    ) {
        if let wl_seat::Event::Capabilities { capabilities } = event {
            let caps = wl_seat::Capability::from_bits_truncate(capabilities.into());
            if caps.contains(wl_seat::Capability::Pointer) && state.pointer.is_none() {
                let pointer = seat.get_pointer(qh, ());
                if let Some(mgr) = &state.cursor_shape_manager {
                    state.cursor_shape_device = Some(mgr.get_pointer(&pointer, qh, ()));
                }
                state.pointer = Some(pointer);
            }
            if caps.contains(wl_seat::Capability::Keyboard) && state.keyboard.is_none() {
                state.keyboard = Some(seat.get_keyboard(qh, ()));
            }
        }
    }
}

impl Dispatch<WlPointer, ()> for WaylandState {
    fn event(
        state: &mut Self,
        _: &WlPointer,
        event: wl_pointer::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        match event {
            wl_pointer::Event::Enter {
                serial,
                surface,
                surface_x,
                surface_y,
            } => {
                let px = surface_x as f32;
                let py = surface_y as f32;
                state.last_seat_serial = serial;
                state.pointer_enter_serial = serial;
                state.pointer_surface = Some(surface);
                state.pointer_pos = (px, py);
                if let Some(device) = &state.cursor_shape_device {
                    device.set_shape(serial, state.current_cursor_shape.to_wayland_shape());
                }
                state.pointer_events.push(PointerEvent::Move { x: px, y: py });
            }
            wl_pointer::Event::Leave { surface, .. } => {
                if state.pointer_surface.as_ref() == Some(&surface) {
                    state.pointer_surface = None;
                }
            }
            wl_pointer::Event::Motion {
                surface_x,
                surface_y,
                ..
            } => {
                let px = surface_x as f32;
                let py = surface_y as f32;
                state.pointer_pos = (px, py);
                state.pointer_events.push(PointerEvent::Move { x: px, y: py });
            }
            wl_pointer::Event::Button {
                serial,
                button,
                state: btn_state,
                ..
            } => {
                state.last_seat_serial = serial;
                let mb = match button {
                    272 => MouseButton::Left,
                    273 => MouseButton::Right,
                    274 => MouseButton::Middle,
                    other => MouseButton::Other(other),
                };

                let (px, py) = state.pointer_pos;
                if btn_state == wayland_client::WEnum::Value(wl_pointer::ButtonState::Pressed) {
                    state.pointer_events.push(PointerEvent::Down {
                        x: px,
                        y: py,
                        button: mb,
                    });
                } else {
                    state.pointer_events.push(PointerEvent::Up {
                        x: px,
                        y: py,
                        button: mb,
                    });
                }
            }
            wl_pointer::Event::Axis { value, .. } => {
                let (px, py) = state.pointer_pos;
                state.pointer_events.push(PointerEvent::Scroll {
                    x: px,
                    y: py,
                    dx: 0.0,
                    dy: value as f32 / 10.0,
                });
            }
            _ => {}
        }
    }
}

impl Dispatch<WlKeyboard, ()> for WaylandState {
    fn event(
        state: &mut Self,
        _: &WlKeyboard,
        event: wl_keyboard::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        match event {
            wl_keyboard::Event::Keymap { format, fd, size } => {
                if format == wayland_client::WEnum::Value(wl_keyboard::KeymapFormat::XkbV1) {
                    use std::fs::File;
                    use std::io::Read;
                    let mut file = File::from(fd);
                    let mut buf = vec![0u8; size as usize];
                    if file.read_exact(&mut buf).is_ok() {
                        if let Some(pos) = buf.iter().position(|&b| b == 0) {
                            buf.truncate(pos);
                        }
                        if let Ok(keymap_str) = String::from_utf8(buf) {
                            if let Some(keymap) = xkbcommon::xkb::Keymap::new_from_string(
                                &state.xkb_context,
                                keymap_str,
                                xkbcommon::xkb::KEYMAP_FORMAT_TEXT_V1,
                                xkbcommon::xkb::KEYMAP_COMPILE_NO_FLAGS,
                            ) {
                                let xkb_state = xkbcommon::xkb::State::new(&keymap);
                                state.xkb_keymap = Some(keymap);
                                state.xkb_state = Some(xkb_state);
                            }
                        }
                    }
                }
            }
            wl_keyboard::Event::Modifiers {
                mods_depressed,
                mods_latched,
                mods_locked,
                group,
                ..
            } => {
                if let Some(xkb_state) = &mut state.xkb_state {
                    xkb_state.update_mask(
                        mods_depressed,
                        mods_latched,
                        mods_locked,
                        0,
                        0,
                        group,
                    );
                }
            }
            wl_keyboard::Event::Key {
                serial,
                key,
                state: key_state,
                ..
            } => {
                state.last_seat_serial = serial;
                let is_pressed =
                    key_state == wayland_client::WEnum::Value(wl_keyboard::KeyState::Pressed);
                let xkb_keycode = xkbcommon::xkb::Keycode::new(key + 8);

                let (keysym, key_str, text_char, modifiers) = if let Some(xkb_state) = &state.xkb_state {
                    let sym = xkb_state.key_get_one_sym(xkb_keycode);
                    let name = xkbcommon::xkb::keysym_get_name(sym);
                    let utf8 = xkb_state.key_get_utf8(xkb_keycode);
                    let text = if !utf8.is_empty() && is_pressed {
                        let first = utf8.chars().next().unwrap();
                        if first >= ' ' || first == '\t' || first == '\n' {
                            Some(utf8)
                        } else {
                            None
                        }
                    } else {
                        None
                    };
                    let mods = Modifiers {
                        shift: xkb_state.mod_name_is_active(
                            xkbcommon::xkb::MOD_NAME_SHIFT,
                            xkbcommon::xkb::STATE_MODS_EFFECTIVE,
                        ),
                        ctrl: xkb_state.mod_name_is_active(
                            xkbcommon::xkb::MOD_NAME_CTRL,
                            xkbcommon::xkb::STATE_MODS_EFFECTIVE,
                        ),
                        alt: xkb_state.mod_name_is_active(
                            xkbcommon::xkb::MOD_NAME_ALT,
                            xkbcommon::xkb::STATE_MODS_EFFECTIVE,
                        ),
                        logo: xkb_state.mod_name_is_active(
                            xkbcommon::xkb::MOD_NAME_LOGO,
                            xkbcommon::xkb::STATE_MODS_EFFECTIVE,
                        ),
                    };
                    (sym.raw(), name, text, mods)
                } else {
                    (0, format!("Key{}", key), None, Modifiers::default())
                };

                if is_pressed {
                    state.key_events.push(KeyEvent::KeyDown {
                        key: key_str,
                        keysym,
                        text: text_char,
                        modifiers,
                    });
                } else {
                    state.key_events.push(KeyEvent::KeyUp {
                        key: key_str,
                        keysym,
                        modifiers,
                    });
                }
            }
            _ => {}
        }
    }
}

impl Dispatch<WlCallback, ()> for WaylandState {
    fn event(
        state: &mut Self,
        callback: &WlCallback,
        event: wl_callback::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        if let wl_callback::Event::Done { .. } = event {
            state.frame_ready = true;
            if let Some(current) = &state.frame_callback {
                if current.id() == callback.id() {
                    state.frame_callback = None;
                }
            }
        }
    }
}

impl Dispatch<WlCompositor, ()> for WaylandState {
    fn event(
        _: &mut Self,
        _: &WlCompositor,
        _: wl_compositor::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
    }
}

impl Dispatch<WlShm, ()> for WaylandState {
    fn event(
        _: &mut Self,
        _: &WlShm,
        _: wl_shm::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
    }
}

impl Dispatch<WlSurface, ()> for WaylandState {
    fn event(
        _: &mut Self,
        _: &WlSurface,
        _: wl_surface::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
    }
}

impl Dispatch<ZxdgDecorationManagerV1, ()> for WaylandState {
    fn event(
        _: &mut Self,
        _: &ZxdgDecorationManagerV1,
        _: zxdg_decoration_manager_v1::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
    }
}

impl Dispatch<ZxdgToplevelDecorationV1, ()> for WaylandState {
    fn event(
        state: &mut Self,
        _: &ZxdgToplevelDecorationV1,
        event: zxdg_toplevel_decoration_v1::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        if let zxdg_toplevel_decoration_v1::Event::Configure { mode } = event {
            if let wayland_client::WEnum::Value(m) = mode {
                match m {
                    zxdg_toplevel_decoration_v1::Mode::ClientSide => {
                        state.decoration_mode = WindowDecorations::ClientSide;
                    }
                    zxdg_toplevel_decoration_v1::Mode::ServerSide => {
                        state.decoration_mode = WindowDecorations::ServerSide;
                    }
                    _ => {}
                }
            }
        }
    }
}

impl Dispatch<WpFractionalScaleManagerV1, ()> for WaylandState {
    fn event(
        _: &mut Self,
        _: &WpFractionalScaleManagerV1,
        _: wp_fractional_scale_manager_v1::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
    }
}

impl Dispatch<WpFractionalScaleV1, ()> for WaylandState {
    fn event(
        state: &mut Self,
        _: &WpFractionalScaleV1,
        event: wp_fractional_scale_v1::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        if let wp_fractional_scale_v1::Event::PreferredScale { scale } = event {
            let new_scale = (scale as f32) / 120.0;
            if (new_scale - state.scale).abs() > 0.001 {
                state.scale = new_scale;
                state.resize_requested = Some((state.width, state.height));
            }
        }
    }
}

impl Dispatch<WpViewporter, ()> for WaylandState {
    fn event(
        _: &mut Self,
        _: &WpViewporter,
        _: wp_viewporter::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
    }
}

impl Dispatch<WpViewport, ()> for WaylandState {
    fn event(
        _: &mut Self,
        _: &WpViewport,
        _: wp_viewport::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
    }
}

impl Dispatch<WlDataDeviceManager, ()> for WaylandState {
    fn event(
        _: &mut Self,
        _: &WlDataDeviceManager,
        _: wl_data_device_manager::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
    }
}

impl Dispatch<WlDataDevice, ()> for WaylandState {
    fn event(
        state: &mut Self,
        _: &WlDataDevice,
        event: wl_data_device::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        match event {
            wl_data_device::Event::DataOffer { id } => {
                state.pending_offers.push((id, Vec::new()));
            }
            wl_data_device::Event::Selection { id } => {
                if let Some(offer) = id {
                    let mimes = if let Some(pos) = state.pending_offers.iter().position(|(o, _)| *o == offer) {
                        state.pending_offers.remove(pos).1
                    } else {
                        Vec::new()
                    };
                    state.current_data_offer = Some(offer);
                    state.current_data_offer_mimes = mimes;
                } else {
                    state.current_data_offer = None;
                    state.current_data_offer_mimes.clear();
                }
            }
            _ => {}
        }
    }

    wayland_client::event_created_child!(WaylandState, WlDataDevice, [
        0 => (WlDataOffer, ()),
    ]);
}

impl Dispatch<WlDataOffer, ()> for WaylandState {
    fn event(
        state: &mut Self,
        offer: &WlDataOffer,
        event: wl_data_offer::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        match event {
            wl_data_offer::Event::Offer { mime_type } => {
                if let Some((_, mimes)) = state.pending_offers.iter_mut().find(|(o, _)| o == offer) {
                    mimes.push(mime_type.clone());
                }
                if let Some(curr) = &state.current_data_offer {
                    if curr == offer {
                        state.current_data_offer_mimes.push(mime_type);
                    }
                }
            }
            _ => {}
        }
    }
}

impl Dispatch<WlDataSource, ()> for WaylandState {
    fn event(
        state: &mut Self,
        _: &WlDataSource,
        event: wl_data_source::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        match event {
            wl_data_source::Event::Send { mime_type: _, fd } => {
                if let Some(text) = &state.clipboard_text {
                    let _ = crate::platform::clipboard::write_text_to_pipe(&fd, text);
                }
            }
            wl_data_source::Event::Cancelled => {
                state.data_source = None;
                state.clipboard_text = None;
            }
            _ => {}
        }
    }
}

impl Dispatch<ZwpPrimarySelectionDeviceManagerV1, ()> for WaylandState {
    fn event(
        _: &mut Self,
        _: &ZwpPrimarySelectionDeviceManagerV1,
        _: zwp_primary_selection_device_manager_v1::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
    }
}

impl Dispatch<ZwpPrimarySelectionDeviceV1, ()> for WaylandState {
    fn event(
        state: &mut Self,
        _: &ZwpPrimarySelectionDeviceV1,
        event: zwp_primary_selection_device_v1::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        match event {
            zwp_primary_selection_device_v1::Event::DataOffer { offer } => {
                state.pending_primary_offers.push((offer, Vec::new()));
            }
            zwp_primary_selection_device_v1::Event::Selection { id } => {
                if let Some(offer) = id {
                    let mimes = if let Some(pos) = state.pending_primary_offers.iter().position(|(o, _)| *o == offer) {
                        state.pending_primary_offers.remove(pos).1
                    } else {
                        Vec::new()
                    };
                    state.current_primary_offer = Some(offer);
                    state.current_primary_offer_mimes = mimes;
                } else {
                    state.current_primary_offer = None;
                    state.current_primary_offer_mimes.clear();
                }
            }
            _ => {}
        }
    }

    wayland_client::event_created_child!(WaylandState, ZwpPrimarySelectionDeviceV1, [
        0 => (ZwpPrimarySelectionOfferV1, ()),
    ]);
}

impl Dispatch<ZwpPrimarySelectionOfferV1, ()> for WaylandState {
    fn event(
        state: &mut Self,
        offer: &ZwpPrimarySelectionOfferV1,
        event: zwp_primary_selection_offer_v1::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        match event {
            zwp_primary_selection_offer_v1::Event::Offer { mime_type } => {
                if let Some((_, mimes)) = state.pending_primary_offers.iter_mut().find(|(o, _)| o == offer) {
                    mimes.push(mime_type.clone());
                }
                if let Some(curr) = &state.current_primary_offer {
                    if curr == offer {
                        state.current_primary_offer_mimes.push(mime_type);
                    }
                }
            }
            _ => {}
        }
    }
}

impl Dispatch<ZwpPrimarySelectionSourceV1, ()> for WaylandState {
    fn event(
        state: &mut Self,
        _: &ZwpPrimarySelectionSourceV1,
        event: zwp_primary_selection_source_v1::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        match event {
            zwp_primary_selection_source_v1::Event::Send { mime_type: _, fd } => {
                if let Some(text) = &state.primary_text {
                    let _ = crate::platform::clipboard::write_text_to_pipe(&fd, text);
                }
            }
            zwp_primary_selection_source_v1::Event::Cancelled => {
                state.primary_source = None;
                state.primary_text = None;
            }
            _ => {}
        }
    }
}

impl Dispatch<WpCursorShapeManagerV1, ()> for WaylandState {
    fn event(
        _: &mut Self,
        _: &WpCursorShapeManagerV1,
        _: wp_cursor_shape_manager_v1::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
    }
}

impl Dispatch<WpCursorShapeDeviceV1, ()> for WaylandState {
    fn event(
        _: &mut Self,
        _: &WpCursorShapeDeviceV1,
        _: wp_cursor_shape_device_v1::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_xkb_keymap_nul_terminator_handling() {
        let context = xkbcommon::xkb::Context::new(xkbcommon::xkb::CONTEXT_NO_FLAGS);
        let raw_xkb = b"xkb_keymap { xkb_keycodes { minimum = 8; maximum = 255; }; xkb_types { type \"ONE_LEVEL\" { modifiers = none; map[none] = Level1; level_name[Level1] = \"Any\"; }; }; xkb_compatibility { }; xkb_symbols { }; };\0";

        let mut buf = raw_xkb.to_vec();
        if let Some(pos) = buf.iter().position(|&b| b == 0) {
            buf.truncate(pos);
        }

        let keymap_str = String::from_utf8(buf).expect("Valid UTF-8 keymap");
        let keymap = xkbcommon::xkb::Keymap::new_from_string(
            &context,
            keymap_str,
            xkbcommon::xkb::KEYMAP_FORMAT_TEXT_V1,
            xkbcommon::xkb::KEYMAP_COMPILE_NO_FLAGS,
        );
        assert!(keymap.is_some());
    }
}
