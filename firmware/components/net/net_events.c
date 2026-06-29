// net_events.c — Fase 05: rilevazione eventi on-device + pubblicazione MQTT.
// A ogni finestra aggregata calcola media noise/light e occupazione di maggioranza,
// confronta con le soglie di config (config/) e pubblica gli eventi rilevati su
// desk/<id>/event. Mantiene lo stato fra finestre per:
//   - macchina occupazione: "desk occupied" alla presa, "desk released" dopo
//     occupancy_timeout_s di inattivita';
//   - "high noise" / "poor lighting": edge-triggered (un solo evento al
//     superamento soglia, niente spam finche' la condizione persiste).
// Cambiando una soglia via MQTT, l'effetto si vede dalla finestra successiva.
#include "net.h"
#include "net_internal.h"
#include "config.h"

#include <stdlib.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"

#define DESK_ID CONFIG_DESK_ID

static const char *TAG = "ESP32_EVENTS";

// Stato persistente fra finestre.
static bool    s_occupied      = false;  // stato occupazione corrente del desk
static int64_t s_idle_since_us = 0;      // ultimo istante di occupazione (per timeout)
static bool    s_noise_high    = false;  // edge "high noise" gia' notificato
static bool    s_light_poor    = false;  // edge "poor lighting" gia' notificato

static void publish_event(const char *name, float noise, float light, bool occupied)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "desk_id", DESK_ID);
    cJSON_AddStringToObject(root, "event", name);
    // Uptime in ms: niente SNTP a bordo, il proxy aggiunge il timestamp server-side.
    cJSON_AddNumberToObject(root, "ts_ms", (double)(esp_timer_get_time() / 1000));
    cJSON_AddNumberToObject(root, "noise", noise);
    cJSON_AddNumberToObject(root, "light", light);
    cJSON_AddBoolToObject(root, "occupied", occupied);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        ESP_LOGE(TAG, "build evento JSON fallito");
        return;
    }
    net_mqtt_publish_event(json);
    free(json);
}

void net_events_eval(const network_payload_t *window)
{
    node_config_t cfg = config_get();

    float sum_noise = 0.0f, sum_light = 0.0f;
    int occupied_count = 0;
    int n = window->total_samples > 0 ? window->total_samples : 1;
    for (int i = 0; i < window->total_samples; i++) {
        sum_noise += window->samples[i].noise_level;
        sum_light += window->samples[i].lux;
        if (window->samples[i].occupancy) occupied_count++;
    }
    float avg_noise = sum_noise / n;
    float avg_light = sum_light / n;
    bool  win_occupied = (occupied_count * 2 >= window->total_samples); // maggioranza

    int64_t now = esp_timer_get_time();

    /* --- occupazione: macchina a stati con occupancy_timeout --- */
    if (win_occupied) {
        s_idle_since_us = now;
        if (!s_occupied) {
            s_occupied = true;
            publish_event("desk occupied", avg_noise, avg_light, true);
        }
    } else if (s_occupied) {
        int64_t idle_s = (now - s_idle_since_us) / 1000000;
        if (idle_s >= cfg.occupancy_timeout_s) {
            s_occupied = false;
            publish_event("desk released", avg_noise, avg_light, false);
        }
    }

    /* --- high noise: edge-triggered sul superamento di noise_thr --- */
    if (avg_noise > cfg.noise_thr) {
        if (!s_noise_high) {
            s_noise_high = true;
            publish_event("high noise", avg_noise, avg_light, s_occupied);
        }
    } else {
        s_noise_high = false;
    }

    /* --- poor lighting: edge-triggered sotto light_thr --- */
    if (avg_light < cfg.light_thr) {
        if (!s_light_poor) {
            s_light_poor = true;
            publish_event("poor lighting", avg_noise, avg_light, s_occupied);
        }
    } else {
        s_light_poor = false;
    }
}
