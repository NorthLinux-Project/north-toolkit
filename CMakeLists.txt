cmake_minimum_required(VERSION 3.25)
project(north-toolkit LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(NORTH_ENABLE_EXCEPTIONS "Enable C++ exception handling" OFF)
option(NORTH_ENABLE_RTTI "Enable C++ runtime type information (RTTI)" OFF)

if(MSVC)
    if(NOT NORTH_ENABLE_EXCEPTIONS)
        string(REGEX REPLACE "[-/]EH[a-z]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    endif()
    if(NOT NORTH_ENABLE_RTTI)
        string(REGEX REPLACE "[-/]GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    endif()
endif()

include(cmake/CompilerWarnings.cmake)
include(cmake/Dependencies.cmake)

add_subdirectory(src)
add_subdirectory(examples)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
