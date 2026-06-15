# Fase 09 — Offloading manager + forecasting distribuito

## Obiettivo
Decidere se calcolare il forecast localmente o delegarlo al peer meno carico, con anti-thrashing
e fallback, rendendo il comportamento distribuito osservabile.

## Prerequisiti
Fase 08: ogni nodo conosce i load dei peer via gossip.

## Da implementare
- [ ] **Parametri** `offload_load_thr` (soglia CPU per delegare) e `offload_timeout`.
- [ ] **Task `offload_mgr`**: confronta il proprio load con `offload_load_thr` e con i load
  dei peer:
  - load < soglia -> **calcolo locale**;
  - load >= soglia -> **delega** al peer meno carico.
- [ ] **Transport delega (MQTT)**:
  - requester pubblica su `deskcluster/offload/req/<target_desk_id>` la **finestra dati**
    (solo valori derivati noise/light, **mai audio**) + `req_id` + `source_desk_id`;
  - il target sottoscrive il proprio topic req, calcola, **scrive il forecast al proxy taggato
    col `source_desk_id`** (nessun round-trip al richiedente), poi pubblica ack su
    `deskcluster/offload/ack/<source_desk_id>`.
- [ ] **Anti-thrashing**: banda di **isteresi** e **tie-break randomizzato** (nodi busy non
  delegano tutti allo stesso peer).
- [ ] **Fallback**: se nessun ack entro `offload_timeout`, il richiedente calcola in locale.
- [ ] **Eventi diagnostici** di offload (es. "forecast delegated to peer X" / "computed for
  peer Y") — solo per visualizzazione, non devono alterare la funzionalità core.

## Definition of Done
- Sotto sbilanciamento indotto, un nodo carico delega al peer meno carico; il forecast risultante
  è correttamente attribuito al desk sorgente in InfluxDB.
- Disabilitando il target (no ack) il richiedente esegue il fallback locale entro `offload_timeout`.
- Nessun thrashing osservabile (decisioni stabili).

## Come testare
1. 3 nodi attivi; indurre carico alto su uno.
2. `mosquitto_sub -t 'deskcluster/offload/#' -v` -> osservare req/ack.
3. Verificare in InfluxDB il forecast taggato col desk sorgente.
4. Spegnere il target a metà -> verificare il fallback locale.

## Aggancio alla fase successiva
Gli eventi di offload e i load alimentano i pannelli distribuiti e la valutazione (fase 10).
