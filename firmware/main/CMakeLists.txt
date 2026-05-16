idf_component_register(
    SRCS
        "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES app_core ble_env env_sensor display
)
