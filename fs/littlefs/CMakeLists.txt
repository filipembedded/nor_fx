# norfx_lfs is only available if the consumer provides a `lfs` target.
# This file is only included from the root when that target exists.

add_library(norfx_lfs STATIC
    nor_flash_lfs.c
)

target_include_directories(norfx_lfs
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(norfx_lfs
    PUBLIC
        norfx_driver
        lfs
)
