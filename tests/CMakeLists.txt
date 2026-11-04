cmake_minimum_required(VERSION 3.25)

enable_testing()

include(GoogleTest)

# Option to build unpopulated blueprint test stubs for future milestones (M2-M7).
# Disabled by default so that only genuine milestone test suites are built and verified.
option(NORTH_BUILD_BLUEPRINT_TESTS "Compile skeleton blueprint test stubs for future milestones" OFF)

# Primary unit test executable
# Compiles authentic, fully-implemented tests for active milestone deliverables.
set(UNIT_TEST_SOURCES
    unit/test_smoke.cpp
    unit/test_assert.cpp
    unit/test_types.cpp
    unit/test_arena.cpp
    unit/test_slab_pool.cpp
    unit/test_constraints.cpp
    unit/test_signal.cpp
    unit/test_node_tree.cpp
    unit/test_flex_layout.cpp
    unit/test_stack_layout.cpp
    unit/test_gpu_device.cpp
    unit/test_display_list.cpp
    unit/test_quad_batcher.cpp
    unit/test_renderer.cpp
    unit/test_font.cpp
    unit/test_shaper.cpp
    unit/test_bidi_and_breaker.cpp
    unit/test_glyph_atlas.cpp
    unit/test_paragraph.cpp
    unit/test_theme.cpp
    unit/test_win32_window.cpp
    unit/test_win32_app.cpp
    unit/test_widgets.cpp
    unit/test_text_buffer.cpp
    unit/test_text_input.cpp
    unit/test_scroll_area.cpp
    unit/test_virtual_list.cpp
    unit/test_animation.cpp
    unit/test_form_controls.cpp
)

add_executable(unit_tests ${UNIT_TEST_SOURCES})

target_include_directories(unit_tests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(unit_tests
    PRIVATE
        north
        north_warnings
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
)

gtest_discover_tests(unit_tests
    PROPERTIES
        LABELS "unit;m1"
        DISCOVERY_TIMEOUT 30
)

# Future Milestone Blueprint Test Stubs (M2-M7)
# When enabled, compiles future skeleton files into an isolated target with distinct labels.
if(NORTH_BUILD_BLUEPRINT_TESTS)
    set(BLUEPRINT_UNIT_SOURCES
        unit/test_display_list.cpp
        unit/test_paragraph.cpp
    )

    add_executable(blueprint_unit_tests ${BLUEPRINT_UNIT_SOURCES})
    target_include_directories(blueprint_unit_tests
        PRIVATE
            "${CMAKE_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(blueprint_unit_tests
        PRIVATE
            north
            north_warnings
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
    )
    gtest_discover_tests(blueprint_unit_tests
        PROPERTIES
            LABELS "blueprint"
            DISCOVERY_TIMEOUT 30
    )

    file(GLOB BLUEPRINT_E2E_SOURCES CONFIGURE_DEPENDS "e2e/*.cpp")
    if(BLUEPRINT_E2E_SOURCES)
        add_executable(blueprint_e2e_tests ${BLUEPRINT_E2E_SOURCES})
        target_include_directories(blueprint_e2e_tests
            PRIVATE
                "${CMAKE_SOURCE_DIR}/src"
                "${CMAKE_CURRENT_SOURCE_DIR}"
        )
        target_link_libraries(blueprint_e2e_tests
            PRIVATE
                north
                north_warnings
                GTest::gtest
                GTest::gmock
                GTest::gtest_main
        )
        gtest_discover_tests(blueprint_e2e_tests
            PROPERTIES
                LABELS "blueprint;e2e"
                DISCOVERY_TIMEOUT 30
        )
    endif()
endif()
