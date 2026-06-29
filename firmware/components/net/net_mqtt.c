// net_mqtt.c — Fase 05: client MQTT (esp-mqtt).
// Pubblica gli eventi del nodo su desk/<id>/event e si sottoscrive a
// desk/<id>/config per aggiornare la config runtime a caldo (senza riavvio).
// La config arrivata viene applicata via config_update_from_cjson(): essendo la
// config la single source of truth, il cambio di soglie/sampling/trasporto ha
// effetto dal ciclo di sensing successivo.
#include "net.h"
#include "net_internal.h"
#include "config.h"

#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "cJSON.h"

#define MQTT_BROKER_URI CONFIG_MQTT_BROKER_URI
#define DESK_ID         CONFIG_DESK_ID

static const char *TAG = "ESP32_MQTT";

static esp_mqtt_client_handle_t s_client = NULL;
static volatile bool s_connected = false;

// Topic costruiti una volta in net_mqtt_start() (dipendono dal DESK_ID).
static char s_topic_event[80];
static char s_topic_config[80];

static void handle_config_payload(const char *data, int len)
{
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (root == NULL) {
        ESP_LOGW(TAG, "config MQTT: JSON non valido");
        return;
    }
    bool changed = config_update_from_cjson(root);
    cJSON_Delete(root);
    ESP_LOGI(TAG, "config MQTT applicata (changed=%s)", changed ? "si" : "no");
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        esp_mqtt_client_subscribe(s_client, s_topic_config, 1);
        ESP_LOGI(TAG, "MQTT connesso, subscribe %s", s_topic_config);
        break;

    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "MQTT disconnesso (riconnessione automatica)");
        break;

    case MQTT_EVENT_DATA:
        // Per payload piccoli il topic arriva intero sul primo (unico) frammento.
        if (event->topic_len > 0 &&
            (int)strlen(s_topic_config) == event->topic_len &&
            strncmp(event->topic, s_topic_config, event->topic_len) == 0) {
            ESP_LOGI(TAG, "config ricevuta (%d byte)", event->data_len);
            handle_config_payload(event->data, event->data_len);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "MQTT_EVENT_ERROR");
        break;

    default:
        break;
    }
}

void net_mqtt_start(void)
{
    snprintf(s_topic_event,  sizeof(s_topic_event),  "desk/%s/event",  DESK_ID);
    snprintf(s_topic_config, sizeof(s_topic_config), "desk/%s/config", DESK_ID);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    s_client = esp_mqtt_client_init(&cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "esp_mqtt_client_init fallito");
        return;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
    ESP_LOGI(TAG, "MQTT client avviato -> %s (event=%s, config=%s)",
             MQTT_BROKER_URI, s_topic_event, s_topic_config);
}

bool net_mqtt_publish_event(const char *payload)
{
    if (s_client == NULL || !s_connected) {
        ESP_LOGW(TAG, "MQTT non connesso: evento scartato");
        return false;
    }
    // QoS 1, retain 0. msg_id < 0 => errore di accodamento.
    int msg_id = esp_mqtt_client_publish(s_client, s_topic_event, payload,
                                         0 /* len: 0 => strlen */, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "publish evento fallito");
        return false;
    }
    ESP_LOGI(TAG, "evento -> %s : %s", s_topic_event, payload);
    return true;
}
