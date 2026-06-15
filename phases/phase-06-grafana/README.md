# Fase 06 — Grafana descriptive analytics (Flux)

## Obiettivo
Dashboard Grafana che visualizza occupazione, rumore, luce ed eventi, con i pannelli
descrittivi implementati come query **Flux** su InfluxDB.

## Prerequisiti
Fase 05: telemetria, sessioni ed eventi presenti in InfluxDB.

## Da implementare
- [ ] **infra/grafana/dashboards/**: dashboard JSON con almeno:
  - occupazione desk in tempo reale;
  - durata media sessione;
  - livello di rumore nel tempo;
  - intensità luminosa nel tempo;
  - eventi high-noise e poor-lighting (tabella/annotazioni).
- [ ] **Query Flux** per i pannelli di utilizzo/trend (sostituiscono il modulo di analisi
  descrittiva rimosso dalla traccia).
- [ ] **Provisioning**: dashboard e datasource versionati in `infra/grafana/provisioning/`.

## Definition of Done
- Aprendo Grafana i pannelli sono popolati con dati live dei 3 desk.
- I pannelli di utilizzo/trend sono query Flux (non solo viz predefinite).
- La dashboard è riproducibile da zero via provisioning (no setup manuale).

## Come testare
1. `docker compose up`, generare dati dai nodi.
2. Aprire la dashboard, verificare aggiornamento in tempo reale e correttezza dei trend.

## Aggancio alla fase successiva
La dashboard verrà estesa in fase 07 con i risultati del forecasting, e in fase 10 con i
pannelli distribuiti.
