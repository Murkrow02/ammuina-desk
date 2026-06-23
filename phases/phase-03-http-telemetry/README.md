# Fase 03 — WiFi + telemetria HTTP + proxy + InfluxDB

## Obiettivo
Trasmettere la telemetria via HTTP al data proxy, che la scrive su InfluxDB. Primo flusso
end-to-end ESP32 -> proxy -> DB.

## Prerequisiti
Fase 02: sessioni, durate ed eventi disponibili on-device.

> ⚠️ **Scope effettivo**: la logica sessioni/eventi di fase 02 non è ancora a bordo.
> Questa fase implementa una **telemetria minima**: aggrega la finestra corrente
> (media noise/light, occupazione di maggioranza) e invia. `session_duration_s` è
> sempre `0` finché fase 02 non porta la durata sessione. Il campo è già nello schema.

## Implementato
- [x] **firmware/components/net/**: connessione WiFi station (credenziali via Kconfig
  → menu `Network` in `idf.py menuconfig`). Stile basato sui template del corso
  (`wifi_init_sta`, event group, cJSON).
- [x] **Invio telemetria**: la task `network_io` (in `main.c`) prende la finestra
  dall'aggregatore e fa `POST /telemetry` con `desk_id`, `occupied`,
  `session_duration_s`, `noise`, `light`, `samples`. Client `esp_http_client`
  riusato con keep-alive.
- [x] **proxy/main.py**: server HTTP **Flask** che riceve la telemetria, la valida e
  la scrive su InfluxDB. Errori DB/rete → 503 senza crashare. Endpoint `/health`.
- [x] **proxy/influx.py**: `InfluxWriter` con writer + schema measurement (tag `desk_id`).
- [x] **.env**: URL/token/org/bucket InfluxDB.

### Schema telemetria
Payload JSON:
```json
{"desk_id":"desk-01","occupied":true,"session_duration_s":0,"noise":0.42,"light":350.5,"samples":30}
```
InfluxDB measurement `telemetry` — tag: `desk_id`; field: `occupied` (0/1),
`session_duration_s`, `noise`, `light`.

## Definition of Done
- I dati inviati dall'ESP32 compaiono in InfluxDB (verificabile da Data Explorer).
- Ogni punto è taggato col `desk_id` corretto.
- Il proxy gestisce errori di rete/DB senza crashare.

## Networking in Wokwi (importante)
In Wokwi l'ESP usa il **Public Gateway** (gratis, default): raggiunge solo host
**pubblici**, **non** il `localhost` della tua macchina. Il Private Gateway che
bridgia localhost (`host.wokwi.internal`) è **a pagamento**.

→ Per colpire il proxy locale, esporlo con un **tunnel** e usare quell'URL in
`CONFIG_PROXY_TELEMETRY_URL`. Si usa **HTTP puro** (niente TLS sul firmware):
```
ngrok http 8080      # stampa http://<random>.ngrok-free.dev
```
Il dominio free **cambia ad ogni riavvio** di ngrok → aggiornare l'URL e riflashare.
Tenere il terminale ngrok aperto.

## Come testare
1. **Infra + proxy**:
   ```
   cd infra && docker compose up -d influxdb
   cd ../proxy && pip install -r requirements.txt && python main.py    # :8080
   ```
2. **Tunnel**: `ngrok http 8080` → copiare l'URL `http://...ngrok-free.dev`.
3. **Config firmware**: `idf.py menuconfig` → *Network* → `PROXY_TELEMETRY_URL` =
   `http://<tuo>.ngrok-free.dev/telemetry`. (WiFi default: SSID `Wokwi-GUEST`.)
4. **Build + run**: `idf.py build flash monitor`, poi avvia la simulazione Wokwi.
5. A monitor: `POST telemetry -> HTTP 202`. In InfluxDB Data Explorer: interrogare
   il measurement `telemetry`.

Smoke test del proxy senza ESP32:
```
curl -X POST http://localhost:8080/telemetry -H 'Content-Type: application/json' \
  -d '{"desk_id":"desk-01","occupied":true,"noise":0.4,"light":300}'   # -> 202
```

## Aggancio alla fase successiva
Il path di trasporto verrà reso pluggable per aggiungere CoAP e il `communication_mode` (fase 04).
La durata sessione (fase 02) riempirà `session_duration_s`.
