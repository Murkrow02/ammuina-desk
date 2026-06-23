"""LibraryDeskSense data proxy — entrypoint.

Fase 03: server HTTP telemetria (Flask) + writer InfluxDB. Le fasi successive aggiungono:
  - server CoAP (fase 04)
  - client MQTT eventi/config (fase 05)
  - ingestione forecast/metriche (fase 07) e offloading log (fase 09)
"""
import logging
import os

from dotenv import load_dotenv
from flask import Flask, jsonify, request

from influx import InfluxWriter

load_dotenv()

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")
log = logging.getLogger("proxy")

# Campi obbligatori del payload telemetria inviato dall'ESP32.
REQUIRED_FIELDS = ("desk_id", "occupied", "noise", "light")

app = Flask(__name__)

writer = InfluxWriter(
    url=os.getenv("INFLUX_URL", "http://localhost:8086"),
    token=os.getenv("INFLUX_TOKEN", "dev-token"),
    org=os.getenv("INFLUX_ORG", "librarydesksense"),
    bucket=os.getenv("INFLUX_BUCKET", "desk"),
)


def _validate(payload: dict) -> str | None:
    """Ritorna un messaggio d'errore se il payload non e' valido, altrimenti None."""
    if not isinstance(payload, dict):
        return "body must be a JSON object"
    missing = [f for f in REQUIRED_FIELDS if f not in payload]
    if missing:
        return f"missing fields: {', '.join(missing)}"
    if not isinstance(payload["desk_id"], str) or not payload["desk_id"]:
        return "desk_id must be a non-empty string"
    if not isinstance(payload["occupied"], bool):
        return "occupied must be a boolean"
    try:
        float(payload["noise"])
        float(payload["light"])
        int(payload.get("session_duration_s", 0))
    except (TypeError, ValueError):
        return "noise/light/session_duration_s must be numeric"
    return None


@app.post("/telemetry")
def telemetry():
    payload = request.get_json(silent=True)
    err = _validate(payload)
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
    port = int(os.getenv("HTTP_PORT", "8080"))
    log.info("LibraryDeskSense proxy up — phase 03 (HTTP telemetry) on :%d", port)
    app.run(host="0.0.0.0", port=port)


if __name__ == "__main__":
    main()
