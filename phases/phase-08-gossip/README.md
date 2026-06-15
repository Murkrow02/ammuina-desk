# Fase 08 — Cluster: gossip del carico CPU

## Obiettivo
Far conoscere a ogni nodo il carico CPU dei peer tramite uno scambio gossip periodico su MQTT.

## Prerequisiti
Fase 07: forecasting locale funzionante. Servono 3 nodi con `desk_id` distinti.

## Da implementare
- [ ] **Stima CPU-load**: abilitare `configGENERATE_RUN_TIME_STATS` (FreeRTOS) e ricavare la
  quota della idle-task (es. via `vTaskGetRunTimeStats`). È **l'unica metrica** che guida
  l'offloading.
- [ ] **components/cluster/**: task `gossip` che:
  - pubblica il proprio load su `deskcluster/load/<desk_id>` (**retained**, così un nodo che
    (ri)entra apprende subito l'ultimo load noto dei peer);
  - si sottoscrive a `deskcluster/load/+` e mantiene una tabella `peer -> load`.
- [ ] **Scalabilità**: il protocollo non deve assumere all-to-all hardcoded; con 3 nodi è di
  fatto all-to-all ma deve restare concettualmente scalabile a N (es. fanout limitato/subset).

## Definition of Done
- Ogni nodo mostra (a log/topic) il load aggiornato di sé e dei peer.
- Un nodo che si riavvia apprende immediatamente i load dei peer dai messaggi retained.
- Il carico riportato riflette il reale stato CPU (verificabile inducendo carico).

## Come testare
1. Avviare 3 nodi.
2. `mosquitto_sub -t 'deskcluster/load/+' -v` -> osservare i load.
3. Indurre carico su un nodo e verificare che il suo load pubblicato salga.

## Aggancio alla fase successiva
La tabella dei load dei peer è l'input decisionale dell'offloading manager (fase 09).
