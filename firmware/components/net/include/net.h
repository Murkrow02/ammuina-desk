// net.h — connettivita': WiFi, telemetria HTTP/CoAP, MQTT.
// Fase 03: WiFi station + telemetria HTTP. Fase 04: CoAP + communication_mode.
// MQTT in fase 05.
#pragma once

#include <stdbool.h>
#include "types.h"

// Trasporto telemetria selezionabile a runtime (fase 04).
typedef enum {
    COMM_MODE_HTTP = 0,
    COMM_MODE_COAP = 1,
} communication_mode_t;

// Inizializza NVS + connette il WiFi in modalita' station (bloccante finche'
// non ottiene un IP). Stile template del corso: ritenta all'infinito.
void net_init(void);

// Aggrega la finestra (media noise/light, occupazione di maggioranza) e invia
// la telemetria al proxy. Dispatcha sul trasporto attivo (HTTP o CoAP).
void net_telemetry_send(const network_payload_t *window);

// Cambia il trasporto usato dalle invii successive: nessun riavvio, la commutazione
// e' immediata (fase 05: aggiornabile da remoto via MQTT).
void net_set_communication_mode(communication_mode_t mode);
communication_mode_t net_get_communication_mode(void);
