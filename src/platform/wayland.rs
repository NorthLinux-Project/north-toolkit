use std::ffi::c_void;
use std::ptr::NonNull;

use anyhow::{Context, Result};
use raw_window_handle::{
    RawDisplayHandle, RawWindowHandle, WaylandDisplayHandle, WaylandWindowHandle,
};
use wayland_client::protocol::{
    wl_callback::{self, WlCallback},
    wl_compositor::{self, WlCompositor},
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

pub struct PopupSurface {
    pub surface: WlSurface,
    pub xdg_surface: XdgSurface,
    pub xdg_popup: XdgPopup,
    pub viewport: Option<WpViewport>,
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
    pub configured: bool,
    pub is_dismissed: bool,
    pub window_handle: RawWindowHandle,
}

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
            frame_callback: None,
            frame_ready: true,
            xkb_context,
            xkb_keymap: None,
            xkb_state: None,
        }
    }
}

pub struct WaylandWindow {
    pub connection: Connection,
    pub event_queue: Option<EventQueue<WaylandState>>,
    pub qh: QueueHandle<WaylandState>,
    pub state: WaylandState,
    pub display_handle: RawDisplayHandle,
    pub window_handle: RawWindowHandle,
}

impl WaylandWindow {
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

    pub fn is_pointer_on_popup(&self) -> bool {
        if let Some(popup) = &self.state.popup {
            self.state.pointer_surface.as_ref() == Some(&popup.surface)
        } else {
            false
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
                    state.seat = Some(registry.bind::<WlSeat, _, _>(name, version.min(5), qh, ()));
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
            xdg_toplevel::Event::Configure { width, height, .. } => {
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
                state.pointer = Some(seat.get_pointer(qh, ()));
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
                state.pointer_surface = Some(surface);
                state.pointer_pos = (px, py);
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
        _: &mut Self,
        _: &ZxdgToplevelDecorationV1,
        _: zxdg_toplevel_decoration_v1::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
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
