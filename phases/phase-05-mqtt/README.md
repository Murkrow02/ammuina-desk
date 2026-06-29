# Fase 05 — MQTT eventi + config runtime

## Obiettivo
Pubblicare gli eventi via MQTT e poter aggiornare i parametri del nodo a runtime da remoto.

## Prerequisiti
Fase 04: eventi calcolati on-device; broker mosquitto già up dalla fase 00.

## Implementato
- [x] **firmware/components/net/**: client MQTT (`esp-mqtt`) in `net_mqtt.c`. Avviato con
  `net_mqtt_start()` dopo `net_init()`; riconnessione automatica gestita da esp-mqtt.
- [x] **Pubblicazione eventi** su `desk/<id>/event` (QoS 1) — rilevazione on-device in
  `net_events.c`, valutata a ogni finestra aggregata:
  - `desk occupied` / `desk released`: macchina a stati con `occupancy_timeout`.
  - `high noise` / `poor lighting`: edge-triggered sul superamento soglia (niente spam).
  - Payload JSON: `{"desk_id","event","ts_ms","noise","light","occupied"}` (`ts_ms` =
    uptime; niente SNTP a bordo, il timestamp finale è server-side nel proxy).
- [x] **Sottoscrizione config** su `desk/<id>/config`: aggiornamento a caldo di
  `sampling_rate`, `noise_thr`, `light_thr`, `occupancy_timeout`, `communication_mode`.
  La config vive in un nuovo component **`config/`**: struct condivisa `node_config_t`
  protetta da **mutex** (`config_get()` ritorna una copia atomica). È la *single source of
  truth* — `net.c` legge da qui il trasporto attivo, il task sensing il `sampling_rate`,
  gli eventi le soglie. Update parziali via `config_update_from_cjson()`.
- [x] **proxy/**: `mqtt_client.py` (`MqttBridge`, paho-mqtt) si sottoscrive a
  `desk/+/event` e scrive su InfluxDB (measurement **`events`**, tag `desk_id`+`event`).
  Management: endpoint Flask `POST /config/<desk_id>` e CLI
  `python mqtt_client.py publish-config <desk_id> '<json>'` per pubblicare update di config.

### Note build
- `communication_mode_t` spostato in `shared/types.h` (usato da `net` e `config` senza cicli).
- **Partizione**: HTTP+CoAP+MQTT insieme sforano la single-app standard (fase 04 era al ~98%):
  `sdkconfig.defaults` ora usa `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE` (~1.5MB, ~31% libero).
- Broker MQTT in `menuconfig → Network → MQTT broker URI` (`mqtt://<host>:1883`).

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
