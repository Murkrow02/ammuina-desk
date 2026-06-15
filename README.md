# LibraryDeskSense

IoT proof-of-concept: cluster di 3 ESP32 che monitorano scrivanie da biblioteca
(occupazione, rumore, luce) con **forecasting on-device distribuito** e analytics in Grafana.
Privacy-preserving: nessun audio grezzo, nessuna identificazione utente.

## Architettura in breve

```
[ESP32 x3]  --HTTP/CoAP-->  [Data Proxy (Python)]  -->  [InfluxDB]  -->  [Grafana]
     |                              ^
     +------------ MQTT ------------+   eventi, config, gossip carico, offloading
   (mosquitto broker)
```

- **firmware/** — progetto ESP-IDF unico (FreeRTOS). Evolve fase per fase.
- **proxy/** — app Python: server HTTP + CoAP, client MQTT, writer InfluxDB.
- **infra/** — docker-compose: mosquitto + influxdb + grafana.
- **docs/** — architettura e valutazione.
- **phases/** — milestone di sviluppo (stile nand2tetris): un README per fase.

## Come è organizzato lo sviluppo

Sviluppo a **fasi incrementali**. Il codice vive nelle dir condivise (`firmware/`,
`proxy/`, `infra/`); ogni cartella `phases/phase-NN/` contiene solo il **README** con cosa
implementare, la **Definition of Done** e come testare. La fase `n+1` assume completata la `n`.

| Fase | Cartella | Obiettivo |
|------|----------|-----------|
| 00 | `phases/phase-00-setup` | Toolchain, scheletro repo, infra up |
| 01 | `phases/phase-01-sensing` | Lettura 3 sensori in una task FreeRTOS |
| 02 | `phases/phase-02-buffering` | Buffer rolling, sessioni occupazione, eventi |
| 03 | `phases/phase-03-http-telemetry` | WiFi + telemetria HTTP -> proxy -> InfluxDB |
| 04 | `phases/phase-04-coap` | Telemetria CoAP + `communication_mode` |
| 05 | `phases/phase-05-mqtt` | Eventi MQTT + config runtime |
| 06 | `phases/phase-06-grafana` | Dashboard descrittiva (Flux) |
| 07 | `phases/phase-07-forecast-local` | Forecasting locale Holt + baseline + MAE/MSE |
| 08 | `phases/phase-08-gossip` | Gossip carico CPU nel cluster |
| 09 | `phases/phase-09-offloading` | Offloading manager + forecasting distribuito |
| 10 | `phases/phase-10-eval` | Grafana distribuito + valutazione |
| 11 | `phases/phase-11-bonus` | Bonus opzionali |

Inizia da [`phases/phase-00-setup/README.md`](phases/phase-00-setup/README.md).

## Hardware

Dev in **Wokwi** (simulazione), demo su **3 ESP32 reali**. Sensori reali:
- Occupancy: **HC-SR501 PIR** (GPIO digitale)
- Rumore: **MAX9814** (uscita analogica -> ADC1; livello *relativo*, AGC)
- Luce: **GY-302 BH1750** (I2C)

In simulazione si usano stand-in (potenziometro=rumore, LDR=luce, pulsante=occupancy)
dietro l'astrazione `firmware/components/sensors/`.

## Vincolo privacy

Il microfono stima **solo l'intensità sonora**. Mai registrare/trasmettere/processare audio
grezzo. Le finestre dati scambiate tra nodi contengono solo valori derivati noise/light.
