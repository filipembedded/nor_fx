cmake_minimum_required(VERSION 3.21)

project(
    nor_flash_xplat
    VERSION 1.0.0
    DESCRIPTION "Cross-platform NOR flash driver"
    LANGUAGES C
)

# ---------------------------------------------------------------------------
# Build options
#
# Design goals:
# - Standalone mode (top-level project): easy host-side development/testing.
# - Consumer mode   (submodule/parent project): build only explicitly selected
#   optional components.
# ---------------------------------------------------------------------------
if(PROJECT_IS_TOP_LEVEL)
    set(_NORFX_DEFAULT_BUILD_LFS ON)
    if(UNIX AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(_NORFX_DEFAULT_BUILD_PORT_NATIVE_LINUX ON)
        set(_NORFX_DEFAULT_BUILD_TESTS ON)
    else()
        set(_NORFX_DEFAULT_BUILD_PORT_NATIVE_LINUX OFF)
        set(_NORFX_DEFAULT_BUILD_TESTS OFF)
    endif()
    set(_NORFX_DEFAULT_BUILD_PORT_STM32_F4 OFF)
else()
    set(_NORFX_DEFAULT_BUILD_LFS OFF)
    set(_NORFX_DEFAULT_BUILD_PORT_NATIVE_LINUX OFF)
    set(_NORFX_DEFAULT_BUILD_PORT_STM32_F4 OFF)
    set(_NORFX_DEFAULT_BUILD_TESTS OFF)
endif()

option(NORFX_BUILD_LFS "Build the littlefs adapter target (norfx_lfs)." ${_NORFX_DEFAULT_BUILD_LFS})
option(NORFX_BUILD_PORT_NATIVE_LINUX "Build the Linux simulation port." ${_NORFX_DEFAULT_BUILD_PORT_NATIVE_LINUX})
option(NORFX_BUILD_PORT_STM32_F4 "Build the STM32F4 port." ${_NORFX_DEFAULT_BUILD_PORT_STM32_F4})
option(NORFX_BUILD_TESTS "Build host-side Unity/CTest tests." ${_NORFX_DEFAULT_BUILD_TESTS})

# ---------------------------------------------------------------------------
# Global C standard — enforce C11, no compiler extensions
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Compiler warnings — applied to all targets defined in this project
# ---------------------------------------------------------------------------
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wundef
    -Wdouble-promotion
    -fno-common
)

# ---------------------------------------------------------------------------
# Subdirectories — always included
# ---------------------------------------------------------------------------
add_subdirectory(driver)

# littlefs — explicit optional component
if(NORFX_BUILD_LFS)
    # Build littlefs from the sibling submodule when developing the library
    # standalone. In parent projects, prefer providing an existing `lfs`
    # target and enabling NORFX_BUILD_LFS explicitly.
    if(NOT TARGET lfs)
        if(PROJECT_IS_TOP_LEVEL)
            add_library(lfs STATIC
                ${CMAKE_CURRENT_SOURCE_DIR}/../littlefs/lfs.c
                ${CMAKE_CURRENT_SOURCE_DIR}/../littlefs/lfs_util.c
            )
            target_include_directories(lfs PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}/../littlefs
            )
            target_compile_options(lfs PRIVATE -w)
        else()
            message(FATAL_ERROR
                "[nor_fx] NORFX_BUILD_LFS=ON requires an existing 'lfs' target "
                "when nor_fx is used from a parent project."
            )
        endif()
    endif()

    add_subdirectory(fs/littlefs)
endif()

# ---------------------------------------------------------------------------
# Optional ports — explicitly selected
# ---------------------------------------------------------------------------
# Host-side simulation port for tests and local development.
if(NORFX_BUILD_PORT_NATIVE_LINUX)
    if(NOT (UNIX AND CMAKE_SYSTEM_NAME STREQUAL "Linux"))
        message(FATAL_ERROR
            "[nor_fx] NORFX_BUILD_PORT_NATIVE_LINUX=ON is only supported on Linux hosts."
        )
    endif()

    add_subdirectory(port/native_sim/linux)
endif()

# Bare-metal STM32F4 port.
if(NORFX_BUILD_PORT_STM32_F4)
    add_subdirectory(port/stm32/f4)
endif()

# ---------------------------------------------------------------------------
# Tests — explicit optional component
# ---------------------------------------------------------------------------
if(NORFX_BUILD_TESTS)
    if(NOT NORFX_BUILD_PORT_NATIVE_LINUX)
        message(FATAL_ERROR
            "[nor_fx] NORFX_BUILD_TESTS=ON requires NORFX_BUILD_PORT_NATIVE_LINUX=ON."
        )
    endif()

    enable_testing()
    add_subdirectory(tests)
endif()
