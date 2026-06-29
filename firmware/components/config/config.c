// config.c — Fase 05: storage della config runtime, protetto da mutex FreeRTOS.
// Single source of truth dei parametri del nodo: net/ legge il trasporto qui,
// il task sensing il sampling_rate, gli eventi le soglie. L'aggiornamento a caldo
// arriva da net_mqtt.c via config_update_from_cjson().
#include "config.h"

#include <string.h>
#include <strings.h>  // strcasecmp

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "config";

static node_config_t s_cfg;
static SemaphoreHandle_t s_mtx;

#if CONFIG_COMM_MODE_DEFAULT_COAP
#define DEFAULT_COMM_MODE COMM_MODE_COAP
#else
#define DEFAULT_COMM_MODE COMM_MODE_HTTP
#endif

void config_init(void)
{
    s_mtx = xSemaphoreCreateMutex();
    s_cfg = (node_config_t){
        .sampling_rate_ms    = CONFIG_NODE_SAMPLING_RATE_MS,
        .noise_thr           = CONFIG_NODE_NOISE_THR_MILLI / 1000.0f,
        .light_thr           = (float)CONFIG_NODE_LIGHT_THR,
        .occupancy_timeout_s = CONFIG_NODE_OCCUPANCY_TIMEOUT_S,
        .communication_mode  = DEFAULT_COMM_MODE,
    };
    ESP_LOGI(TAG, "config init: sampling=%dms noise_thr=%.3f light_thr=%.0f "
                  "occ_timeout=%ds mode=%s",
             s_cfg.sampling_rate_ms, s_cfg.noise_thr, s_cfg.light_thr,
             s_cfg.occupancy_timeout_s,
             s_cfg.communication_mode == COMM_MODE_COAP ? "CoAP" : "HTTP");
}

node_config_t config_get(void)
{
    node_config_t copy;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    copy = s_cfg;
    xSemaphoreGive(s_mtx);
    return copy;
}

void config_set_communication_mode(communication_mode_t mode)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    if (mode != s_cfg.communication_mode) {
        ESP_LOGI(TAG, "communication_mode -> %s",
                 mode == COMM_MODE_COAP ? "CoAP" : "HTTP");
        s_cfg.communication_mode = mode;
    }
    xSemaphoreGive(s_mtx);
}

bool config_update_from_cjson(const cJSON *root)
{
    bool changed = false;
    const cJSON *it;

    xSemaphoreTake(s_mtx, portMAX_DELAY);

    if ((it = cJSON_GetObjectItem(root, "sampling_rate")) && cJSON_IsNumber(it)) {
        int v = it->valueint;
        if (v > 0 && v != s_cfg.sampling_rate_ms) {
            s_cfg.sampling_rate_ms = v; changed = true;
            ESP_LOGI(TAG, "sampling_rate -> %d ms", v);
        }
    }
    if ((it = cJSON_GetObjectItem(root, "noise_thr")) && cJSON_IsNumber(it)) {
        float v = (float)it->valuedouble;
        if (v != s_cfg.noise_thr) {
            s_cfg.noise_thr = v; changed = true;
            ESP_LOGI(TAG, "noise_thr -> %.3f", v);
        }
    }
    if ((it = cJSON_GetObjectItem(root, "light_thr")) && cJSON_IsNumber(it)) {
        float v = (float)it->valuedouble;
        if (v != s_cfg.light_thr) {
            s_cfg.light_thr = v; changed = true;
            ESP_LOGI(TAG, "light_thr -> %.1f", v);
        }
    }
    if ((it = cJSON_GetObjectItem(root, "occupancy_timeout")) && cJSON_IsNumber(it)) {
        int v = it->valueint;
        if (v >= 0 && v != s_cfg.occupancy_timeout_s) {
            s_cfg.occupancy_timeout_s = v; changed = true;
            ESP_LOGI(TAG, "occupancy_timeout -> %d s", v);
        }
    }
    if ((it = cJSON_GetObjectItem(root, "communication_mode")) != NULL) {
        communication_mode_t m = s_cfg.communication_mode;
        if (cJSON_IsString(it) && it->valuestring) {
            if (strcasecmp(it->valuestring, "coap") == 0)      m = COMM_MODE_COAP;
            else if (strcasecmp(it->valuestring, "http") == 0) m = COMM_MODE_HTTP;
        } else if (cJSON_IsNumber(it)) {
            m = it->valueint ? COMM_MODE_COAP : COMM_MODE_HTTP;
        }
        if (m != s_cfg.communication_mode) {
            s_cfg.communication_mode = m; changed = true;
            ESP_LOGI(TAG, "communication_mode -> %s",
                     m == COMM_MODE_COAP ? "CoAP" : "HTTP");
        }
    }

    xSemaphoreGive(s_mtx);
    return changed;
}
