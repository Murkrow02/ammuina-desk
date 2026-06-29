"""Bridge MQTT del proxy (fase 05).

Si sottoscrive a desk/+/event, valida gli eventi pubblicati dai nodi e li scrive
su InfluxDB con lo stesso InfluxWriter di telemetria (measurement "events").
Espone publish_config() per inviare update di config a un nodo (desk/<id>/config),
usato sia dall'endpoint di management Flask sia dalla CLI in fondo al file.

Pensato per girare accanto a Flask/CoAP: start() avvia il loop MQTT in un thread
interno di paho (loop_start), non blocca.
"""
import json
import logging

import paho.mqtt.client as mqtt

from influx import InfluxWriter

log = logging.getLogger("proxy.mqtt")

EVENT_TOPIC = "desk/+/event"  # wildcard: tutti i desk


class MqttBridge:
    def __init__(self, writer: InfluxWriter, host: str = "localhost", port: int = 1883) -> None:
        self.writer = writer
        self.host = host
        self.port = port
        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message

    def _on_connect(self, client, userdata, flags, reason_code, properties) -> None:
        if reason_code != 0:
            log.error("MQTT connect fallita: %s", reason_code)
            return
        client.subscribe(EVENT_TOPIC, qos=1)
        log.info("MQTT connesso, subscribe %s", EVENT_TOPIC)

    def _on_message(self, client, userdata, msg) -> None:
        try:
            payload = json.loads(msg.payload.decode("utf-8"))
        except (ValueError, UnicodeDecodeError):
            log.warning("Evento non JSON su %s", msg.topic)
            return
        if not isinstance(payload, dict) or "desk_id" not in payload or "event" not in payload:
            log.warning("Evento incompleto su %s: %s", msg.topic, payload)
            return

        try:
            self.writer.write_event(payload)
        except Exception as exc:  # noqa: BLE001 — non far cadere il bridge per errori DB/rete
            log.error("Scrittura evento InfluxDB fallita: %s", exc)
            return

        log.info("Evento desk_id=%s %s noise=%.3f light=%.1f occupied=%s",
                 payload["desk_id"], payload["event"],
                 float(payload.get("noise", 0.0)), float(payload.get("light", 0.0)),
                 payload.get("occupied"))

    def start(self) -> None:
        self.client.connect(self.host, self.port, keepalive=60)
        self.client.loop_start()
        log.info("MQTT bridge up — %s:%d", self.host, self.port)

    def publish_config(self, desk_id: str, config: dict) -> None:
        topic = f"desk/{desk_id}/config"
        self.client.publish(topic, json.dumps(config), qos=1)
        log.info("Config -> %s : %s", topic, config)


def _cli() -> None:
    """CLI di management: pubblica un update di config su un nodo.

    Esempio:
      python mqtt_client.py publish-config desk-01 '{"noise_thr": 0.3}'
    """
    import os
    import sys

    from dotenv import load_dotenv
    import paho.mqtt.publish as publish

    load_dotenv()
    logging.basicConfig(level=logging.INFO,
                        format="%(asctime)s %(levelname)s %(name)s: %(message)s")

    host = os.getenv("MQTT_HOST", "localhost")
    port = int(os.getenv("MQTT_PORT", "1883"))

    if len(sys.argv) >= 4 and sys.argv[1] == "publish-config":
        desk_id = sys.argv[2]
        config = json.loads(sys.argv[3])
        topic = f"desk/{desk_id}/config"
        publish.single(topic, json.dumps(config), qos=1, hostname=host, port=port)
        print(f"sent config to {topic}: {config}")
    else:
        print("usage: python mqtt_client.py publish-config <desk_id> '<json>'")
        sys.exit(2)


if __name__ == "__main__":
    _cli()
