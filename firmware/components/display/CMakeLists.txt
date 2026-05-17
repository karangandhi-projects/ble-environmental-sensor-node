idf_component_register(
    SRCS "ssd1306.c" "display.c" "font_big.c"
    INCLUDE_DIRS "include"
    REQUIRES driver app_core env_sensor
)
