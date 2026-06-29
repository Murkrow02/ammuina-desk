idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES sensors net config forecast cluster shared
    PRIV_REQUIRES esp_driver_gpio
)
