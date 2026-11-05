# Text Pipeline & Typography

North Toolkit features an industrial-grade text shaping and rasterization engine. Built to handle complex multilingual scripts, bidirectional text, and fractional typography, the pipeline integrates HarfBuzz, FreeType, SheenBidi, libunibreak, and DirectWrite system fallback into a unified cache-backed typography subsystem.

---

## 1. Multi-Engine Architecture

Text layout executes as a multi-stage pipeline converting UTF-8 strings into shaped glyph runs and packed GPU texture quads:

```mermaid
flowchart TD
    Input["UTF-8 Input Text"]
    Break["Line & Grapheme Breaking<br/>(libunibreak)"]
    Bidi["Bidirectional Reordering<br/>(SheenBidi)"]
    Fallback["Font Matching & Fallback<br/>(DirectWrite)"]
    Shape["Glyph Shaping & Positioning<br/>(HarfBuzz)"]
    Raster["Subpixel Rasterization<br/>(FreeType)"]
    Atlas["Dynamic GPU Glyph Atlas<br/>(R8 Skyline Shelf, 4 Subpixel Bins)"]
    Draw["DisplayList Glyph Batching<br/>(GlyphDraw / glyph.hlsl)"]

    Input --> Break
    Break --> Bidi
    Bidi --> Fallback
    Fallback --> Shape
    Shape --> Raster
    Raster --> Atlas
    Atlas --> Draw
```

### Subsystem Roles
- **[`libunibreak`](../third_party/libunibreak/)**: Determines line-breaking opportunities according to Unicode Standard Annex #14 and grapheme cluster boundaries according to UAX #29.
- **[`SheenBidi`](../third_party/sheenbidi/)**: Implements the Unicode Bidirectional Algorithm (UAX #9) for mixed left-to-right (LTR) and right-to-left (RTL) scripts (e.g., Arabic, Hebrew).
- **DirectWrite Fallback**: When an active font face lacks glyph coverage for a Unicode codepoint, North queries DirectWrite system font collections to locate matching fallback fonts, avoiding "missing glyph" replacement boxes ($\Box$).
- **[`HarfBuzz`](../third_party/harfbuzz/)**: Performs OpenType complex script shaping, ligature substitution, kerning pairs, and mark attachment.
- **[`FreeType`](../third_party/freetype/)**: Loads vector glyph outlines and renders high-precision 8-bit anti-aliased alpha bitmaps with hinting.

---

## 2. Dynamic GPU Glyph Atlas (`GlyphAtlas`)

Rendered glyphs are cached on the GPU in a dynamic `R8_UNORM` texture atlas managed by a skyline shelf bin-packing algorithm.

```mermaid
flowchart LR
    subgraph Atlas["Dynamic R8 Glyph Atlas (2048 x 2048)"]
        Shelf1["Shelf 0 (Height 24 px): 'A', 'B', 'C', 'g', 'p'"]
        Shelf2["Shelf 1 (Height 32 px): Large Header Glyphs"]
        Shelf3["Shelf 2 (Height 16 px): Caption Glyphs"]
        Unused["Unallocated Texture Space..."]
    end
```

### 4-Bin Subpixel Positioning
Rasterizing a glyph strictly at integer pixel coordinates causes noticeable jitter and irregular spacing during fractional scrolling or fine layout adjustments. North Toolkit solves this using **horizontal subpixel binning**:

- Each glyph key combines `(font_id, glyph_index, font_size, subpixel_bin)`.
- The fractional horizontal offset $x - \lfloor x \rfloor$ is mapped to one of 4 discrete subpixel bins:
  $$\text{bin} = \lfloor 4.0 \times (x - \lfloor x \rfloor) + 0.5 \rfloor \pmod 4 \in \{0.0\text{ px}, 0.25\text{ px}, 0.5\text{ px}, 0.75\text{ px}\}$$
- FreeType renders glyph bitmaps with the corresponding fractional subpixel shift, guaranteeing crisp, optically balanced typography without blurriness or color fringing.

### Cache Deduplication & Updates
- If a requested glyph is already resident in the atlas, its cached UV coordinates are returned in $O(1)$ time via a hash map lookup.
- If missing, FreeType rasterizes the bitmap, the skyline shelf allocator allocates atlas space, and [`Device::update_texture_region`](../src/north/gpu/device.hpp) streams only the dirty rectangle to the GPU.

---

## 3. Paragraph, Multiline Editor & Fuzzy Matcher

### Paragraph & Text Layout
The high-level [`Paragraph`](../src/north/text/paragraph.hpp) and [`Shaper`](../src/north/text/shaper.hpp) coordinate shaping, word wrapping, and text measurement across single-line and multiline runs.

### Multiline Text Editor (`text::TextEditor`)
[`TextEditor`](../src/north/text/text_editor.hpp) provides a headless 2D document editing engine backing [`TextAreaBuilder`](../src/north/widget/text_area.hpp):
- **2D Cursor Navigation**: Up/Down navigation preserving preferred horizontal column affinity across lines of varying lengths, plus Left/Right, Home/End, and Ctrl+Word movement.
- **UTF-8 Boundary Safety**: All cursor movements, selections, and deletions align strictly to UTF-8 codepoint boundaries.
- **Coalesced Undo/Redo**: Consecutive character insertions are coalesced into atomic undo snapshots, paired with Win32 `CF_UNICODETEXT` clipboard integration ([`Clipboard`](../src/north/platform/clipboard.hpp)).

### Subsequence Fuzzy Matcher (`text::fuzzy_match`)
[`fuzzy_match`](../src/north/text/fuzzy_matcher.hpp) powers real-time filtering in [`CommandPaletteBuilder`](../src/north/widget/command_palette.hpp) and [`ComboBoxBuilder`](../src/north/widget/combo_box.hpp):
- Evaluates case-insensitive subsequence matches with prefix bonuses, word-boundary bonuses (spaces, hyphens, underscores, dots, and camelCase transitions), and consecutive character streak bonuses.
- Returns ranked scores and matched character index spans for highlight rendering.
