# Fase 02 — Buffering locale + sessioni + eventi

## Obiettivo
Mantenere una finestra rolling dei campioni, calcolare le sessioni di occupazione e rilevare
gli eventi ambientali — tutto on-device, comunicando tra task via queue.

## Prerequisiti
Fase 01: i 3 sensori vengono letti periodicamente dalla task `sensing`.

## Da implementare
- [ ] **Finestra rolling in RAM**: buffer circolare degli ultimi N campioni noise/light
  (serve al forecasting in fase 07). Dimensione configurabile.
- [ ] **Task `logic`** separata: riceve i campioni dalla task `sensing` via **FreeRTOS queue**.
- [ ] **Sessione di occupazione**: stato occupied/free; la sessione termina dopo
  `occupancy_timeout` senza presenza; calcolo **durata sessione**.
- [ ] **Rilevazione eventi**: `noise_thr` -> high-noise; `light_thr` -> poor-lighting.
- [ ] Parametri (`sampling_rate`, `noise_thr`, `light_thr`, `occupancy_timeout`) in una
  struct di config centrale (ancora statica; diventerà aggiornabile via MQTT in fase 05).

## Definition of Done
- A log: transizioni occupied/free con durata sessione corretta.
- A log: eventi high-noise e poor-lighting quando si superano le soglie.
- La finestra rolling contiene sempre gli ultimi N campioni (verificabile con un dump di debug).

## Come testare
1. In Wokwi: simulare presenza/assenza col pulsante -> verificare sessioni e timeout.
2. Spingere potenziometro/LDR oltre soglia -> verificare gli eventi.

## Aggancio alla fase successiva
Sessioni, durate ed eventi diventeranno la telemetria inviata al proxy in fase 03.
