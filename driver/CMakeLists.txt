add_library(norfx_driver STATIC
    nor_fx.c
)

target_include_directories(norfx_driver
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(norfx_driver
    PUBLIC
        c_std_11
)
