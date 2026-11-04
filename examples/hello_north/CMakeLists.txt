add_executable(hello_north
    main.cpp
)

target_include_directories(hello_north
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(hello_north
    PRIVATE
        north
        north_warnings
)
