# Fase 03 — WiFi + telemetria HTTP + proxy + InfluxDB

## Obiettivo
Trasmettere la telemetria via HTTP al data proxy, che la scrive su InfluxDB. Primo flusso
end-to-end ESP32 -> proxy -> DB.

## Prerequisiti
Fase 02: sessioni, durate ed eventi disponibili on-device.

## Da implementare
- [ ] **firmware/components/net/**: connessione WiFi (credenziali via Kconfig/env).
- [ ] **Task `telemetry`**: invia periodicamente JSON con `desk_id`, stato occupazione,
  durata sessione, noise, light al proxy (`POST /telemetry`).
- [ ] **proxy/**: server HTTP (es. FastAPI/Flask o `aiohttp`) che riceve la telemetria,
  la valida e la scrive su InfluxDB tramite il client ufficiale.
- [ ] **proxy/**: modulo `influx.py` con writer + schema measurement (tag `desk_id`).
- [ ] **.env**: URL/token/bucket InfluxDB.

## Definition of Done
- I dati inviati dall'ESP32 compaiono in InfluxDB (verificabile da Data Explorer).
- Ogni punto è taggato col `desk_id` corretto.
- Il proxy gestisce errori di rete/DB senza crashare.

## Come testare
1. Configurare WiFi (in Wokwi: rete `Wokwi-GUEST`).
2. Avviare proxy + infra; far girare il firmware.
3. In InfluxDB Data Explorer: interrogare il measurement della telemetria.

## Aggancio alla fase successiva
Il path di trasporto verrà reso pluggable per aggiungere CoAP e il `communication_mode` (fase 04).
