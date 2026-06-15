# Valutazione — LibraryDeskSense

Da compilare nella **fase 10**. Cinque dimensioni.

## 1. Accuratezza forecasting (MAE/MSE)
Confronto Holt vs baseline naive su noise e light. Tabella per desk + aggregato.

| Desk | Grandezza | Modello | MAE | MSE |
|------|-----------|---------|-----|-----|
| | | Holt | | |
| | | naive | | |

## 2. HTTP vs CoAP — overhead & latenza
Stessa telemetria su entrambi i protocolli.

| Protocollo | Bytes/msg | Latenza media (ms) | Latenza p95 (ms) |
|------------|-----------|--------------------|------------------|
| HTTP | | | |
| CoAP | | | |

## 3. Forecast locale vs delegato — latenza & overhead
| Modalità | Latenza end-to-end (ms) | Bytes su MQTT | Note |
|----------|-------------------------|---------------|------|
| locale | | 0 | |
| delegato | | | finestra + ack |

## 4. Efficacia load-balancing
Sotto sbilanciamento di carico indotto: distribuzione del lavoro di forecast tra nodi,
riduzione del picco per-nodo.

| Scenario | Forecast nodo A | nodo B | nodo C | Picco load max |
|----------|-----------------|--------|--------|----------------|
| no offload | | | | |
| con offload | | | | |

## 5. Overhead gossip
Costo in messaggi/banda del protocollo di disseminazione carico.

| Metrica | Valore |
|---------|--------|
| msg/s per nodo | |
| bytes/msg | |
| banda totale cluster | |

## Setup di misura
Descrivere qui: come si induce il carico, come si misurano latenze (timestamp), strumenti.
