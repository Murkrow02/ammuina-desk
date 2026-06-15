# Fase 07 — Forecasting locale (singolo nodo)

## Obiettivo
Calcolare on-device la previsione a breve termine di rumore/luce con il modello **Holt**,
confrontarla con la baseline naive e memorizzare forecast + metriche d'errore.

## Prerequisiti
Fase 02 (finestra rolling) + fase 06 (dashboard) + fase 03/04 (scrittura al proxy).

## Da implementare
- [ ] **components/forecast/**:
  - `holt()` — smoothing esponenziale con trend lineare sulla finestra recente.
  - `naive()` — baseline persistence (next = ultimo osservato).
  - `metrics()` — MAE/MSE confrontando ogni predizione col valore osservato successivo.
- [ ] **Parametro `forecast_horizon`**: numero di intervalli futuri da predire.
- [ ] **Task `forecast`** dedicata: periodicamente predice su finestra locale.
- [ ] **Scrittura al proxy**: forecast (valori predetti, horizon) + metriche, **taggati col
  `desk_id` sorgente**. Il proxy ingerisce e scrive su InfluxDB.
- [ ] **Grafana**: pannello forecasting (predetto vs osservato, MAE/MSE).

## Definition of Done
- Forecast e metriche MAE/MSE presenti in InfluxDB e visibili in Grafana.
- Holt mostra errore confrontabile/migliore rispetto alla baseline naive su dati reali.
- Cambiando `forecast_horizon` cambia l'orizzonte predetto.

## Come testare
1. Far girare un nodo, attendere il riempimento della finestra.
2. Verificare i record di forecast/metriche in InfluxDB e il pannello Grafana.
3. Variare `forecast_horizon` via MQTT e osservare l'effetto.

## Aggancio alla fase successiva
Il forecasting locale diventa il lavoro che la fase 08/09 distribuiranno tra i nodi del cluster.
