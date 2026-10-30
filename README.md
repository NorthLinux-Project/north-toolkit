# North Toolkit

A high-performance, retained-mode GUI toolkit for Linux Wayland written in Rust (Edition 2024), powered by Vulkan 1.3, native Wayland client protocols, a non-blocking Calloop event reactor, a Flexbox layout engine, and hardware-accelerated text and vector rasterization.

---

## Architecture Overview

```
                      +---------------------------------------+
                      |                app.rs                 |
                      | Application Orchestrator & Event Loop |
                      +-------------------+-------------------+
                                          |
        +---------------------------------+---------------------------------+
        |                                 |                                 |
        v                                 v                                 v
+-------------------------------+ +-------------------------------+ +-------------------------------+
|           platform/           | |            widget/            | |            render/            |
|  - wayland.rs (wl_surface,    | | - tree.rs (WidgetTree/Id)     | | - vulkan/ (Ash 1.3 pipeline)  |
|    xdg_shell, fractional scale| | - state.rs (Reactive Property)| | - primitives.rs (Quads/Glyphs)|
|    viewport, seat/input)      | | - button, label, input, etc.  | | - atlas.rs (Glyph bin-packing)|
+-------------------------------+ +---------------+---------------+ +-------------------------------+
                                                  |
                        +-------------------------+-------------------------+
                        |                         |                         |
                        v                         v                         v
        +-------------------------------+ +-----------------+ +-------------------------------+
        |            layout/            | |     theme/      | |             text/             |
        | - flex.rs (Flexbox engine)    | | - tokens.rs     | | - shaper.rs (rustybuzz)       |
        | - LayoutEngine & Rects        | | - theme.rs      | | - rasterizer.rs (swash)       |
        +-------------------------------+ +-----------------+ | - font_db.rs (fontdb fallback)|
                                                              +-------------------------------+
```

### Core Subsystems

- **Platform (`src/platform/`)**: Native Wayland client integration (`wayland-client` 0.31, `wayland-protocols`). Directly manages `wl_surface`, `xdg_wm_base`, `xdg_toplevel`, `xdg_popup`, `wp_viewport`, `wp_fractional_scale_manager_v1`, `wl_seat` pointer and keyboard focus via `xkbcommon`, and Wayland data device clipboard transactions (`wl_data_device_manager`).
- **Render Engine (`src/render/`)**: Hardware-accelerated Vulkan 1.3 renderer using `ash`. Double/triple-buffered frame synchronization with Vulkan fences and semaphores, dynamic 2D shelf-bin-packing texture atlases for subpixel text glyphs and arbitrary SVG vector paths, and batched pipeline drawing (`QuadPipeline`, `GlyphPipeline`).
- **Layout (`src/layout/`)**: Flexbox engine (`LayoutEngine`) resolving `FlexDirection`, `FlexWrap`, `JustifyContent`, `AlignItems`, `AlignSelf`, `Dimension` (Points, Percent, Auto), `EdgeInsets`, and intrinsic content measurements.
- **Text & Font Pipeline (`src/text/`)**: High-performance OpenType text shaping using `rustybuzz`, subpixel glyph rasterization via `swash`, system and bundled font discovery with fallback chaining via `fontdb`, and an LRU text shape cache.
- **Retained Widget Arena (`src/widget/`)**: Arena-allocated generational `WidgetTree` mapping generational `WidgetId`s to widgets, reactive property change tracking (`Property<T>`), hit-testing, focus management, and client-side window decorations (`Titlebar`).
- **Animation (`src/animation/`)**: Easing curves (Linear, EaseIn, EaseOut, EaseInOut, Elastic, Bounce) and frame-interpolated tween state machines.
- **Theming (`src/theme/`)**: Design tokens for typography, spacing, corner radii, elevation borders, and dark/light color palettes.

---

## Supported Widgets

