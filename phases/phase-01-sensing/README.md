# Fase 01 — Sensing single-task

## Obiettivo
Una task FreeRTOS legge i 3 sensori a `sampling_rate` e stampa i valori su seriale.

## Prerequisiti
Fase 00 completa: firmware compila, component `sensors/` presente come stub.

## Da implementare
- [ ] **components/sensors/**: API unica di lettura indipendente dal backend:
  - `sensors_read_light()` -> lux (BH1750 I2C su HW / LDR ADC in sim).
  - `sensors_read_noise()` -> livello relativo (MAX9814 ADC1 su HW / potenziometro in sim).
  - `sensors_read_occupancy()` -> bool (PIR GPIO su HW / pulsante in sim).
  - Selezione backend via `Kconfig`/`#ifdef` (`SENSOR_BACKEND_SIM` vs `_HW`).
- [ ] **main/**: task `sensing` creata con `xTaskCreate`, loop ogni `sampling_rate`
  (per ora costante hardcoded), `ESP_LOGI` dei 3 valori.
- [ ] **diagram.json** (Wokwi): aggiungere potenziometro, LDR, pulsante con i pin scelti.

## Definition of Done
- Log seriale mostra periodicamente i 3 valori plausibili.
- Cambiando potenziometro/LDR/pulsante in Wokwi i valori cambiano coerentemente.
- Stesso codice applicativo compila per backend sim e HW (solo backend diverso).

## Come testare
1. Run in Wokwi, variare gli input, osservare il monitor seriale.
2. (Opzionale HW) `idf.py flash monitor` con i sensori reali collegati.

## Aggancio alla fase successiva
I campioni letti qui alimenteranno il buffer rolling e la logica eventi/sessioni della fase 02.
