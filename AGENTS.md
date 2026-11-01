# AGENTS.md - North Toolkit

North Toolkit is a high-performance, retained-mode GUI toolkit for Linux Wayland written in Rust (Edition 2024), powered by Vulkan 1.3 (Ash 0.38), Wayland Client (0.31), Calloop (0.14), and custom Flexbox layout and text shaping engines.

---

## 1. Subsystem Architecture & Responsibilities

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

### Module Breakdown
- `src/app.rs`: Central application orchestrator (`Application`). Integrates the Wayland surface, Vulkan swapchain, frame renderer, glyph cache, layout engine, event dispatcher, key-repeat timer, and runs the Calloop event loop.
- `src/platform/`:
  - `wayland.rs`: Native Wayland client integration using `wayland-client` and `wayland-protocols`. Handles `xdg_wm_base`, `wl_surface`, `wp_viewport`, `wp_fractional_scale_manager_v1`, input events (`wl_seat`, `wl_keyboard::repeat_info`), IME (`zwp_text_input_v3`), and presentation timing via `wl_surface::frame`.
- `src/widget/`: Retained widget tree and state management.
  - `tree.rs`: Flat widget arena (`WidgetTree`) mapping generational `WidgetId`s to boxed `Widget` nodes.
  - `state.rs`: Reactive `Property<T>` structures with fine-grained dirty bit tracking for relayout and repaint.
  - `button.rs`, `checkbox.rs`, `container.rs`, `dialog.rs`, `icon.rs`, `image.rs`, `label.rs`, `popup.rs`, `progress.rs`, `radio.rs`, `scroll_area.rs`, `slider.rs`, `split_view.rs`, `svg.rs`, `switch.rs`, `table_view.rs`, `tabs.rs`, `text_area.rs`, `text_input.rs`, `titlebar.rs`, `toast.rs`, `tooltip.rs`, `virtual_list.rs`: Core concrete widget implementations.
- `src/layout/`:
  - `flex.rs`: Custom Flexbox layout engine (`LayoutEngine`). Resolves `FlexDirection`, `JustifyContent`, `AlignItems`, `AlignSelf`, `Dimension` (Points, Percent, Auto), `EdgeInsets`, and dynamic intrinsic text measurements.
- `src/text/`:
  - `shaper.rs`: Fast OpenType text shaping using `rustybuzz` to extract glyph IDs, horizontal advances, character clusters, and baseline offsets.
  - `rasterizer.rs`: Subpixel and alpha glyph rasterization via `swash`.
  - `font_db.rs`: System and bundled font database (`fontdb`) providing font family matching and fallback chains.
  - `editing.rs`: Shared character-indexed `EditBuffer` for cursor, selection, word-boundary navigation, and IME surrounding-text deletion.
- `src/render/`:
  - `primitives.rs`: Low-level visual primitives (`RenderPrimitive::Quad`, `RenderPrimitive::Glyph`, `Border`, `Color`).
  - `atlas.rs`: Dynamic 2D bin-packing texture atlas for glyph caching.
  - `vulkan/`: Vulkan 1.3 hardware rendering backend via `ash`.
    - `context.rs`: Logical device, instance, physical device scoring, and queue family discovery.
    - `swapchain.rs`: Vulkan surface swapchain management and recreation on window resize.
    - `pipeline.rs`: Specialized graphics pipelines (`QuadPipeline`, `GlyphPipeline`) with push constants and descriptor sets.
    - `frame.rs`: Double/triple-buffered frame synchronization (`FrameRenderer`) using Vulkan command pools, semaphores, and fences.
    - `texture.rs`: Vulkan image allocation, memory binding, and pixel data uploads.
- `src/event/`:
  - `dispatcher.rs`: Dispatches mouse, keyboard, and IME events (`PointerEvent`, `KeyEvent`, `ImeEvent`), tracks `KeyRepeat` scheduling, performs hit-testing against calculated `LayoutRect`s, tracks focused/hovered widgets, and manages event bubbling.
- `src/animation/`:
  - `tween.rs`: Easing curves (Linear, EaseIn, EaseOut, EaseInOut, Elastic, Bounce) and tween state progression.
- `src/theme/`:
  - `tokens.rs`: Standardized design tokens for colors, corner radii, typography, elevation, and spacing.

---

## 2. Concurrency & Event Loop Invariants

