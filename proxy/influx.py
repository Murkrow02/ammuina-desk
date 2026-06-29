"""InfluxDB writer per telemetria ed eventi.

Schema measurement "telemetry" (fase 03):
  tag:    desk_id
  fields: occupied (int 0/1), session_duration_s (int), noise (float), light (float)

Schema measurement "events" (fase 05, via MQTT):
  tags:   desk_id, event ("desk occupied"|"desk released"|"high noise"|"poor lighting")
  fields: noise (float), light (float), occupied (int 0/1)
"""
import logging

from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS

log = logging.getLogger("proxy.influx")

MEASUREMENT = "telemetry"
EVENTS_MEASUREMENT = "events"


class InfluxWriter:
    """Wrapper sottile sul client ufficiale InfluxDB con write sincrona."""

    def __init__(self, url: str, token: str, org: str, bucket: str) -> None:
        self.bucket = bucket
        self.org = org
        self._client = InfluxDBClient(url=url, token=token, org=org)
        self._write_api = self._client.write_api(write_options=SYNCHRONOUS)
        log.info("InfluxWriter pronto: url=%s org=%s bucket=%s", url, org, bucket)

    def write_telemetry(self, data: dict) -> None:
        """Scrive un punto di telemetria. Solleva in caso di errore DB/rete
        (il chiamante decide come rispondere all'ESP32)."""
        point = (
            Point(MEASUREMENT)
            .tag("desk_id", str(data["desk_id"]))
            .field("occupied", 1 if data["occupied"] else 0)
            .field("session_duration_s", int(data.get("session_duration_s", 0)))
            .field("noise", float(data["noise"]))
            .field("light", float(data["light"]))
        )
        self._write_api.write(bucket=self.bucket, org=self.org, record=point,
                              write_precision=WritePrecision.S)
        log.debug("Scritto punto telemetry desk_id=%s", data["desk_id"])

    def write_event(self, data: dict) -> None:
        """Scrive un evento ricevuto via MQTT (fase 05). Il timestamp e' quello
        server-side (write time): il nodo manda solo l'uptime ts_ms, non l'epoch.
        Solleva in caso di errore DB/rete (il chiamante decide come reagire)."""
        point = (
            Point(EVENTS_MEASUREMENT)
            .tag("desk_id", str(data["desk_id"]))
            .tag("event", str(data["event"]))
            .field("noise", float(data.get("noise", 0.0)))
            .field("light", float(data.get("light", 0.0)))
            .field("occupied", 1 if data.get("occupied") else 0)
        )
        self._write_api.write(bucket=self.bucket, org=self.org, record=point,
                              write_precision=WritePrecision.S)
        log.debug("Scritto evento desk_id=%s event=%s", data["desk_id"], data["event"])

    def close(self) -> None:
        self._client.close()
