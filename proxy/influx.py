"""InfluxDB writer per la telemetria — Fase 03.

Schema measurement:
  measurement: "telemetry"
  tag:    desk_id
  fields: occupied (int 0/1), session_duration_s (int), noise (float), light (float)
"""
import logging

from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS

log = logging.getLogger("proxy.influx")

MEASUREMENT = "telemetry"


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

    def close(self) -> None:
        self._client.close()
