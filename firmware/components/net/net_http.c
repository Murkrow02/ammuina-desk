// net_http.c — Fase 03/04: trasporto telemetria via HTTP POST verso il proxy.
#include "net_internal.h"

#include <string.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_http_client.h"

#define PROXY_TELEMETRY_URL CONFIG_PROXY_TELEMETRY_URL

static const char *TAG = "ESP32_HTTP";

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

bool net_http_send(const char *payload)
{
    esp_http_client_handle_t client = get_client();
    if (client == NULL) {
        ESP_LOGE(TAG, "esp_http_client_init failed");
        return false;
    }

    esp_http_client_set_post_field(client, payload, strlen(payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "POST telemetry -> HTTP %d", status);
        return status >= 200 && status < 300;
    }

    ESP_LOGW(TAG, "POST telemetry failed: %s", esp_err_to_name(err));
    // Connessione probabilmente morta: chiudila cosi' il prossimo invio
    // rifa' init + handshake da zero.
    esp_http_client_cleanup(client);
    s_client = NULL;
    return false;
}
