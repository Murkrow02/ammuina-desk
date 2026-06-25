# Fase 04 — Telemetria CoAP + `communication_mode`

## Obiettivo
Aggiungere CoAP come trasporto alternativo della telemetria e poter scegliere il protocollo
tramite il parametro `communication_mode`.

## Prerequisiti
Fase 03: telemetria HTTP end-to-end funzionante; trasporto incapsulato in `net/`.

## Implementato
- [x] **firmware/components/net/**: trasporto reso pluggable. `net.c` ora connette il WiFi,
  costruisce il payload JSON condiviso (`net_build_telemetry_json`) e **dispatcha** su HTTP o
  CoAP. I due trasporti vivono in file separati:
  - `net_http.c` → `net_http_send()` (POST `esp_http_client`, keep-alive, come fase 03).
  - `net_coap.c` → `net_coap_send()`: client **libcoap** (`espressif/coap`), POST CON su
    `coap://<host>:5683/telemetry`, content-format `application/json`, stesso payload.
    Sessione creata pigramente e ricreata su errore/timeout.
- [x] **`communication_mode` (HTTP|CoAP)**: default scelto in `menuconfig → Network`
  (`COMM_MODE_DEFAULT_*`), commutabile **a runtime senza riavvio** con
  `net_set_communication_mode()` — le invii successive usano il nuovo trasporto.
- [x] **proxy/**: server **CoAP** (`aiocoap`, `coap_server.py`) in un thread daemon accanto a
  Flask. Riceve la stessa telemetria sulla risorsa `/telemetry`, valida con la **stessa**
  `telemetry.validate()` dell'HTTP e scrive con lo **stesso** `InfluxWriter` (nessuna
  duplicazione del path DB/schema). Errori DB → `5.03`, payload invalido → `4.00`, ok → `2.04`.

### Schema telemetria
Identico alla fase 03 (stesso JSON, stesso measurement `telemetry`, stessi tag/field):
```json
{"desk_id":"desk-01","occupied":true,"session_duration_s":0,"noise":0.42,"light":350.5,"samples":30}
```
HTTP e CoAP producono punti **indistinguibili** a valle.

## Definition of Done
- La stessa telemetria arriva in InfluxDB sia via HTTP sia via CoAP. ✅
- Cambiando `communication_mode` il firmware commuta trasporto senza riavvio. ✅
- I dati delle due modalità sono indistinguibili a valle (stesso schema/tag). ✅

## Networking in Wokwi (importante)
CoAP è **UDP** (porta 5683). Il Public Gateway di Wokwi raggiunge solo host pubblici e i
tunnel free (ngrok/cloudflared) **NON inoltrano UDP**. Quindi:
- **HTTP**: funziona in Wokwi via tunnel (come fase 03).
- **CoAP**: funziona su **hardware reale in LAN** (`coap://<ip>:5683/telemetry`) o con un
  gateway/tunnel UDP a pagamento. In Wokwi richiede il Private Gateway (Wokwi Club).

## Come testare
1. **Infra + proxy** (espone sia HTTP :8080 sia CoAP udp/:5683):
   ```
   cd infra && docker compose up -d influxdb
   cd ../proxy && pip install -r requirements.txt && python main.py
   ```
   > Su macOS il transport di fallback di `aiocoap` non si lega all'any-address:
   > esportare `COAP_BIND=127.0.0.1` per i test locali (in Docker/Linux resta `0.0.0.0`).
2. **HTTP**: `menuconfig → Network → Trasporto = HTTP`, build/flash, verifica ingestione.
3. **CoAP**: impostare `Trasporto = CoAP` e `COAP_SERVER_URI = coap://<ip>:5683/telemetry`,
   build/flash, verifica che i punti arrivino **identici** in InfluxDB.
4. (Anticipo eval fase 10) annotare bytes/latenza per il confronto futuro.

Smoke test del server CoAP senza ESP32 (client `aiocoap`):
```
echo -n '{"desk_id":"desk-01","occupied":true,"noise":0.4,"light":300}' \
  | aiocoap-client -m POST coap://127.0.0.1/telemetry --content-format application/json
# -> 2.04 Changed
```

## Aggancio alla fase successiva
`communication_mode` diventerà aggiornabile da remoto via MQTT insieme agli altri parametri
(fase 05): basterà chiamare `net_set_communication_mode()` alla ricezione del messaggio di config.

> ⚠️ **Spazio flash**: con libcoap l'app occupa ~98% della partizione app (2% libero).
> Prima di aggiungere altri component pesanti (fase 05 MQTT è già incluso) valutare una
> partition table custom con partizione app più grande.
