idf_component_register(
    SRCS "net.c"
    INCLUDE_DIRS "include"
    REQUIRES shared esp_wifi esp_event esp_netif esp_http_client nvs_flash json
)
