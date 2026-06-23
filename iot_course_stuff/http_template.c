#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_err.h"

#include "driver/temperature_sensor.h"
#include "esp_http_server.h"
#include "cJSON.h"

// CUSTOMIZE
#define WIFI_SSID          "xxx"
#define WIFI_PASSWORD      "xxx"

static const char *TAG = "ESP32_WEB_SERVER";

static EventGroupHandle_t wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;

static httpd_handle_t server = NULL;


/* ---------------- Wi-Fi management ---------------- */

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi started, connecting...");
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi disconnected, retrying...");
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &wifi_event_handler,
                                               NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler,
                                               NULL));

    wifi_config_t wifi_config = { 0 };

    strncpy((char *)wifi_config.sta.ssid,
            WIFI_SSID,
            sizeof(wifi_config.sta.ssid) - 1);

    strncpy((char *)wifi_config.sta.password,
            WIFI_PASSWORD,
            sizeof(wifi_config.sta.password) - 1);

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
    xEventGroupWaitBits(wifi_event_group,
                        WIFI_CONNECTED_BIT,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);
}

/* ---------------- Helper functions ---------------- */

static float get_internal_temperature(void)
{
    static temperature_sensor_handle_t temp_handle = NULL;

    if (temp_handle == NULL) {
        temperature_sensor_config_t cfg = {
            .range_min = 10,
            .range_max = 50,
        };

        if (temperature_sensor_install(&cfg, &temp_handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to install temperature sensor");
            return -1000.0f;
        }

        if (temperature_sensor_enable(temp_handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable temperature sensor");
            return -1000.0f;
        }
    }

    float temp = 0.0f;
    if (temperature_sensor_get_celsius(temp_handle, &temp) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read temperature");
        return -1000.0f;
    }

    return temp;
}

char* create_json(void)
{
    // Create JSON object
    cJSON *root = cJSON_CreateObject();

    // Customize!
    cJSON_AddStringToObject(root, "stringex", "esp32");
    cJSON_AddNumberToObject(root, "numberex", 23.5);
    cJSON_AddBoolToObject(root, "boolex", 1);

    char *json_string = cJSON_PrintUnformatted(root);

    cJSON_Delete(root);

    return json_string; 
}


/* ---------------- HTTP server ---------------- */


static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char *resp =
        "ESP32 Web Server is running.\n";

    // httpd_resp_set_type(req, "application/json");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, resp);
    return ESP_OK;
}

// CUSTOMIZE!
static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_handle_t server_handle = NULL;
    if (httpd_start(&server_handle, &config) == ESP_OK) {
        httpd_uri_t root_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = root_get_handler,
            .user_ctx  = NULL
        };


        httpd_register_uri_handler(server_handle, &root_uri);
        
        ESP_LOGI(TAG, "Web server started");
        return server_handle;
    }

    ESP_LOGE(TAG, "Failed to start web server");
    return NULL;
}

/* ---------------- Main ---------------- */

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();

    server = start_webserver();
    if (server == NULL) {
        ESP_LOGE(TAG, "Server start failed");
    }
}