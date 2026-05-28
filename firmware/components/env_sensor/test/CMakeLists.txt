idf_component_register(
    SRCS
        "test_sensor_provider.c"
        "test_sensor_override.c"
    INCLUDE_DIRS "."
    REQUIRES env_sensor unity
)
