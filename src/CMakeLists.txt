add_library(north STATIC
    north/core/assert.cpp
    north/core/types.cpp
    north/core/arena.cpp
    north/core/tree.cpp
    north/layout/flex.cpp
    north/layout/stack.cpp
    north/gpu/d3d11/d3d11_device.cpp
    north/render/display_list.cpp
    north/render/quad_batcher.cpp
    north/render/glyph_batcher.cpp
    north/render/renderer.cpp
    north/text/font.cpp
    north/text/font_manager.cpp
    north/text/shaper.cpp
    north/text/bidi.cpp
    north/text/line_breaker.cpp
    north/text/glyph_atlas.cpp
    north/text/paragraph.cpp
    north/text/text_buffer.cpp
    north/platform/win32/win32_window.cpp
    north/platform/win32/win32_app.cpp
    north/style/theme.cpp
    north/event/input_dispatcher.cpp
    north/animation/curve.cpp
    north/animation/animation_manager.cpp
    north/widget/box.cpp
    north/widget/text.cpp
    north/widget/button.cpp
    north/widget/text_input.cpp
    north/widget/scroll_area.cpp
    north/widget/virtual_list.cpp
    north/widget/checkbox.cpp
    north/widget/switch.cpp
    north/widget/slider.cpp
    north/widget/radio_button.cpp
)

target_include_directories(north PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(north PUBLIC
    north_warnings
    d3d11
    dxgi
    dwrite
    imm32
    Freetype::Freetype
    harfbuzz::harfbuzz
    sheenbidi
    libunibreak
)
