// net_coap.c — Fase 04: trasporto telemetria via CoAP (libcoap, UDP plain).
// Stesso payload JSON dell'HTTP, POST verso la risorsa /telemetry del proxy.
// Context/sessione creati pigramente al primo invio e ricreati su errore
// (gestisce sessione morta o cambio host del server).
#include "net_internal.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/netdb.h"

#include <coap3/coap.h>

#define COAP_SERVER_URI    CONFIG_COAP_SERVER_URI
#define COAP_RESPONSE_MS   3000   // attesa massima dell'ACK/risposta dal server

static const char *TAG = "ESP32_COAP";

static coap_context_t *s_ctx = NULL;
static coap_session_t *s_session = NULL;
static coap_uri_t s_uri;                 // path.s/host.s puntano a COAP_SERVER_URI (static)
static volatile bool s_waiting = false;  // true finche' la risposta non arriva
static volatile bool s_ok = false;       // esito dell'ultima richiesta

/* ---- callback libcoap ---- */

static coap_response_t response_handler(coap_session_t *session,
                                        const coap_pdu_t *sent,
                                        const coap_pdu_t *received,
                                        const coap_mid_t id)
{
    coap_pdu_code_t code = coap_pdu_get_code(received);
    s_ok = ((code >> 5) == 2); // classe 2.xx = successo (2.01..2.05)
    s_waiting = false;
    return COAP_RESPONSE_OK;
}

static void nack_handler(coap_session_t *session,
                         const coap_pdu_t *sent,
                         coap_nack_reason_t reason,
                         const coap_mid_t id)
{
    ESP_LOGW(TAG, "NACK CoAP (reason=%d)", reason);
    s_ok = false;
    s_waiting = false;
}

/* ---- setup/teardown ---- */

// Risolve host:port dell'URI in un coap_address_t (IPv4, UDP).
static bool resolve_server(const coap_uri_t *uri, coap_address_t *dst)
{
    char host[128];
    size_t hlen = uri->host.length < sizeof(host) - 1 ? uri->host.length
                                                       : sizeof(host) - 1;
    memcpy(host, uri->host.s, hlen);
    host[hlen] = '\0';

    char port[8];
    snprintf(port, sizeof(port), "%u", uri->port);

    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_DGRAM };
    struct addrinfo *res = NULL;
    if (getaddrinfo(host, port, &hints, &res) != 0 || res == NULL) {
        ESP_LOGE(TAG, "getaddrinfo fallito per %s:%s", host, port);
        return false;
    }

    coap_address_init(dst);
    memcpy(&dst->addr.sa, res->ai_addr, res->ai_addrlen);
    dst->size = res->ai_addrlen;
    freeaddrinfo(res);
    return true;
}

static void coap_teardown(void)
{
    if (s_session) { coap_session_release(s_session); s_session = NULL; }
    if (s_ctx)     { coap_free_context(s_ctx);        s_ctx = NULL; }
}

static bool coap_setup(void)
{
    if (s_session != NULL) return true;

    coap_startup();

    if (coap_split_uri((const uint8_t *)COAP_SERVER_URI,
                       strlen(COAP_SERVER_URI), &s_uri) < 0) {
        ESP_LOGE(TAG, "URI CoAP non valido: %s", COAP_SERVER_URI);
        return false;
    }

    coap_address_t dst;
    if (!resolve_server(&s_uri, &dst)) return false;

    s_ctx = coap_new_context(NULL);
    if (s_ctx == NULL) {
        ESP_LOGE(TAG, "coap_new_context fallito");
        return false;
    }

    s_session = coap_new_client_session(s_ctx, NULL, &dst, COAP_PROTO_UDP);
    if (s_session == NULL) {
        ESP_LOGE(TAG, "coap_new_client_session fallito");
        coap_free_context(s_ctx);
        s_ctx = NULL;
        return false;
    }

    coap_register_response_handler(s_ctx, response_handler);
    coap_register_nack_handler(s_ctx, nack_handler);
    ESP_LOGI(TAG, "Sessione CoAP pronta verso %s", COAP_SERVER_URI);
    return true;
}

// Aggiunge le opzioni URI-Path (un'opzione per segmento) dall'URI configurato.
static void add_path_options(coap_optlist_t **optlist, const coap_uri_t *uri)
{
    const uint8_t *p = uri->path.s;
    size_t remaining = uri->path.length;
    while (remaining > 0) {
        size_t seg = 0;
        while (seg < remaining && p[seg] != '/') seg++;
        coap_insert_optlist(optlist,
                            coap_new_optlist(COAP_OPTION_URI_PATH, seg, p));
        p += seg;
        remaining -= seg;
        if (remaining > 0) { p++; remaining--; } // salta lo '/'
    }
}

/* ---- invio ---- */

bool net_coap_send(const char *payload)
{
    if (!coap_setup()) return false;

    coap_pdu_t *pdu = coap_pdu_init(COAP_MESSAGE_CON,
                                    COAP_REQUEST_CODE_POST,
                                    coap_new_message_id(s_session),
                                    coap_session_max_pdu_size(s_session));
    if (pdu == NULL) {
        ESP_LOGE(TAG, "coap_pdu_init fallito");
        return false;
    }

    coap_optlist_t *optlist = NULL;
    add_path_options(&optlist, &s_uri);

    uint8_t cf[4];
    size_t cflen = coap_encode_var_safe(cf, sizeof(cf),
                                        COAP_MEDIATYPE_APPLICATION_JSON);
    coap_insert_optlist(&optlist,
                        coap_new_optlist(COAP_OPTION_CONTENT_FORMAT, cflen, cf));

    coap_add_optlist_pdu(pdu, &optlist);
    coap_delete_optlist(optlist);

    coap_add_data(pdu, strlen(payload), (const uint8_t *)payload);

    s_waiting = true;
    s_ok = false;
    if (coap_send(s_session, pdu) == COAP_INVALID_MID) {
        ESP_LOGW(TAG, "coap_send fallito");
        coap_teardown();
        return false;
    }

    int64_t deadline = esp_timer_get_time() + (int64_t)COAP_RESPONSE_MS * 1000;
    while (s_waiting && esp_timer_get_time() < deadline) {
        coap_io_process(s_ctx, 50);
    }

    if (s_waiting) {
        ESP_LOGW(TAG, "timeout risposta CoAP");
        s_waiting = false;
        coap_teardown(); // sessione probabilmente inutilizzabile: ricreala
        return false;
    }

    if (s_ok) {
        ESP_LOGI(TAG, "POST telemetria CoAP -> 2.xx OK");
    } else {
        ESP_LOGW(TAG, "risposta CoAP non-success");
    }
    return s_ok;
}