- **Container**: Flexbox layout container with background colors, borders, and rounded corners.
- **Button**: Interactive button with hover, press, focus rings, disabled states, and click callbacks.
- **Label**: Dynamic text display with reactive property binding.
- **TextInput**: Single-line text input with selection dragging, double-click word selection, clipboard copy/cut/paste, and cursor blinking.
- **Checkbox**: Boolean checkbox toggle with custom icons and labels.
- **Switch**: Animated iOS/Material-style sliding toggle switch with spring-damper easing.
- **Slider**: Continuous or stepped value slider with draggable thumb and keyboard arrow adjustments.
- **ScrollArea**: Viewport container with smooth inertia scrolling and draggable scrollbar thumb.
- **VirtualList**: Virtualized list capable of rendering 100,000+ items with uniform or variable row heights, single/multi-selection, and keyboard navigation.
- **TableView**: Multi-column data grid with interactive column header resizing, numeric/string column sorting, and row virtualization.
- **PopupMenu & Dropdown**: Wayland `xdg_popup` surfaces and inline menus with keyboard traversal.
- **Dialog**: Animated modal dialog overlays with dimmed background scrims, autofocus, and Escape/backdrop dismissal.
- **Icon**: Scalable vector icons rasterized into dynamic vector texture atlases.
- **Svg**: Arbitrary SVG path string renderer supporting custom fills, stroke widths, and viewBox transforms.
- **Titlebar**: Client-side window decorations (CSD) with minimize, maximize/restore, close buttons, double-click maximize, and interactive edge resizing.
- **Tooltip**: Hover tooltips with configurable delay timers, fade animations, and edge-clamped positioning.

---

## Prerequisites

North Toolkit requires a Linux system with a functional Wayland compositor and Vulkan 1.3 drivers:

- **Wayland Compositor**: Sway, Hyprland, GNOME Wayland, KDE Plasma Wayland, or Weston.
- **Vulkan 1.3 Driver**: Mesa (RADV, ANV, NVK) or proprietary NVIDIA driver (510+).
- **System Libraries**:
  - `libwayland-client.so.0`
  - `libxkbcommon.so.0`
  - `libvulkan.so.1` (Vulkan loader)

On Debian/Ubuntu:
```bash
sudo apt-get install -y libwayland-dev libxkbcommon-dev libvulkan-dev vulkan-tools
```

On Fedora/RHEL:
```bash
sudo dnf install -y wayland-devel libxkbcommon-devel vulkan-loader-devel vulkan-tools
```

On Arch Linux:
```bash
sudo pacman -S --needed wayland libxkbcommon vulkan-icd-loader vulkan-tools
```

---

## Quickstart

Run the built-in showcase application:

```bash
cargo run --example hello_world
```

### Basic Example

```rust
use north_toolkit::app::Application;
use north_toolkit::layout::{Dimension, EdgeInsets, FlexDirection, FlexStyle};
use north_toolkit::theme::Theme;
use north_toolkit::widget::{Button, Container, Label};

fn main() -> anyhow::Result<()> {
    let mut app = Application::new(800.0, 600.0, "North Toolkit Example")?;
    let mut root = Container::new(FlexStyle {
        direction: FlexDirection::Column,
        padding: EdgeInsets::all(24.0),
        gap: 16.0,
        width: Dimension::Percent(100.0),
        height: Dimension::Percent(100.0),
        ..Default::default()
    });

    let label = Label::new("Welcome to North Toolkit!").with_font_size(20.0);
    let button = Button::new("Click Me").on_click(|| {
        println!("Button clicked!");
    });

    let label_id = app.tree.add_widget(label);
    let button_id = app.tree.add_widget(button);
    let root_id = app.tree.add_widget(root);

    app.tree.append_child(root_id, label_id);
    app.tree.append_child(root_id, button_id);
    app.tree.set_root(root_id);

    app.run()
}
```

---

## Verification & Development

Run the full verification suite across the library and examples:

```bash
# Verify compilation with all warnings treated as errors
RUSTFLAGS="-D warnings" cargo check --all-targets

# Run the complete test suite
cargo test

# Generate complete API documentation
RUSTDOCFLAGS="-D warnings" cargo doc --no-deps

# Build the example binary
cargo build --example hello_world
```

---

## License

Licensed under either of:

- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
- MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.
