idf_component_register(
    SRCS
        "test_app_state.c"
        "test_storage_config.c"
        "test_power_mode.c"
    INCLUDE_DIRS "."
    REQUIRES app_core unity
    WHOLE_ARCHIVE
)
