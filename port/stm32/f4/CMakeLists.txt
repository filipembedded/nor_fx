# ---------------------------------------------------------------------------
# norfx_port_stm32 — STM32F4 series port
#
# Requires the parent project to provide an stm32_hal INTERFACE target (or
# equivalent) that exposes the STM32F4xx HAL headers. Example:
#
#   add_library(stm32_hal INTERFACE)
#   target_include_directories(stm32_hal INTERFACE
#       Drivers/STM32F4xx_HAL_Driver/Inc
#       Drivers/CMSIS/Device/ST/STM32F4xx/Include
#       Drivers/CMSIS/Include
#   )
#   target_compile_definitions(stm32_hal INTERFACE STM32F411xE)  # adjust MCU
#
# Then link: target_link_libraries(norfx_port_stm32 PRIVATE stm32_hal)
# ---------------------------------------------------------------------------

add_library(norfx_port_stm32 STATIC
    stm32f4x_port.c
)

target_include_directories(norfx_port_stm32
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(norfx_port_stm32
    PUBLIC
        norfx_driver
    PRIVATE
        # stm32_hal   # <-- provide this target from your firmware project
)
