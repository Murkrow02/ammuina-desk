"""LibraryDeskSense data proxy — entrypoint.

Fase 00: avvia e logga lo stato. Le fasi successive aggiungono:
  - server HTTP telemetria + writer InfluxDB (fase 03)
  - server CoAP (fase 04)
  - client MQTT eventi/config (fase 05)
  - ingestione forecast/metriche (fase 07) e offloading log (fase 09)
"""
import logging
import os
import time

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")
log = logging.getLogger("proxy")


def main() -> None:
    log.info("LibraryDeskSense proxy up — phase 00 skeleton")
    log.info("influx_url=%s mqtt_host=%s", os.getenv("INFLUX_URL", "unset"), os.getenv("MQTT_HOST", "unset"))
    # Fase 03+: qui partiranno i server. Per ora resta vivo.
    while True:
        time.sleep(60)


if __name__ == "__main__":
    main()
