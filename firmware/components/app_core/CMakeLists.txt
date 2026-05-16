idf_component_register(
    SRCS
        "app_state.c"
        "storage_config.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash
)
