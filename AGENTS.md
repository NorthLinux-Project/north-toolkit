# North Toolkit - Agent Guidelines

## 1. Project Overview & Architecture

North Toolkit is a high-performance, GPU-accelerated, cross-platform UI toolkit implemented in modern C++20 under the `north::` namespace. Windows (Direct3D 11 with DirectComposition) serves as the tier-1 platform target.

The toolkit is structured as a unidirectional reactive pipeline:

```text
User Code / Widgets (north::widget, north::style)
         │
         ▼
Reactive Signals (north::core::Signal<T>, north::core::Computed<T>)
         │ (dirty bit bubbling)
         ▼
Retained Tree (north::core::NodeTree, north::core::Node in SlabPool<Node>)
         │
         ├──► Layout Engine (north::layout::BoxConstraints, FlexLayout, StackLayout)
         │
         ├──► Text Pipeline (HarfBuzz, FreeType, SheenBidi, libunibreak, DirectWrite fallback)
         │           │
         │           ▼
         │   Dynamic GPU GlyphAtlas (R8, skyline shelf, subpixel bins)
         │
         ├──► Vector Paths & Tessellation (north::render::Path, SvgPathParser, PathTessellator)
         │
         ├──► Image Pipeline (WIC ImageDecoder, ImageCache, ContentScale)
         │
         └──► Overlay Subsystem (north::overlay::OverlayManager: Dialog, Menu, Popover, Tooltip)
                     │
                     ▼
DisplayList (Per-frame Arena backed command stream)
         │
         ├──► QuadBatcher (Instanced SDF quads, inner borders, erf shadows)
         ├──► ImageBatcher (Instanced textured quads with SDF corner clipping)
         ├──► PathBatcher (Analytic AA 1.0 px fringe)
         └──► GlyphDraw (Subpixel textured glyphs)
                     │
                     ▼
GPU Device Abstraction (north::gpu::Device)
         │
         ▼
Direct3D 11 Backend (Flip discard swapchain, DirectComposition, waitable frame latency pacing)
         │
         ▼
Platform Shell (north::platform::win32::Win32App, Win32Window, Per-Monitor V2 DPI, IME)
```

---

## 2. Language & Toolchain Constraints

- **Dialect**: Modern C++20 with classic headers (`.hpp` and `.cpp`).
- **Zero Exceptions & RTTI**: Compiles strictly without C++ exceptions (`/EHs-c- /D_HAS_EXCEPTIONS=0` on MSVC, `-fno-exceptions` on Clang) and without runtime type information (`/GR-` on MSVC, `-fno-rtti` on Clang).
- **Compiler Warnings**: Strict warning profiles (`/W4 /WX` on MSVC, `-Wall -Wextra -Wpedantic -Werror` on Clang). All warnings are treated as compilation errors. Never suppress warnings via ad-hoc pragma or flag modifications.
- **Build System**: CMake Presets (`CMakePresets.json` version 6) driving Ninja.
  - Windows MSVC configuration uses preset `debug-msvc`.
  - Windows Clang-cl configuration uses preset `debug`.
  - Release configuration uses preset `release`.
- **Third-Party Dependencies**: Managed strictly via Git submodules under `third_party/` (`harfbuzz`, `freetype`, `sheenbidi`, `libunibreak`, `googletest`). Submodule targets must include system headers via `SYSTEM` to prevent third-party warning leaks.

---

## 3. Core Subsystems & Conventions

### Memory Architecture
- **Per-Frame Arena (`north::core::Arena`)**: Chunked growable bump allocator retaining memory capacity between frames. Use for per-frame allocations, display list command buffers, and ephemeral string/layout scratch. No individual deallocations.
- **Slab Pool (`north::core::SlabPool<T>`)**: Chunked, pointer-stable object pool with 64-bit generational handles (`Handle<T>`: 32-bit index, 32-bit generation). Used for all retained nodes in the UI hierarchy.
- **No Untracked Heap Churn**: Avoid raw `new`/`delete` or naked heap allocations in performance-critical paths.

### Geometry & Color Primitives
- `Point`, `Size`, `Rect`, `Color`, `Edges`, and `CornerRadii` in `north::core` are flat, trivially copyable value types.
- Colors use normalized `float` values in $[0.0, 1.0]$ with support for premultiplied alpha conversion (`to_premultiplied()`).
- **Perceptual Color Space (`north::color::HctColor`, `north::style::Theme`)**: Material 3 CAM16 HCT (Hue, Chroma, Tone) perceptual color model with bisection gamut mapping. Generates mathematically harmonious tonal palettes (Primary, Secondary, Tertiary, Neutral, Neutral Variant, Error) with contrast guarantees across light and dark themes.

### Reactive State & Node Hierarchy
- **Signals (`north::core::Signal<T>`)**: Encapsulates state values and notifies registered dirty observers. Uses Small Buffer Optimization (SBO) for up to 2 inline observers. Callbacks use raw function pointers and `void*` context to avoid `std::function` allocation overhead.
- **Computed Values (`north::core::Computed<T>`)**: Push-dirty, pull-cached derived signals.
- **Retained Node Tree (`north::core::NodeTree`)**: Manages parent-child hierarchies of `Node` instances stored in `SlabPool<Node>`. Uses bitmask dirty flags (`kLayout`, `kPaint`, `kTransform`, `kChildren`, `kAll`) that bubble upward to the root, allowing layout and paint passes to skip clean subtrees.
- **Node Protocols**: Node behaviors are defined via raw function pointers (`LayoutFn`, `PaintFn`, `HitTestFn`).

