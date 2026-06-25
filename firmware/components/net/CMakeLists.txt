idf_component_register(
    SRCS "net.c" "net_http.c" "net_coap.c"
    INCLUDE_DIRS "include"
    REQUIRES shared esp_wifi esp_event esp_netif esp_http_client nvs_flash json coap lwip esp_timer
)
