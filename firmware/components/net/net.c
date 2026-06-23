// net.c — Fase 03: WiFi station + telemetria HTTP verso il proxy.
// Stile basato sui template del corso (wifi_init_sta, event group, cJSON).
#include "net.h"

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
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "cJSON.h"

// Credenziali ed endpoint configurabili via `idf.py menuconfig` -> Network.
// (Default: SSID "Wokwi-GUEST", proxy sul gateway Wokwi 10.13.37.2.)
#define WIFI_SSID           CONFIG_WIFI_SSID
#define WIFI_PASSWORD       CONFIG_WIFI_PASSWORD
#define DESK_ID             CONFIG_DESK_ID
#define PROXY_TELEMETRY_URL CONFIG_PROXY_TELEMETRY_URL

static const char *TAG = "ESP32_NET";

static EventGroupHandle_t wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;


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

    // Rete aperta (Wokwi-GUEST) se la password e' vuota, altrimenti WPA2.
    wifi_config.sta.threshold.authmode =
        (strlen(WIFI_PASSWORD) == 0) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

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

// Riduce la finestra a media noise/light e occupazione di maggioranza, poi
// confeziona il JSON di telemetria. Ritorna stringa malloc'd: il chiamante free().
static char* create_json(const network_payload_t *window)
{
    float sum_noise = 0.0f, sum_light = 0.0f;
    int occupied_count = 0;
    int n = window->total_samples > 0 ? window->total_samples : 1;

    for (int i = 0; i < window->total_samples; i++) {
        sum_noise += window->samples[i].noise_level;
        sum_light += window->samples[i].lux;
        if (window->samples[i].occupancy) occupied_count++;
    }

    bool occupied = (occupied_count * 2 >= window->total_samples); // maggioranza

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "desk_id", DESK_ID);
    cJSON_AddBoolToObject(root, "occupied", occupied);
    // Durata sessione: arrivera' con la logica di fase 02, per ora 0.
    cJSON_AddNumberToObject(root, "session_duration_s", 0);
    cJSON_AddNumberToObject(root, "noise", sum_noise / n);
    cJSON_AddNumberToObject(root, "light", sum_light / n);
    cJSON_AddNumberToObject(root, "samples", window->total_samples);

    char *json_string = cJSON_PrintUnformatted(root);

    cJSON_Delete(root);

    return json_string;
}


/* ---------------- HTTP telemetry ---------------- */

// Client riutilizzato fra invii: con keep-alive l'handshake TLS (costoso sul
// gateway pubblico Wokwi) avviene una sola volta, poi la connessione si riusa.
static esp_http_client_handle_t s_client = NULL;

static esp_http_client_handle_t get_client(void)
{
    if (s_client != NULL) return s_client;

    esp_http_client_config_t config = {
        .url = PROXY_TELEMETRY_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000,
        .keep_alive_enable = true,
    };
    s_client = esp_http_client_init(&config);
    if (s_client) {
        esp_http_client_set_header(s_client, "Content-Type", "application/json");
        // ngrok free mostra una pagina di warning ai browser: questo header la salta.
        esp_http_client_set_header(s_client, "ngrok-skip-browser-warning", "1");
    }
    return s_client;
}

void net_telemetry_send(const network_payload_t *window)
{
    char *payload = create_json(window);
    if (payload == NULL) {
        ESP_LOGE(TAG, "Failed to build telemetry JSON");
        return;
    }

    esp_http_client_handle_t client = get_client();
    if (client == NULL) {
        ESP_LOGE(TAG, "esp_http_client_init failed");
        free(payload);
        return;
    }

    esp_http_client_set_post_field(client, payload, strlen(payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "POST telemetry -> HTTP %d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGW(TAG, "POST telemetry failed: %s", esp_err_to_name(err));
        // Connessione probabilmente morta: chiudila cosi' il prossimo invio
        // rifa' init + handshake da zero.
        esp_http_client_cleanup(client);
        s_client = NULL;
    }

    free(payload);
}

void net_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();
}