- **Non-Blocking UI Reactor**: The Calloop event loop thread processes Wayland server messages, user input, and timer ticks. **Never** block this thread with synchronous disk I/O, heavy file parsing, or CPU-intensive image decoding.
- **Wayland Source Integration**: Wayland messages are drained via `calloop::generic::Generic` on the Wayland connection file descriptor alongside `calloop::timer::Timer` sources. Window configuration, scaling, IME, and input events update local state synchronously during loop dispatch.
- **Frame Callback Throttling**: Renders are synchronized to the compositor's refresh rate using `wl_surface::frame` callbacks. Never submit frames in an unconstrained loop; only request a new frame callback when the widget tree is dirty.
- **Dirty Tree Propagation**: Mutations to reactive properties (`Property<T>`) mark widgets with `DirtyFlags::LAYOUT` or `DirtyFlags::PAINT`. If no dirty flags are set, skip the layout and render passes entirely to conserve CPU/GPU cycles.

---

## 3. Ash Vulkan & Unsafe Code Policy

- **Mandatory `// SAFETY:` Comments**: Every `unsafe` block must be preceded by an explicit `// SAFETY:` comment clearly detailing:
  1. The caller preconditions and resource validity.
  2. Pointer boundaries, alignment, and non-null guarantees.
  3. Lifetime and synchronization guarantees of Vulkan handles.
- **Per-Frame Fence Synchronization**: Prior to recording commands into a frame's command buffer, wait for that frame's fence (`device.wait_for_fences(&[frame.fence], true, u64::MAX)`) and reset it immediately after.
- **RAII Teardown Ordering**: Vulkan resources must be destroyed in strict reverse order of creation. Always call `device.device_wait_idle()` before destroying pipelines, descriptor pools, command pools, or the logical `Device`.
- **Dynamic Atlas Synchronization**: When uploading new glyphs to the texture atlas, insert pipeline memory barriers (`vk::PipelineStageFlags::TRANSFER` -> `vk::PipelineStageFlags::FRAGMENT_SHADER`) to prevent sampling unwritten texture memory.

---

## 4. UI, Layout, & Text Invariants

- **Two-Pass Cycle (Layout -> Render)**:
  1. **Layout Pass**: `layout_engine.compute_layout()` calculates absolute `LayoutRect` bounds for all visible nodes.
  2. **Render Pass**: Widgets generate batches of `RenderPrimitive::Quad` and `RenderPrimitive::Glyph` using their computed rects. Never compute layouts or measure text inside the render method.
- **Text Shaping Caching**: Text shaping is an expensive operation. Cache `ShapedText` within widgets or labels; reshape only when font size, text content, or maximum container width changes.
- **Fractional Scaling & HiDPI**: All layout calculations operate in logical points. Before rendering, convert logical dimensions to physical device pixels using the active scale factor (`scale = window.state.scale`). Viewport destinations (`wp_viewport.set_destination`) must remain in logical coordinates.
- **Hit Testing & Clipping**: Pointer hit-testing traverses the widget tree top-to-bottom in reverse render order (front-to-back). Widgets with `overflow: Hidden` must clip child bounds during hit testing and push scissors to the Vulkan command buffer.

---

## 5. Development & Verification Commands

All modifications must be verified using the standard Cargo workflow:

```bash
# Verify compilation across library and examples
cargo check --all-targets

# Run all unit tests
cargo test

# Enforce zero compiler warnings
RUSTFLAGS="-D warnings" cargo check --all-targets

# Run the hello world example application
cargo run --example hello_world
```

---

## 6. Agent Behavioral Rules (Do & Don't)

### DO:
- **Surgical Edits**: Touch only the lines required for the specific task. Maintain established architecture and design patterns.
- **Add Tests for New Features**: Add unit tests for every new widget state, layout calculation, easing function, or text shaping edge case.
- **Idiomatic Error Handling**: Use `anyhow::Result` with `.context(...)` for initialization and application setup; use `thiserror` for library domain errors.
- **Tracing Instrumentation**: Log lifecycle, Wayland configure, and Vulkan setup events with `tracing::debug`, `tracing::info`, or `tracing::error`.

### DON'T:
- **No Speculative Abstractions**: Do not introduce ECS architectures, Tokio/async-std runtimes, or excessive trait layers. Direct structs and Calloop event loops are standard.
- **No Raw Unchecked Panics**: Never leave bare `unwrap()` calls on external Wayland client events or Vulkan allocations. Propagate errors cleanly.
- **No Mid-Frame Allocations**: Avoid heap allocations (`Vec::new`, `String::clone`) inside per-frame render routines; reuse primitive buffers across frames.
