// config.h — Fase 05: config runtime del nodo, condivisa e protetta da mutex.
// I parametri sono aggiornabili a caldo da remoto via MQTT (desk/<id>/config)
// senza riavvio. I consumatori leggono una copia atomica con config_get().
#pragma once

#include <stdbool.h>
#include "types.h"   // communication_mode_t
#include "cJSON.h"

// Parametri runtime del nodo. Valori iniziali da menuconfig -> "Node runtime config".
typedef struct {
    int   sampling_rate_ms;       // periodo di campionamento del task sensing
    float noise_thr;              // soglia rumore relativo 0..1 -> evento "high noise"
    float light_thr;             // soglia luce (lux): sotto -> evento "poor lighting"
    int   occupancy_timeout_s;   // inattivita' prima dell'evento "desk released"
    communication_mode_t communication_mode; // trasporto telemetria attivo
} node_config_t;

// Crea il mutex e carica i default da Kconfig. Chiamare una volta all'avvio.
void config_init(void);

// Ritorna una copia atomica della config corrente (presa sotto mutex).
node_config_t config_get(void);

// Aggiorna il solo communication_mode sotto mutex (usato da net_set_communication_mode).
void config_set_communication_mode(communication_mode_t mode);

// Applica un JSON di config parziale: aggiorna solo i campi presenti e validi.
// Ritorna true se almeno un campo e' cambiato. Thread-safe (mutex interno).
bool config_update_from_cjson(const cJSON *root);
