// net.h — connettivita': WiFi, telemetria HTTP/CoAP, MQTT.
// Fase 03: WiFi station + telemetria HTTP. CoAP in fase 04, MQTT in fase 05.
#pragma once

#include <stdbool.h>
#include "types.h"

// Inizializza NVS + connette il WiFi in modalita' station (bloccante finche'
// non ottiene un IP). Stile template del corso: ritenta all'infinito.
void net_init(void);

// Aggrega la finestra (media noise/light, occupazione di maggioranza) e invia
// la telemetria al proxy via POST JSON su /telemetry.
void net_telemetry_send(const network_payload_t *window);
