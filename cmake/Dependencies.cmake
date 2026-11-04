set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries for dependencies" FORCE)

# FreeType
set(FT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/freetype")
if(NOT EXISTS "${FT_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "Missing submodule in third_party/freetype.\n"
    "Run: git submodule update --init --recursive\n"
    "Or execute: powershell -ExecutionPolicy Bypass -File scripts/setup_submodules.ps1"
  )
endif()

set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "Disable HarfBuzz in FreeType" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "Disable ZLIB in FreeType" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "Disable BZIP2 in FreeType" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "Disable PNG in FreeType" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "Disable Brotli in FreeType" FORCE)
set(FT_ENABLE_ERROR_STRINGS OFF CACHE BOOL "Disable verbose error strings" FORCE)

add_subdirectory(third_party/freetype EXCLUDE_FROM_ALL SYSTEM)

if(NOT TARGET Freetype::Freetype AND TARGET freetype)
  add_library(Freetype::Freetype ALIAS freetype)
endif()
if(NOT TARGET freetype::freetype AND TARGET freetype)
  add_library(freetype::freetype ALIAS freetype)
endif()

# HarfBuzz
set(HB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/harfbuzz")
if(NOT EXISTS "${HB_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "Missing submodule in third_party/harfbuzz.\n"
    "Run: git submodule update --init --recursive\n"
    "Or execute: powershell -ExecutionPolicy Bypass -File scripts/setup_submodules.ps1"
  )
endif()

set(HB_HAVE_FREETYPE ON CACHE BOOL "Enable FreeType in HarfBuzz" FORCE)
set(HB_BUILD_UTILS OFF CACHE BOOL "Disable HarfBuzz command-line utilities" FORCE)
set(HB_BUILD_SUBSET ON CACHE BOOL "Build HarfBuzz subsetting library" FORCE)
set(HB_HAVE_GLIB OFF CACHE BOOL "Disable GLib in HarfBuzz" FORCE)
set(HB_HAVE_GOBJECT OFF CACHE BOOL "Disable GObject in HarfBuzz" FORCE)
set(HB_HAVE_ICU OFF CACHE BOOL "Disable ICU in HarfBuzz" FORCE)
set(HB_HAVE_CAIRO OFF CACHE BOOL "Disable Cairo in HarfBuzz" FORCE)
set(HB_BUILD_TESTS OFF CACHE BOOL "Disable HarfBuzz tests" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "Skip HarfBuzz installation" FORCE)

add_subdirectory(third_party/harfbuzz EXCLUDE_FROM_ALL SYSTEM)

if(NOT TARGET harfbuzz::harfbuzz AND TARGET harfbuzz)
  add_library(harfbuzz::harfbuzz ALIAS harfbuzz)
endif()

# SheenBidi
set(SHEENBIDI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sheenbidi")
if(NOT EXISTS "${SHEENBIDI_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "Missing submodule in third_party/sheenbidi.\n"
    "Run: git submodule update --init --recursive\n"
    "Or execute: powershell -ExecutionPolicy Bypass -File scripts/setup_submodules.ps1"
  )
endif()

add_subdirectory(third_party/sheenbidi EXCLUDE_FROM_ALL SYSTEM)

if(NOT TARGET sheenbidi AND TARGET SheenBidi)
  add_library(sheenbidi ALIAS SheenBidi)
endif()
if(NOT TARGET sheenbidi::sheenbidi AND TARGET SheenBidi)
  add_library(sheenbidi::sheenbidi ALIAS SheenBidi)
endif()

# libunibreak
set(UNIBREAK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libunibreak")
if(NOT EXISTS "${UNIBREAK_DIR}/src/linebreak.c")
  message(FATAL_ERROR
    "Missing submodule in third_party/libunibreak.\n"
    "Run: git submodule update --init --recursive\n"
    "Or execute: powershell -ExecutionPolicy Bypass -File scripts/setup_submodules.ps1"
  )
endif()

if(EXISTS "${UNIBREAK_DIR}/CMakeLists.txt")
  add_subdirectory(third_party/libunibreak EXCLUDE_FROM_ALL SYSTEM)
else()
  set(LIBUNIBREAK_SOURCES
    "${UNIBREAK_DIR}/src/unibreakbase.c"
    "${UNIBREAK_DIR}/src/unibreakdef.c"
    "${UNIBREAK_DIR}/src/linebreak.c"
    "${UNIBREAK_DIR}/src/linebreakdata.c"
    "${UNIBREAK_DIR}/src/linebreakdef.c"
    "${UNIBREAK_DIR}/src/eastasianwidthdef.c"
    "${UNIBREAK_DIR}/src/emojidef.c"
    "${UNIBREAK_DIR}/src/graphemebreak.c"
    "${UNIBREAK_DIR}/src/wordbreak.c"
  )

  add_library(libunibreak STATIC ${LIBUNIBREAK_SOURCES})
  set_target_properties(libunibreak PROPERTIES
    SYSTEM ON
    FOLDER "ThirdParty"
  )
  target_include_directories(libunibreak SYSTEM PUBLIC
    $<BUILD_INTERFACE:${UNIBREAK_DIR}/src>
  )
  if(MSVC)
    target_compile_options(libunibreak PRIVATE /W0)
  else()
    target_compile_options(libunibreak PRIVATE -w)
  endif()

  add_library(libunibreak::libunibreak ALIAS libunibreak)
  add_library(unibreak ALIAS libunibreak)
endif()

# GoogleTest
if(BUILD_TESTING)
  set(GTEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest")
  if(NOT EXISTS "${GTEST_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
      "Missing submodule in third_party/googletest.\n"
      "Run: git submodule update --init --recursive\n"
      "Or execute: powershell -ExecutionPolicy Bypass -File scripts/setup_submodules.ps1"
    )
  endif()

  set(BUILD_GMOCK ON CACHE BOOL "Build GoogleMock" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "Disable GoogleTest installation" FORCE)
  set(gtest_force_shared_crt ON CACHE BOOL "Force shared CRT for GoogleTest" FORCE)

  add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL SYSTEM)

  if(NOT TARGET gtest::gtest AND TARGET gtest)
    add_library(gtest::gtest ALIAS gtest)
  endif()
  if(NOT TARGET gtest::gtest_main AND TARGET gtest_main)
    add_library(gtest::gtest_main ALIAS gtest_main)
  endif()
endif()

# ==============================================================================
# Third-Party Dependencies Meta-Target
# ==============================================================================
# Ensures all third-party static libraries are built during default build invocations
# without removing EXCLUDE_FROM_ALL warning and target isolation.
add_custom_target(north_third_party ALL
    COMMENT "Building all third-party static libraries"
)
add_dependencies(north_third_party
    freetype
    harfbuzz
    SheenBidi
    libunibreak
)
