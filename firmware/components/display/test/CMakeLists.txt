# Phase 1.5 will populate these tests once display.h is added.
# Placeholder so the test directory is tracked by git.
idf_component_register(
    SRCS
        "test_display_pending.c"
        "test_display_logic.c"
    INCLUDE_DIRS "."
    REQUIRES display unity
)
