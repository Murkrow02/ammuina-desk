# Fase 10 — Grafana distribuito + valutazione

## Obiettivo
Rendere visibile il comportamento distribuito in Grafana e compilare la valutazione completa.

## Prerequisiti
Fase 09: offloading distribuito funzionante, eventi diagnostici e load disponibili.

## Da implementare
- [ ] **Grafana — pannelli distribuiti**:
  - CPU-load per nodo nel tempo;
  - local-vs-delegated: dove ogni forecast è stato calcolato vs il desk a cui si riferisce
    (pannello o annotazioni), così l'offloading si vede a colpo d'occhio.
- [ ] **docs/evaluation.md** — compilare le 5 dimensioni:
  1. accuratezza forecasting MAE/MSE (Holt vs naive);
  2. overhead & latenza HTTP vs CoAP;
  3. forecast locale vs delegato (latenza, overhead MQTT);
  4. efficacia load-balancing sotto imbalance + riduzione picco per-nodo;
  5. overhead del gossip (msg/banda).
- [ ] **Harness di misura**: script/procedura per indurre carico e raccogliere i numeri.

## Definition of Done
- Pannelli CPU-load e local-vs-delegated live e leggibili.
- `docs/evaluation.md` compilato con tabelle e numeri reali per tutte e 5 le dimensioni.

## Come testare
1. Eseguire gli scenari (baseline vs offload, HTTP vs CoAP) e raccogliere le metriche.
2. Verificare che i pannelli distribuiti riflettano gli scenari.

## Aggancio alla fase successiva
Progetto core completo. La fase 11 aggiunge bonus opzionali indipendenti.
