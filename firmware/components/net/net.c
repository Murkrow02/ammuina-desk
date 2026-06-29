// net.c — Fase 04: WiFi station + dispatch telemetria su trasporto HTTP/CoAP.
// Lo specifico trasporto vive in net_http.c / net_coap.c; qui sta la connessione
// WiFi, la costruzione del payload condiviso e la selezione del communication_mode.
#include "net.h"
#include "net_internal.h"
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

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
#include "cJSON.h"

// Credenziali ed endpoint configurabili via `idf.py menuconfig` -> Network.
#define WIFI_SSID     CONFIG_WIFI_SSID
#define WIFI_PASSWORD CONFIG_WIFI_PASSWORD
#define DESK_ID       CONFIG_DESK_ID

static const char *TAG = "ESP32_NET";

static EventGroupHandle_t wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;

// Il trasporto attivo vive nella config runtime (config/): single source of truth,
// cosi' net_set_communication_mode() (fase 04) e l'update MQTT (fase 05) scrivono
// lo stesso campo. Il default e' caricato da config_init().


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


/* ---------------- communication_mode ---------------- */

void net_set_communication_mode(communication_mode_t mode)
{
    if (mode != COMM_MODE_HTTP && mode != COMM_MODE_COAP) {
        ESP_LOGW(TAG, "communication_mode sconosciuto (%d), ignorato", mode);
        return;
    }
    config_set_communication_mode(mode);  // log del cambio nella config
}

communication_mode_t net_get_communication_mode(void)
{
    return config_get().communication_mode;
}


/* ---------------- Payload telemetria (condiviso fra i trasporti) ---------------- */

// Riduce la finestra a media noise/light e occupazione di maggioranza, poi
// confeziona il JSON di telemetria. Ritorna stringa malloc'd: il chiamante free().
char *net_build_telemetry_json(const network_payload_t *window)
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


/* ---------------- Dispatch telemetria ---------------- */

void net_telemetry_send(const network_payload_t *window)
{
    char *payload = net_build_telemetry_json(window);
    if (payload == NULL) {
        ESP_LOGE(TAG, "Failed to build telemetry JSON");
        return;
    }

    communication_mode_t mode = config_get().communication_mode;
    bool ok = (mode == COMM_MODE_COAP) ? net_coap_send(payload)
                                       : net_http_send(payload);
    if (!ok) {
        ESP_LOGW(TAG, "Invio telemetria fallito (%s)",
                 mode == COMM_MODE_COAP ? "CoAP" : "HTTP");
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

    config_init();   // carica i default runtime prima che i task li leggano
    wifi_init_sta();
}
