idf_component_register(
    SRCS
        "test_ble_env_encode.c"
    INCLUDE_DIRS "."
    REQUIRES ble_env app_core env_sensor unity
    WHOLE_ARCHIVE
)
