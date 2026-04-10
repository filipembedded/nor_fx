# This target is only reachable when CMAKE_CROSSCOMPILING is ON.
# The root CMakeLists.txt enforces that guard before including this directory.
#
# The consumer (parent firmware project) is responsible for providing the
# STM32 HAL include path via an INTERFACE or IMPORTED target, e.g.:
#
#   target_link_libraries(norfx_port_stm32 PRIVATE stm32_hal)
#
# or by appending to target_include_directories below.

add_library(norfx_port_stm32 STATIC
    port_stm32.c
)

target_include_directories(norfx_port_stm32
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(norfx_port_stm32
    PUBLIC
        norfx_driver
    PRIVATE
        # stm32_hal   # <-- uncomment and set by parent project
)
