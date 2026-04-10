# ---------------------------------------------------------------------------
# Unity test framework
# ---------------------------------------------------------------------------
# Expected as a git submodule at tests/unity/.
# Add it with:
#   git submodule add https://github.com/ThrowTheSwitch/Unity.git tests/unity

add_library(unity STATIC
    unity/src/unity.c
)

target_include_directories(unity
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/unity/src
)

# Disable warnings we do not control in third-party code.
target_compile_options(unity
    PRIVATE
        -w
)

# ---------------------------------------------------------------------------
# Helper function — reduces boilerplate for each test executable
#
# Usage:
#   norfx_add_test(test_norfx_reset test_norfx_reset.c)
# ---------------------------------------------------------------------------
function(norfx_add_test target source)
    add_executable(${target} ${source})

    target_link_libraries(${target}
        PRIVATE
            norfx_driver
            norfx_port_linux
            unity
    )

    # Register with CTest — one test per executable.
    add_test(
        NAME    ${target}
        COMMAND ${target}
    )

    # Mark the test as failed if any output goes to stderr.
    set_tests_properties(${target}
        PROPERTIES
            FAIL_REGULAR_EXPRESSION "FAIL"
    )
endfunction()

# ---------------------------------------------------------------------------
# Test executables
# ---------------------------------------------------------------------------
norfx_add_test(test_norfx_reset         test_norfx_reset.c)
norfx_add_test(test_norfx_read          test_norfx_read.c)
norfx_add_test(test_norfx_fast_read     test_norfx_fast_read.c)
norfx_add_test(test_norfx_page_program  test_norfx_page_program.c)
norfx_add_test(test_norfx_erase_sector  test_norfx_erase_sector.c)
norfx_add_test(test_norfx_write         test_norfx_write.c)
