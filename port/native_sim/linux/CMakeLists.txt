# This directory is included explicitly from the root CMakeLists.txt when
# NORFX_BUILD_PORT_NATIVE_LINUX=ON.

add_library(norfx_port_linux STATIC
    port_linux.c
)

# Clearer public name retained alongside the original target for compatibility.
add_library(norfx_port_native_linux ALIAS norfx_port_linux)

target_include_directories(norfx_port_linux
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(norfx_port_linux
    PUBLIC
        norfx_driver
)
