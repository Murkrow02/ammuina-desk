// net.h — connettivita': WiFi, telemetria HTTP/CoAP, MQTT.
// Fase 03: WiFi station + telemetria HTTP. Fase 04: CoAP + communication_mode.
// MQTT in fase 05.
#pragma once

#include <stdbool.h>
#include "types.h"  // communication_mode_t, network_payload_t

// Inizializza NVS + config runtime + connette il WiFi in modalita' station
// (bloccante finche' non ottiene un IP). Stile template del corso: ritenta
// all'infinito.
void net_init(void);

// Aggrega la finestra (media noise/light, occupazione di maggioranza) e invia
// la telemetria al proxy. Dispatcha sul trasporto attivo (HTTP o CoAP).
void net_telemetry_send(const network_payload_t *window);

// Cambia il trasporto usato dalle invii successive: nessun riavvio, la commutazione
// e' immediata. Fase 05: aggiornabile anche da remoto via MQTT (config runtime).
void net_set_communication_mode(communication_mode_t mode);
communication_mode_t net_get_communication_mode(void);

// Fase 05 — MQTT. Avvia il client MQTT (eventi + config). Chiamare dopo net_init().
void net_mqtt_start(void);

// Fase 05 — eventi. Valuta la finestra contro le soglie di config (config/) e
// pubblica via MQTT gli eventi rilevati: "desk occupied"/"desk released" (macchina
// a stati con occupancy_timeout), "high noise", "poor lighting" (edge-triggered).
void net_events_eval(const network_payload_t *window);
