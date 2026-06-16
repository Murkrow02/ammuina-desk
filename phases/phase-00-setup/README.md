# Fase 00 — Setup & scheletro

## Obiettivo
Avere toolchain, scheletro repo e infrastruttura funzionanti: firmware che compila e gira,
proxy che avvia, broker/InfluxDB/Grafana up.

## Prerequisiti
Nessuno (fase iniziale).

## Da implementare
- [ ] **firmware/**: progetto ESP-IDF minimo (`main/main.c`) con blink + log su seriale.
  - `CMakeLists.txt`, `sdkconfig.defaults`, `wokwi.toml` + `diagram.json` per Wokwi.
  - Scheletri vuoti dei component: `sensors/`, `net/`, `forecast/`, `cluster/` (header stub).
- [x] **proxy/**: app Python che avvia e logga "proxy up" (entrypoint `proxy/main.py`),
  `requirements.txt`, config via env (`.env.example`).
- [x] **infra/**: `docker-compose.yml` con **mosquitto**, **influxdb**, **grafana**;
  config base (`mosquitto.conf`, datasource Grafana -> InfluxDB).

## Definition of Done
- `cd infra && docker compose up` -> 3 servizi healthy (porte: mqtt 1883, influx 8086, grafana 3000).
- `idf.py build` ok; firmware gira in Wokwi e stampa log periodici.
- `python proxy/main.py` avvia senza errori e logga lo stato.

## Come testare
1. `cd infra && docker compose up -d && docker compose ps`
2. Aprire Grafana su `localhost:3000`, verificare datasource InfluxDB connesso.
3. Build+run firmware in Wokwi (o `idf.py flash monitor` su HW).
4. Avviare il proxy e vedere il log di startup.

## Aggancio alla fase successiva
Lo scheletro `firmware/components/sensors/` ospiterà la lettura reale dei sensori in fase 01.
