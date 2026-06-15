# Architettura — LibraryDeskSense

## Componenti

```
                         MQTT (mosquitto)
        eventi/config <------+------> gossip carico / offloading
                             |
  +--------+   HTTP/CoAP  +--+-----+   write   +----------+   query   +---------+
  | ESP32  | -----------> | Proxy  | --------> | InfluxDB | <-------- | Grafana |
  | (xN)   |              | Python |           |          |           |         |
  +--------+              +--------+           +----------+           +---------+
```

## Task FreeRTOS per nodo

| Task | Ruolo | Comunicazione |
|------|-------|---------------|
| `sensing` | legge sensori a `sampling_rate`, mantiene finestra rolling RAM | queue -> logic |
| `logic/telemetry` | sessioni occupazione, eventi, invio telemetria HTTP/CoAP | queue, event group |
| `mqtt` | pubblica eventi, riceve config, instrada traffico cluster | notifications |
| `gossip` | pubblica CPU-load, consuma load dei peer | topic retained |
| `offload_mgr` | decide local vs delega; gestisce lavoro delegato in arrivo | queue, notify |
| `forecast` | esegue Holt + baseline su finestra locale o ricevuta | queue |

Inter-task: **queue / event group / task notifications** (no variabili globali condivise).

## Parametri di configurazione (via MQTT)

| Param | Significato | Introdotto in |
|-------|-------------|---------------|
| `sampling_rate` | intervallo tra sensing | fase 01/05 |
| `noise_thr` | soglia evento high-noise | fase 02/05 |
| `light_thr` | soglia evento poor-light | fase 02/05 |
| `occupancy_timeout` | timeout fine sessione | fase 02/05 |
| `communication_mode` | HTTP \| CoAP | fase 04/05 |
| `forecast_horizon` | n. intervalli futuri da predire | fase 07 |
| `offload_load_thr` | soglia CPU oltre cui delegare | fase 09 |
| `offload_timeout` | attesa ack prima del fallback locale | fase 09 |

## Topic MQTT

| Topic | Uso | Note |
|-------|-----|------|
| `desk/<id>/event` | eventi (occupied/released/high-noise/poor-light) | fase 05 |
| `desk/<id>/config` | update config runtime | fase 05 |
| `deskcluster/load/<desk_id>` | gossip carico CPU | **retained**, fase 08 |
| `deskcluster/load/+` | subscribe carico peer | fase 08 |
| `deskcluster/offload/req/<target_desk_id>` | finestra + req-id + source desk-id | fase 09 |
| `deskcluster/offload/ack/<source_desk_id>` | completamento delega | fase 09 |

(I nomi esatti dei topic eventi/config si fissano in fase 05.)

## Forecasting

- Modello: **Holt** (exp smoothing con trend lineare) sulla finestra recente.
- Baseline: **naive persistence** (next = last observed).
- Metriche: **MAE/MSE** calcolate on-device confrontando predizione vs valore osservato.
- Output scritto al proxy **taggato col desk-id sorgente**, a prescindere da chi calcola.

## Vincolo privacy

Nessun audio grezzo memorizzato/trasmesso/processato. Le finestre scambiate per la delega
contengono **solo** valori derivati noise/light.
