// net_internal.h — API privata condivisa fra i trasporti del component net.
// Non e' in include/: non va esposta fuori dal component.
#pragma once

#include <stdbool.h>
#include "types.h"

// Costruisce il JSON telemetria condiviso da HTTP e CoAP a partire dalla
// finestra aggregata. Ritorna stringa malloc'd: il chiamante fa free().
char *net_build_telemetry_json(const network_payload_t *window);

// Trasporti telemetria: ricevono il payload JSON gia' pronto.
// Ritornano true se l'invio e' stato confermato dal proxy.
bool net_http_send(const char *payload);
bool net_coap_send(const char *payload);

// Fase 05 — MQTT (net_mqtt.c). Pubblica un evento JSON gia' pronto su
// desk/<id>/event. Ritorna false se il client non e' connesso. Usato da
// net_events.c (stesso component).
bool net_mqtt_publish_event(const char *payload);
