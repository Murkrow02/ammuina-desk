#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_timer.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include <stdbool.h>
#include <stdlib.h>
#include "cJSON.h"
#include "driver/temperature_sensor.h"
#include "esp_err.h"

#include <arpa/inet.h>
#include <coap3/coap.h>


// CUSTOMIZE!
//Change with your own credentials
#define WIFI_SSID          "test"
#define WIFI_PASSWORD      "test"

// CUSTOMIZE!
// Change with your broker's credentials
#define MQTT_BROKER_URI     "mqtt://1.1.1.1:1883"
#define MQTT_USERNAME      "test" 
#define MQTT_PASSWORD      "test*"

//CUSTOMIZE!
#define COAP_SERVER_IP         "1.1.1.1"


static const char *TAG = "ESP32_SKETCH";

static EventGroupHandle_t wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;

static esp_mqtt_client_handle_t mqtt_client = NULL;

coap_context_t *ctx;
coap_session_t *session;
static volatile int waiting_response = 0;


// CUSTOMIZE!
typedef struct {
    char device[32];
    float temperature;
    bool active;
} my_data_t;

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

float get_internal_temperature(void)
{
    static temperature_sensor_handle_t temp_handle = NULL;

    if (temp_handle == NULL) {
        temperature_sensor_config_t cfg = {
            .range_min = 10,
            .range_max = 50,
        };

        if (temperature_sensor_install(&cfg, &temp_handle) != ESP_OK) {
            return -1000.0; // error value
        }

        if (temperature_sensor_enable(temp_handle) != ESP_OK) {
            return -1000.0;
        }
    }

    float temp = 0;
    if (temperature_sensor_get_celsius(temp_handle, &temp) != ESP_OK) {
        return -1000.0;
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


// CUSTOMIZE!
bool parse_json(const char *json_str, my_data_t *out)
{
    if (!json_str || !out) return false;

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) return false;

    cJSON *device = cJSON_GetObjectItem(root, "stringex");
    cJSON *temperature = cJSON_GetObjectItem(root, "numberex");
    cJSON *active = cJSON_GetObjectItem(root, "boolex");

    if (cJSON_IsString(device) && device->valuestring) {
        snprintf(out->device, sizeof(out->device), "%s", device->valuestring);
    } else {
        cJSON_Delete(root);
        return false;
    }

    if (cJSON_IsNumber(temperature)) {
        out->temperature = temperature->valuedouble;
    } else {
        cJSON_Delete(root);
        return false;
    }

    if (cJSON_IsBool(active)) {
        out->active = cJSON_IsTrue(active);
    } else {
        cJSON_Delete(root);
        return false;
    }

    cJSON_Delete(root);
    return true;
}


// CUSTOMIZE!
static void handle_incoming_data(const char *topic, const char *data, int len)
{
    char *buf = calloc(1, len + 1);
    if (buf == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return;
    }

    memcpy(buf, data, len);
    buf[len] = '\0';

    ESP_LOGI(TAG, "Received JSON: %s", buf);

    my_data_t parsed;

    if (parse_json(buf, &parsed)) {
        printf("Temp: %.2f\n", parsed.temperature);
    } else {
        printf("Errore parsing JSON\n");
    }

    free(buf);
}


/* ---------------- CoAP functionalities ---------------- */
static coap_response_t response_handler(coap_session_t *session,
                                        const coap_pdu_t *sent,
                                        const coap_pdu_t *received,
                                        const coap_mid_t id)
{
    waiting_response = 0;
    return COAP_RESPONSE_OK;
}

static void nack_handler(coap_session_t *session,
                         const coap_pdu_t *sent,
                         coap_nack_reason_t reason,
                         const coap_mid_t id)
{   
    waiting_response = 0;
}


void coap_init(void)
{
    coap_address_t dst;

    coap_startup();

    ctx = coap_new_context(NULL);

    coap_address_init(&dst);
    dst.addr.sin.sin_family = AF_INET;
    dst.addr.sin.sin_port = htons(5683);
    dst.addr.sin.sin_addr.s_addr = inet_addr(COAP_SERVER_IP); 

    session = coap_new_client_session(ctx, NULL, &dst, COAP_PROTO_UDP);
    coap_register_response_handler(ctx, response_handler);
    coap_register_nack_handler(ctx, nack_handler);
}

void coap_send_json(coap_session_t *session, const char *payload, bool confirmable)
{   
    coap_pdu_t *pdu;
    if (confirmable) 
        pdu= coap_pdu_init(
            COAP_MESSAGE_CON,
            COAP_REQUEST_CODE_POST,
            coap_new_message_id(session),
            coap_session_max_pdu_size(session)
        );
    else
         pdu= coap_pdu_init(
            COAP_MESSAGE_NON,
            COAP_REQUEST_CODE_POST,
            coap_new_message_id(session),
            coap_session_max_pdu_size(session)
        );


    uint8_t buf[4];
    size_t len = coap_encode_var_safe(buf, sizeof(buf), COAP_MEDIATYPE_APPLICATION_JSON);
    coap_add_option(pdu, COAP_OPTION_CONTENT_FORMAT, len, buf);

    coap_add_data(pdu, strlen(payload), (const uint8_t *)payload);
    int64_t coap_t_start_us = esp_timer_get_time();
    int64_t deadline = coap_t_start_us + (int64_t) 1000 * 1000;
   
    coap_send(session, pdu);
    waiting_response = 1;
    while (waiting_response && esp_timer_get_time() < deadline) {
        coap_io_process(ctx, 5);   
    }
}


/* ---------------- MQTT management ---------------- */

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "Subscribed to topic, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "Published message, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Incoming topic: %.*s", event->topic_len, event->topic);
            handle_incoming_data(event->topic, event->data, event->data_len);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT event error");
            break;

        default:
            break;
    }
}



static void mqtt_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client,
                                                   ESP_EVENT_ANY_ID,
                                                   mqtt_event_handler,
                                                   NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
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
    mqtt_start();
}