# ---------------------------------------------------------------------------
# Legacy STM32 dispatcher
#
# Preferred usage today is to enable a concrete port explicitly from the root
# project, e.g. NORFX_BUILD_PORT_STM32_F4=ON. This file is kept only as a
# compatibility shim for consumers that still include port/stm32 directly.
# ---------------------------------------------------------------------------

if(DEFINED NORFX_STM32_SERIES)
    string(TOUPPER "${NORFX_STM32_SERIES}" _NORFX_SERIES)

    if(_NORFX_SERIES STREQUAL "F4")
        add_subdirectory(f4)
    else()
        message(FATAL_ERROR
            "[nor_fx] Unsupported NORFX_STM32_SERIES='${NORFX_STM32_SERIES}'. "
            "Supported by this compatibility dispatcher: F4"
        )
    endif()
elseif(NORFX_BUILD_PORT_STM32_F4)
    add_subdirectory(f4)
else()
    message(FATAL_ERROR
        "[nor_fx] No STM32 port selected. Prefer enabling NORFX_BUILD_PORT_STM32_F4 "
        "from the root project, or set NORFX_STM32_SERIES=F4 when using this "
        "legacy dispatcher directly."
    )
endif()
