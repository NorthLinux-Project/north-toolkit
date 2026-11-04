add_library(north_warnings INTERFACE)

if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC" OR (NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT AND MSVC))
    target_compile_options(north_warnings INTERFACE /W4 /WX)
    if(NOT NORTH_ENABLE_EXCEPTIONS)
        target_compile_options(north_warnings INTERFACE /EHs-c-)
        target_compile_definitions(north_warnings INTERFACE _HAS_EXCEPTIONS=0)
    endif()
    if(NOT NORTH_ENABLE_RTTI)
        target_compile_options(north_warnings INTERFACE /GR-)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(north_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
    )
    if(NOT NORTH_ENABLE_EXCEPTIONS)
        target_compile_options(north_warnings INTERFACE -fno-exceptions)
        if(WIN32)
            target_compile_definitions(north_warnings INTERFACE _HAS_EXCEPTIONS=0)
        endif()
    endif()
    if(NOT NORTH_ENABLE_RTTI)
        target_compile_options(north_warnings INTERFACE -fno-rtti)
    endif()
endif()
