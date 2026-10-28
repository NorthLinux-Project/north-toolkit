use std::ffi::c_void;
use std::ptr::NonNull;

use anyhow::{Context, Result};
use raw_window_handle::{
    RawDisplayHandle, RawWindowHandle, WaylandDisplayHandle, WaylandWindowHandle,
};
use wayland_client::protocol::{
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

use crate::event::{KeyEvent, MouseButton, PointerEvent};

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
    pub width: u32,
    pub height: u32,
    pub scale: f32,
    pub configured: bool,
    pub should_exit: bool,
    pub pointer_events: Vec<PointerEvent>,
    pub key_events: Vec<KeyEvent>,
    pub resize_requested: Option<(u32, u32)>,
    pub pointer_pos: (f32, f32),
    pub shift_held: bool,
    pub caps_lock: bool,
}

impl WaylandState {
    pub fn new(width: u32, height: u32) -> Self {
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
            width,
            height,
            scale: 1.0,
            configured: false,
            should_exit: false,
            pointer_events: Vec::new(),
            key_events: Vec::new(),
            resize_requested: None,
            pointer_pos: (0.0, 0.0),
            shift_held: false,
            caps_lock: false,
        }
    }
}

pub struct WaylandWindow {
    pub connection: Connection,
    pub event_queue: Option<EventQueue<WaylandState>>,
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
            state,
            display_handle,
            window_handle,
        })
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
                surface_x,
                surface_y,
                ..
            } => {
                let px = surface_x as f32;
                let py = surface_y as f32;
                state.pointer_pos = (px, py);
                state.pointer_events.push(PointerEvent::Move { x: px, y: py });
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
                button,
                state: btn_state,
                ..
            } => {
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
            wl_keyboard::Event::Modifiers {
                mods_depressed,
                mods_locked,
                ..
            } => {
                state.shift_held = (mods_depressed & 1) != 0;
                state.caps_lock = (mods_locked & 2) != 0;
            }
            wl_keyboard::Event::Key {
                key,
                state: key_state,
                ..
            } => {
                let is_pressed =
                    key_state == wayland_client::WEnum::Value(wl_keyboard::KeyState::Pressed);
                if key == 42 || key == 54 {
                    state.shift_held = is_pressed;
                }
                if key == 58 && is_pressed {
                    state.caps_lock = !state.caps_lock;
                }

                let shift = state.shift_held;
                let uppercase = shift ^ state.caps_lock;

                let (key_str, text_char) = match key {
                    14 => ("Backspace".to_string(), None),
                    28 => ("Return".to_string(), Some("\n".to_string())),
                    57 => ("Space".to_string(), Some(" ".to_string())),
                    105 => ("Left".to_string(), None),
                    106 => ("Right".to_string(), None),
                    103 => ("Up".to_string(), None),
                    108 => ("Down".to_string(), None),
                    102 => ("Home".to_string(), None),
                    107 => ("End".to_string(), None),
                    111 => ("Delete".to_string(), None),
                    15 => ("Tab".to_string(), Some("\t".to_string())),

                    // Digits row
                    41 => if shift { ("~".to_string(), Some("~".to_string())) } else { ("`".to_string(), Some("`".to_string())) },
                    2 => if shift { ("!".to_string(), Some("!".to_string())) } else { ("1".to_string(), Some("1".to_string())) },
                    3 => if shift { ("@".to_string(), Some("@".to_string())) } else { ("2".to_string(), Some("2".to_string())) },
                    4 => if shift { ("#".to_string(), Some("#".to_string())) } else { ("3".to_string(), Some("3".to_string())) },
                    5 => if shift { ("$".to_string(), Some("$".to_string())) } else { ("4".to_string(), Some("4".to_string())) },
                    6 => if shift { ("%".to_string(), Some("%".to_string())) } else { ("5".to_string(), Some("5".to_string())) },
                    7 => if shift { ("^".to_string(), Some("^".to_string())) } else { ("6".to_string(), Some("6".to_string())) },
                    8 => if shift { ("&".to_string(), Some("&".to_string())) } else { ("7".to_string(), Some("7".to_string())) },
                    9 => if shift { ("*".to_string(), Some("*".to_string())) } else { ("8".to_string(), Some("8".to_string())) },
                    10 => if shift { ("(".to_string(), Some("(".to_string())) } else { ("9".to_string(), Some("9".to_string())) },
                    11 => if shift { (")".to_string(), Some(")".to_string())) } else { ("0".to_string(), Some("0".to_string())) },
                    12 => if shift { ("_".to_string(), Some("_".to_string())) } else { ("-".to_string(), Some("-".to_string())) },
                    13 => if shift { ("+".to_string(), Some("+".to_string())) } else { ("=".to_string(), Some("=".to_string())) },

                    // Letters row 1
                    16 => if uppercase { ("Q".to_string(), Some("Q".to_string())) } else { ("q".to_string(), Some("q".to_string())) },
                    17 => if uppercase { ("W".to_string(), Some("W".to_string())) } else { ("w".to_string(), Some("w".to_string())) },
                    18 => if uppercase { ("E".to_string(), Some("E".to_string())) } else { ("e".to_string(), Some("e".to_string())) },
                    19 => if uppercase { ("R".to_string(), Some("R".to_string())) } else { ("r".to_string(), Some("r".to_string())) },
                    20 => if uppercase { ("T".to_string(), Some("T".to_string())) } else { ("t".to_string(), Some("t".to_string())) },
                    21 => if uppercase { ("Y".to_string(), Some("Y".to_string())) } else { ("y".to_string(), Some("y".to_string())) },
                    22 => if uppercase { ("U".to_string(), Some("U".to_string())) } else { ("u".to_string(), Some("u".to_string())) },
                    23 => if uppercase { ("I".to_string(), Some("I".to_string())) } else { ("i".to_string(), Some("i".to_string())) },
                    24 => if uppercase { ("O".to_string(), Some("O".to_string())) } else { ("o".to_string(), Some("o".to_string())) },
                    25 => if uppercase { ("P".to_string(), Some("P".to_string())) } else { ("p".to_string(), Some("p".to_string())) },
                    26 => if shift { ("{".to_string(), Some("{".to_string())) } else { ("[".to_string(), Some("[".to_string())) },
                    27 => if shift { ("}".to_string(), Some("}".to_string())) } else { ("]".to_string(), Some("]".to_string())) },
                    43 => if shift { ("|".to_string(), Some("|".to_string())) } else { ("\\".to_string(), Some("\\".to_string())) },

                    // Letters row 2
                    30 => if uppercase { ("A".to_string(), Some("A".to_string())) } else { ("a".to_string(), Some("a".to_string())) },
                    31 => if uppercase { ("S".to_string(), Some("S".to_string())) } else { ("s".to_string(), Some("s".to_string())) },
                    32 => if uppercase { ("D".to_string(), Some("D".to_string())) } else { ("d".to_string(), Some("d".to_string())) },
                    33 => if uppercase { ("F".to_string(), Some("F".to_string())) } else { ("f".to_string(), Some("f".to_string())) },
                    34 => if uppercase { ("G".to_string(), Some("G".to_string())) } else { ("g".to_string(), Some("g".to_string())) },
                    35 => if uppercase { ("H".to_string(), Some("H".to_string())) } else { ("h".to_string(), Some("h".to_string())) },
                    36 => if uppercase { ("J".to_string(), Some("J".to_string())) } else { ("j".to_string(), Some("j".to_string())) },
                    37 => if uppercase { ("K".to_string(), Some("K".to_string())) } else { ("k".to_string(), Some("k".to_string())) },
                    38 => if uppercase { ("L".to_string(), Some("L".to_string())) } else { ("l".to_string(), Some("l".to_string())) },
                    39 => if shift { (":".to_string(), Some(":".to_string())) } else { (";".to_string(), Some(";".to_string())) },
                    40 => if shift { ("\"".to_string(), Some("\"".to_string())) } else { ("'".to_string(), Some("'".to_string())) },

                    // Letters row 3
                    44 => if uppercase { ("Z".to_string(), Some("Z".to_string())) } else { ("z".to_string(), Some("z".to_string())) },
                    45 => if uppercase { ("X".to_string(), Some("X".to_string())) } else { ("x".to_string(), Some("x".to_string())) },
                    46 => if uppercase { ("C".to_string(), Some("C".to_string())) } else { ("c".to_string(), Some("c".to_string())) },
                    47 => if uppercase { ("V".to_string(), Some("V".to_string())) } else { ("v".to_string(), Some("v".to_string())) },
                    48 => if uppercase { ("B".to_string(), Some("B".to_string())) } else { ("b".to_string(), Some("b".to_string())) },
                    49 => if uppercase { ("N".to_string(), Some("N".to_string())) } else { ("n".to_string(), Some("n".to_string())) },
                    50 => if uppercase { ("M".to_string(), Some("M".to_string())) } else { ("m".to_string(), Some("m".to_string())) },
                    51 => if shift { ("<".to_string(), Some("<".to_string())) } else { (",".to_string(), Some(",".to_string())) },
                    52 => if shift { (">".to_string(), Some(">".to_string())) } else { (".".to_string(), Some(".".to_string())) },
                    53 => if shift { ("?".to_string(), Some("?".to_string())) } else { ("/".to_string(), Some("/".to_string())) },

                    _ => (format!("Key{}", key), None),
                };

                if is_pressed {
                    state.key_events.push(KeyEvent::KeyDown {
                        key: key_str,
                        text: text_char,
                    });
                } else {
                    state.key_events.push(KeyEvent::KeyUp { key: key_str });
                }
            }
            _ => {}
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
