# This target is only reachable when CMAKE_CROSSCOMPILING is OFF.
# The root CMakeLists.txt enforces that guard before including this directory.

add_library(norfx_port_linux STATIC
    port_linux.c
)

target_include_directories(norfx_port_linux
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(norfx_port_linux
    PUBLIC
        norfx_driver
)
