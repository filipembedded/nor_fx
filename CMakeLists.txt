cmake_minimum_required(VERSION 3.21)

project(
    nor_flash_xplat
    VERSION 1.0.0
    DESCRIPTION "Cross-platform NOR flash driver"
    LANGUAGES C
)

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

# littlefs — build from the sibling submodule when building standalone
if(PROJECT_IS_TOP_LEVEL AND NOT TARGET lfs)
    add_library(lfs STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../littlefs/lfs.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../littlefs/lfs_util.c
    )
    target_include_directories(lfs PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../littlefs
    )
    # Suppress warnings in third-party code
    target_compile_options(lfs PRIVATE -w)
endif()

# littlefs glue layer — only if the consumer provides a `lfs` target
if(TARGET lfs)
    add_subdirectory(fs/littlefs)
endif()

# ---------------------------------------------------------------------------
# Port selection
# ---------------------------------------------------------------------------
if(CMAKE_CROSSCOMPILING)
    # Bare-metal target (e.g. STM32): STM32 HAL port
    add_subdirectory(port/stm32)
else()
    # Host build (Linux): simulation port
    add_subdirectory(port/native_sim/linux)
endif()

# ---------------------------------------------------------------------------
# Tests — only when this is the top-level project (not a submodule)
# ---------------------------------------------------------------------------
if(PROJECT_IS_TOP_LEVEL)
    enable_testing()
    add_subdirectory(tests)
endif()
