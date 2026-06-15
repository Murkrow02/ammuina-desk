# Fase 05 — MQTT eventi + config runtime

## Obiettivo
Pubblicare gli eventi via MQTT e poter aggiornare i parametri del nodo a runtime da remoto.

## Prerequisiti
Fase 04: eventi calcolati on-device; broker mosquitto già up dalla fase 00.

## Da implementare
- [ ] **firmware/components/net/**: client MQTT (`esp-mqtt`). Task `mqtt` dedicata.
- [ ] **Pubblicazione eventi** su `desk/<id>/event`: `desk occupied`, `desk released`,
  `high noise`, `poor lighting` (payload JSON con timestamp e valori).
- [ ] **Sottoscrizione config** su `desk/<id>/config`: aggiornamento a caldo di
  `sampling_rate`, `noise_thr`, `light_thr`, `occupancy_timeout`, `communication_mode`.
  La config diventa una struct condivisa protetta (mutex) o aggiornata via notify/queue.
- [ ] **proxy/**: client MQTT che si sottoscrive agli eventi e li scrive su InfluxDB; può
  inoltre pubblicare update di config (CLI/endpoint di management).

## Definition of Done
- Gli eventi compaiono in InfluxDB via MQTT (oltre alla telemetria via HTTP/CoAP).
- Pubblicando un nuovo `noise_thr` (o altro) su `desk/<id>/config`, il comportamento del nodo
  cambia entro un ciclo di sensing, senza riavvio.

## Come testare
1. `mosquitto_sub -t 'desk/+/event'` -> vedere gli eventi in tempo reale.
2. `mosquitto_pub -t 'desk/1/config' -m '{"noise_thr": 0.3}'` -> verificare che la soglia
   aggiornata generi/elimini eventi di conseguenza.
3. Verificare eventi memorizzati in InfluxDB.

## Aggancio alla fase successiva
Con telemetria, eventi e config persistiti, la fase 06 costruisce le dashboard descrittive.
