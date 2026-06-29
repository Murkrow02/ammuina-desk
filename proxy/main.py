"""LibraryDeskSense data proxy — entrypoint.

Fase 03: server HTTP telemetria (Flask) + writer InfluxDB.
Fase 04: server CoAP (aiocoap) affiancato, stessa validazione e stesso writer.
Fase 05: bridge MQTT — sottoscrive desk/+/event e scrive gli eventi su InfluxDB;
  endpoint di management POST /config/<desk_id> per pubblicare update di config.
Le fasi successive aggiungono:
  - ingestione forecast/metriche (fase 07) e offloading log (fase 09)
"""
import logging
import os
import threading

from dotenv import load_dotenv
from flask import Flask, jsonify, render_template_string, request

from coap_server import run_coap_server
from influx import InfluxWriter
from mqtt_client import MqttBridge
from telemetry import validate

load_dotenv()

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")
log = logging.getLogger("proxy")

app = Flask(__name__)

writer = InfluxWriter(
    url=os.getenv("INFLUX_URL", "http://localhost:8086"),
    token=os.getenv("INFLUX_TOKEN", "dev-token"),
    org=os.getenv("INFLUX_ORG", "librarydesksense"),
    bucket=os.getenv("INFLUX_BUCKET", "desk"),
)

mqtt_bridge = MqttBridge(
    writer,
    host=os.getenv("MQTT_HOST", "localhost"),
    port=int(os.getenv("MQTT_PORT", "1883")),
)


@app.post("/telemetry")
def telemetry():
    payload = request.get_json(silent=True)
    err = validate(payload)
    if err:
        log.warning("Telemetria rifiutata: %s", err)
        return jsonify(error=err), 400

    try:
        writer.write_telemetry(payload)
    except Exception as exc:  # noqa: BLE001 — non far crashare il proxy per errori DB/rete
        log.error("Scrittura InfluxDB fallita: %s", exc)
        return jsonify(error="influx write failed"), 503

    log.info("Telemetria desk_id=%s occupied=%s noise=%.3f light=%.1f",
             payload["desk_id"], payload["occupied"], float(payload["noise"]),
             float(payload["light"]))
    return jsonify(status="ok"), 202


CONFIG_UI = """<!doctype html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>LibraryDeskSense — config nodo</title>
<style>
  :root { color-scheme: light dark; }
  body { font: 15px/1.5 system-ui, sans-serif; max-width: 460px; margin: 2rem auto; padding: 0 1rem; }
  h1 { font-size: 1.2rem; }
  p.hint { color: #888; font-size: .85rem; margin-top: -.4rem; }
  label { display: block; margin: .9rem 0 .2rem; font-weight: 600; }
  input, select { width: 100%; padding: .5rem; box-sizing: border-box; font: inherit; }
  small { color: #888; font-weight: 400; }
  button { margin-top: 1.2rem; padding: .6rem 1.2rem; font: inherit; cursor: pointer; }
  #out { margin-top: 1rem; padding: .6rem; border-radius: 6px; white-space: pre-wrap;
         font-family: monospace; font-size: .85rem; display: none; }
  .ok { background: #1c7c3622; border: 1px solid #1c7c36; }
  .err { background: #c0303022; border: 1px solid #c03030; }
</style>
</head>
<body>
<h1>Config nodo — runtime</h1>
<p class="hint">Pubblica su <code>desk/&lt;id&gt;/config</code> via MQTT. Campi vuoti = invariati.</p>

<label>Desk ID <small>(es. desk-01)</small></label>
<input id="desk_id" value="desk-01">

<label>sampling_rate <small>ms</small></label>
<input id="sampling_rate" type="number" min="1" step="1" placeholder="es. 50">

<label>noise_thr <small>0..1</small></label>
<input id="noise_thr" type="number" min="0" max="1" step="0.01" placeholder="es. 0.6">

<label>light_thr <small>lux</small></label>
<input id="light_thr" type="number" min="0" step="1" placeholder="es. 200">

<label>occupancy_timeout <small>s</small></label>
<input id="occupancy_timeout" type="number" min="0" step="1" placeholder="es. 300">

<label>communication_mode</label>
<select id="communication_mode">
  <option value="">(invariato)</option>
  <option value="http">http</option>
  <option value="coap">coap</option>
</select>

<button onclick="send()">Pubblica config</button>
<div id="out"></div>

<script>
const NUM = ["sampling_rate", "noise_thr", "light_thr", "occupancy_timeout"];
async function send() {
  const out = document.getElementById("out");
  const desk = document.getElementById("desk_id").value.trim();
  if (!desk) { return show("desk_id obbligatorio", false); }
  const cfg = {};
  for (const k of NUM) {
    const v = document.getElementById(k).value.trim();
    if (v !== "") cfg[k] = Number(v);
  }
  const cm = document.getElementById("communication_mode").value;
  if (cm) cfg.communication_mode = cm;
  if (Object.keys(cfg).length === 0) { return show("nessun campo da inviare", false); }
  try {
    const r = await fetch(`/config/${encodeURIComponent(desk)}`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(cfg),
    });
    const j = await r.json();
    show(JSON.stringify(j, null, 2), r.ok);
  } catch (e) {
    show("errore: " + e, false);
  }
}
function show(msg, ok) {
  const out = document.getElementById("out");
  out.textContent = msg;
  out.className = ok ? "ok" : "err";
  out.style.display = "block";
}
</script>
</body>
</html>
"""


@app.get("/")
def config_ui():
    """Pannello web per cambiare la config dei nodi al volo (POST /config/<id>)."""
    return render_template_string(CONFIG_UI)


@app.post("/config/<desk_id>")
def push_config(desk_id):
    """Management: pubblica un update di config su desk/<desk_id>/config.
    Body JSON con i campi da cambiare, es. {"noise_thr": 0.3}."""
    config = request.get_json(silent=True)
    if not isinstance(config, dict) or not config:
        return jsonify(error="body must be a non-empty JSON object"), 400
    mqtt_bridge.publish_config(desk_id, config)
    return jsonify(status="published", topic=f"desk/{desk_id}/config", config=config), 202


@app.get("/health")
def health():
    return jsonify(status="up"), 200


def main() -> None:
    # Server CoAP in un thread daemon (event loop asyncio proprio), accanto a Flask.
    coap_port = int(os.getenv("COAP_PORT", "5683"))
    coap_bind = os.getenv("COAP_BIND", "10.69.69.50")
    threading.Thread(target=run_coap_server, args=(writer, coap_port, coap_bind),
                     daemon=True, name="coap").start()

    # Bridge MQTT: loop in thread interno di paho, non blocca.
    mqtt_bridge.start()

    port = int(os.getenv("HTTP_PORT", "8080"))
    log.info("LibraryDeskSense proxy up — phase 05 (HTTP :%d + CoAP udp/:%d + MQTT eventi/config)",
             port, coap_port)
    app.run(host="0.0.0.0", port=port)


if __name__ == "__main__":
    main()
