# Fase 04 — Telemetria CoAP + `communication_mode`

## Obiettivo
Aggiungere CoAP come trasporto alternativo della telemetria e poter scegliere il protocollo
tramite il parametro `communication_mode`.

## Prerequisiti
Fase 03: telemetria HTTP end-to-end funzionante; trasporto incapsulato in `net/`.

## Da implementare
- [ ] **firmware/components/net/**: client CoAP (libcoap via ESP-IDF) con stessa payload
  della telemetria HTTP. Interfaccia comune `telemetry_send()` che dispatcha su HTTP o CoAP.
- [ ] **Parametro `communication_mode`** (HTTP|CoAP) nella config; al cambio, le invii
  successive usano il nuovo trasporto.
- [ ] **proxy/**: server CoAP (es. `aiocoap`) che riceve la stessa telemetria e usa lo stesso
  writer InfluxDB della fase 03 (nessuna duplicazione del path DB).

## Definition of Done
- La stessa telemetria arriva in InfluxDB sia via HTTP sia via CoAP.
- Cambiando `communication_mode` il firmware commuta trasporto senza riavvio.
- I dati delle due modalità sono indistinguibili a valle (stesso schema/tag).

## Come testare
1. Impostare `communication_mode=HTTP`, verificare ingestione.
2. Impostare `communication_mode=CoAP`, verificare ingestione identica.
3. (Anticipo eval fase 10) annotare bytes/latenza per il confronto futuro.

## Aggancio alla fase successiva
`communication_mode` diventerà aggiornabile da remoto via MQTT insieme agli altri parametri (fase 05).
