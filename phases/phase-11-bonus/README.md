# Fase 11 — Bonus (opzionale)

## Obiettivo
Feature opzionali indipendenti, ognuna valutata a parte (+1pt ciascuna). Implementabili in
qualsiasi ordine dopo la fase 10.

## Prerequisiti
Fase 10: sistema core completo e valutato.

## Opzioni

### A — Adaptive Protocol Selection (+1)
Il sistema sceglie automaticamente HTTP o CoAP in base a misure runtime (latenza/overhead).
- [ ] Misura periodica delle metriche di comunicazione per protocollo.
- [ ] Logica di switch automatico di `communication_mode` con isteresi.
- **DoD**: il nodo cambia protocollo da solo quando le condizioni cambiano, log della decisione.

### B — Telegram Bot (+1)
Bot per interrogare stato desk, ricevere notifiche eventi, vedere statistiche recenti.
- [ ] Bot (Python) che legge da InfluxDB / si sottoscrive a MQTT.
- [ ] Comandi: stato desk, statistiche, notifiche push su high-noise/poor-light.
- **DoD**: query e notifiche funzionano end-to-end.

### C — Quietness Recommendation (+1)
Raccomanda le migliori fasce orarie per un desk libero e silenzioso da dati storici.
- [ ] Analisi storica occupazione/rumore/luce (Flux o servizio dedicato).
- [ ] Output: finestre temporali con desk probabilmente libero e quieto.
- **DoD**: raccomandazioni coerenti coi dati storici, visibili in dashboard/bot.

## Come testare
Ogni bonus si valida in isolamento secondo la propria DoD; nessuno deve rompere il core.
