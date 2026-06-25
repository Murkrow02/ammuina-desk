"""Server CoAP della telemetria (fase 04).

Riceve lo stesso payload JSON dell'HTTP su POST coap://<host>:5683/telemetry e lo
scrive su InfluxDB tramite lo stesso InfluxWriter della fase 03. Pensato per girare
in un thread daemon affiancato al server Flask (vedi main.py).
"""
import asyncio
import json
import logging

import aiocoap
import aiocoap.resource as resource
from aiocoap.numbers.codes import Code

from influx import InfluxWriter
from telemetry import validate

log = logging.getLogger("proxy.coap")


class TelemetryResource(resource.Resource):
    """Risorsa /telemetry: accetta POST con payload JSON identico all'HTTP."""

    def __init__(self, writer: InfluxWriter) -> None:
        super().__init__()
        self.writer = writer

    async def render_post(self, request) -> aiocoap.Message:
        try:
            payload = json.loads(request.payload.decode("utf-8"))
        except (ValueError, UnicodeDecodeError):
            log.warning("Telemetria CoAP: payload non JSON")
            return aiocoap.Message(code=Code.BAD_REQUEST, payload=b"invalid json")

        err = validate(payload)
        if err:
            log.warning("Telemetria CoAP rifiutata: %s", err)
            return aiocoap.Message(code=Code.BAD_REQUEST, payload=err.encode())

        try:
            self.writer.write_telemetry(payload)
        except Exception as exc:  # noqa: BLE001 — non far cadere il server per errori DB/rete
            log.error("Scrittura InfluxDB (CoAP) fallita: %s", exc)
            return aiocoap.Message(code=Code.SERVICE_UNAVAILABLE,
                                   payload=b"influx write failed")

        log.info("Telemetria CoAP desk_id=%s occupied=%s noise=%.3f light=%.1f",
                 payload["desk_id"], payload["occupied"], float(payload["noise"]),
                 float(payload["light"]))
        return aiocoap.Message(code=Code.CHANGED)  # 2.04


async def _serve(writer: InfluxWriter, bind: str, port: int) -> None:
    root = resource.Site()
    root.add_resource(["telemetry"], TelemetryResource(writer))
    await aiocoap.Context.create_server_context(root, bind=(bind, port))
    log.info("CoAP server up — udp/%s:%d  (POST /telemetry)", bind, port)
    await asyncio.get_running_loop().create_future()  # gira finche' il processo vive


def run_coap_server(writer: InfluxWriter, port: int = 5683, bind: str = "0.0.0.0") -> None:
    """Avvia il server CoAP su un proprio event loop. Bloccante: lanciare in un thread.

    `bind` default 0.0.0.0 (accetta dall'ESP32 in LAN). Su macOS il transport di
    fallback di aiocoap non si lega all'any-address: per test locali usare 127.0.0.1.
    """
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    try:
        loop.run_until_complete(_serve(writer, bind, port))
    except Exception as exc:  # noqa: BLE001 — logga e termina solo il thread CoAP
        log.error("CoAP server terminato: %s", exc)