### Layout Engine
- **Constraints (`north::layout::BoxConstraints`)**: Min/max width and height bounds. Supports `loosen`, `tighten`, and `constrain`.
- **Flex Layout (`north::layout::FlexLayout`)**: Two-pass row and column distribution supporting flex grow, shrink, cross-axis stretch, and gap spacing.
- **Stack Layout (`north::layout::StackLayout`)**: Z-order positioning supporting nine-point alignment for unpositioned children and explicit edge pinning via `PositionedData`.

### Vector Paths & SVG Rendering
- **Path Geometry (`north::render::Path`, `SvgPathParser`)**: MoveTo, LineTo, QuadTo, CubicTo, Close commands with SVG path string parsing.
- **Analytic AA Tessellation (`north::render::PathTessellator`, `PathBatcher`)**: Fast polygon flattening and edge extrusion producing 1.0 px anti-aliasing fringes rendered via `path.hlsl`.
- **Icon Widget (`north::widget::IconBuilder`)**: Declarative vector icons styled according to active theme tokens.

### Overlay Subsystem
- **Overlay Management (`north::overlay::OverlayManager`)**: Root-attached overlay layer hosting `Dialog`, `Menu`, `Popover`, and `Tooltip` widgets outside normal parent clipping boundaries.
- **Placement & Collision Detection**: Viewport-aware nine-point anchor positioning with automatic viewport collision flipping (bottom $\leftrightarrow$ top, right $\leftrightarrow$ left) and edge clamping.
- **Event Interception**: Modal overlays absorb outside pointer events, dismiss on outside click, and handle Escape key navigation.

### Image & Texture Pipeline
- **WIC Image Decoder (`north::image::ImageDecoder`)**: Zero-dependency decoding for PNG, JPEG, BMP, and ICO via Windows Imaging Component.
- **Texture Cache (`north::image::ImageCache`)**: Centralized deduplication cache returning reusable GPU `TextureId` handles.
- **Image Widget (`north::widget::ImageBuilder`)**: Declarative image viewing supporting `Fit`, `Cover`, `Fill`, `None`, and `ScaleDown` content scaling, 9-point alignment, and hardware-accelerated SDF corner clipping and borders via `image.hlsl` and `ImageBatcher`.

### GPU & Rendering Pipeline
- **Device Abstraction (`north::gpu::Device`)**: API-agnostic hardware interface operating on lightweight handles (`TextureId`, `BufferId`, `PipelineId`).
- **Direct3D 11 Backend (`north::gpu::D3D11Device`)**: Flip-discard swapchains (`DXGI_SWAP_EFFECT_FLIP_DISCARD`), DirectComposition visual attachment, waitable frame latency pacing (`IDXGISwapChain2`), and headless offscreen render target support for CI test execution.
- **Analytic Shaders**: Bufferless quad rendering via `SV_VertexID` in `quad.hlsl` evaluating exact anti-aliased rounded rect SDFs, inner borders, and Evan Wallace rational erf Gaussian box shadows. Textured quad rendering via `image.hlsl` with bilinear sampling and analytic SDF corner clipping. Vector path rendering via `path.hlsl` with coverage-based anti-aliasing. Glyph rendering via `glyph.hlsl` sampling 8-bit alpha textures with subpixel positioning.

---

## 4. Engineering & Agent Disciplines

### Code Cleanliness & Zero Slop
- **Surgical Edits**: Make only the minimum necessary changes required to complete the task. Never reformat untouched lines, refactor unrelated components, or churn files without explicit instruction.
- **No Speculative Abstractions**: Avoid introducing factory classes, wrapper abstractions, or helper layers for simple operations. Maintain flat, direct implementations.
- **No Stubs or Placeholders**: Write fully working, production-ready code. Never leave `TODO`, `FIXME`, or mock stubs in final deliverables.
- **Comment Discipline**: Code must be self-documenting through clear identifier naming. Never include step numbers, numerical markers, or comments explaining trivial operations. Preserve existing comments.
- **Strict Prohibition of Emojis**: Do not use emojis in code, comments, documentation, commit messages, or agent responses.
- **Case Discipline**: Do not use all-caps words for shouting or emphasis. Standard sentence case or bold markdown is required. Reserve all-caps strictly for syntactically required macros, constants, technical acronyms, or format names.

### Verification Standards
- Every modification must be validated by running the build and test suite:
  ```powershell
  cmd.exe /c "call `"`"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`"`" && cmake --build --preset debug-msvc --target unit_tests"
  cmd.exe /c "call `"`"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`"`" && ctest --preset debug-msvc --output-on-failure"
  ```
- Never declare work complete without verifying zero compiler warnings and 100% test pass rates from direct command output.

### Version Control & Git Operations
- Never run `git commit` or `git add` without explicit user instruction.
- When commits are directed, follow the single unified commit format with an imperative single-line header and no commit body or conventional prefixes.

### Execution Scope
- When a task or milestone is finished, stop and report the results immediately. Do not proceed to subsequent milestones or modify additional files without explicit user directive.
