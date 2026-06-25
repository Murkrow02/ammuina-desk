"""Validazione telemetria condivisa fra i trasporti del proxy.

HTTP (main.py) e CoAP (coap_server.py) usano la stessa validazione e lo stesso
InfluxWriter: nessuna duplicazione del path DB ne' dello schema.
"""

# Campi obbligatori del payload telemetria inviato dall'ESP32.
REQUIRED_FIELDS = ("desk_id", "occupied", "noise", "light")


def validate(payload: dict) -> str | None:
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
