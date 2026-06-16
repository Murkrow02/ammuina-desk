idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES sensors net forecast cluster
    PRIV_REQUIRES esp_driver_gpio
)
