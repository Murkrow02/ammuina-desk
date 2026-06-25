"""LibraryDeskSense data proxy — entrypoint.

Fase 03: server HTTP telemetria (Flask) + writer InfluxDB.
Fase 04: server CoAP (aiocoap) affiancato, stessa validazione e stesso writer.
Le fasi successive aggiungono:
  - client MQTT eventi/config (fase 05)
  - ingestione forecast/metriche (fase 07) e offloading log (fase 09)
"""
import logging
import os
import threading

from dotenv import load_dotenv
from flask import Flask, jsonify, request

from coap_server import run_coap_server
from influx import InfluxWriter
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


@app.get("/health")
def health():
    return jsonify(status="up"), 200


def main() -> None:
    # Server CoAP in un thread daemon (event loop asyncio proprio), accanto a Flask.
    coap_port = int(os.getenv("COAP_PORT", "5683"))
    coap_bind = os.getenv("COAP_BIND", "10.69.69.50")
    threading.Thread(target=run_coap_server, args=(writer, coap_port, coap_bind),
                     daemon=True, name="coap").start()

    port = int(os.getenv("HTTP_PORT", "8080"))
    log.info("LibraryDeskSense proxy up — phase 04 (HTTP :%d + CoAP udp/:%d)",
             port, coap_port)
    app.run(host="0.0.0.0", port=port)


if __name__ == "__main__":
    main()
