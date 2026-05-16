idf_component_register(
    SRCS
        "ble_env_service.c"
    INCLUDE_DIRS "include"
    REQUIRES bt app_core env_sensor
)
