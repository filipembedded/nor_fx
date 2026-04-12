# ---------------------------------------------------------------------------
# STM32 port — series selection
#
# Set NORFX_STM32_SERIES before adding this library, e.g.:
#
#   cmake -DNORFX_STM32_SERIES=F4 ..
#
# Supported values: F4
# ---------------------------------------------------------------------------

if(NOT DEFINED NORFX_STM32_SERIES)
    message(FATAL_ERROR
        "[nor_fx] NORFX_STM32_SERIES is not set. "
        "Pass -DNORFX_STM32_SERIES=<series> (e.g. F4) to CMake."
    )
endif()

string(TOUPPER "${NORFX_STM32_SERIES}" _NORFX_SERIES)

if(_NORFX_SERIES STREQUAL "F4")
    add_subdirectory(f4)
else()
    message(FATAL_ERROR
        "[nor_fx] Unsupported NORFX_STM32_SERIES='${NORFX_STM32_SERIES}'. "
        "Supported: F4"
    )
endif()
