#!/usr/bin/env python3
"""
Seedling Monitor — MQTT to PostgreSQL bridge
Runs on Raspberry Pi 5.

Sensors (matches seedling_monitor_main.ino v2):
  air_temp_c    — DHT11 air temperature (°C)
  humidity      — DHT11 humidity (%)
  soil_temp_c   — DS18B20 soil temperature (probe in soil) (°C)
  soil_raw      — Soil sensor raw ADC value (0-1023, through the divider)
  soil_percent  — Soil moisture as 0-100%, 100 = fully wet
  trigger       — why this reading was sent: "boot" | "manual" | "scheduled"

soil_wet is kept as a legacy column so old rows still read back fine, but
new firmware doesn't send it — it'll be NULL on every row from here on.
"""

import json
import logging
import signal
import sys

import psycopg2
import psycopg2.extras
import paho.mqtt.client as mqtt


# ── Configuration ─────────────────────────────────────────────────────────────

DB_HOST = "localhost"
DB_NAME = "seedlingdb"
DB_USER = "seedling"
DB_PASS = "seedling123"

MQTT_HOST  = "localhost"
MQTT_PORT  = 1883
MQTT_TOPIC = "seedling/data"

ALERT_TEMP_HIGH_C      = 35.0   # Air temp above this is too warm
ALERT_SOIL_TEMP_LOW_C  = 10.0   # Soil temp below this may stress roots
ALERT_SOIL_DRY_PERCENT = 25     # Soil moisture below this needs water


# ── Logging ───────────────────────────────────────────────────────────────────

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s  %(levelname)-8s  %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
    handlers=[
        logging.StreamHandler(sys.stdout),
        logging.FileHandler("/home/rashawnoverton/seedling.log"),
    ],
)
log = logging.getLogger("seedling")


# ── Database ──────────────────────────────────────────────────────────────────

def get_conn():
    return psycopg2.connect(
        host=DB_HOST, dbname=DB_NAME, user=DB_USER, password=DB_PASS
    )


def init_db():
    conn = get_conn()
    cur  = conn.cursor()
    cur.execute("""
        CREATE TABLE IF NOT EXISTS readings (
            id           SERIAL  PRIMARY KEY,
            received_at  TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
            device_id    TEXT,
            air_temp_c   REAL,
            humidity     REAL,
            soil_temp_c  REAL,
            soil_wet     SMALLINT   -- legacy: 1 = moist, 0 = dry (old firmware only)
        )
    """)
    # Adds the new columns on Pis that already had the table from before —
    # safe to run every startup, a no-op once the columns exist.
    cur.execute("ALTER TABLE readings ADD COLUMN IF NOT EXISTS soil_raw INTEGER")
    cur.execute("ALTER TABLE readings ADD COLUMN IF NOT EXISTS soil_percent INTEGER")
    cur.execute("ALTER TABLE readings ADD COLUMN IF NOT EXISTS trigger_type TEXT")
    conn.commit()
    cur.close()
    conn.close()
    log.info("Database ready — table 'readings' in '%s'", DB_NAME)


def store(data: dict):
    conn = get_conn()
    cur  = conn.cursor()
    cur.execute(
        """
        INSERT INTO readings
            (device_id, air_temp_c, humidity, soil_temp_c, soil_raw, soil_percent, trigger_type)
        VALUES (%s, %s, %s, %s, %s, %s, %s)
        """,
        (
            data.get("device_id"),
            data.get("air_temp_c"),
            data.get("humidity"),
            data.get("soil_temp_c"),
            data.get("soil_raw"),
            data.get("soil_percent"),
            data.get("trigger"),
        ),
    )
    conn.commit()
    cur.close()
    conn.close()


# ── Alerts ────────────────────────────────────────────────────────────────────

def check_alerts(data: dict):
    air      = data.get("air_temp_c")
    soil_temp = data.get("soil_temp_c")
    soil_pct = data.get("soil_percent")

    if soil_pct is not None and soil_pct < ALERT_SOIL_DRY_PERCENT:
        log.warning(
            "🌱 SOIL DRY — %d%% (threshold %d%%) — seedlings may need water",
            soil_pct, ALERT_SOIL_DRY_PERCENT,
        )

    if air is not None and air > ALERT_TEMP_HIGH_C:
        log.warning("🌡️  AIR TEMP HIGH — %.1f°C (threshold %.0f°C)", air, ALERT_TEMP_HIGH_C)

    if soil_temp is not None and soil_temp < ALERT_SOIL_TEMP_LOW_C:
        log.warning("🌡️  SOIL TEMP LOW — %.1f°C (threshold %.0f°C)", soil_temp, ALERT_SOIL_TEMP_LOW_C)


# ── MQTT Callbacks ────────────────────────────────────────────────────────────

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        log.info("Connected to MQTT broker — subscribing to '%s'", MQTT_TOPIC)
        client.subscribe(MQTT_TOPIC)
    else:
        log.error("MQTT connection refused (rc=%d)", rc)


def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode("utf-8"))

        log.info(
            "[%s]  trigger=%s  Air=%.1f°C  Hum=%.1f%%  SoilTemp=%.1f°C  SoilRaw=%s  Soil=%s%%",
            data.get("device_id", "?"),
            data.get("trigger", "?"),
            data.get("air_temp_c")   or 0,
            data.get("humidity")     or 0,
            data.get("soil_temp_c")  or 0,
            data.get("soil_raw", "?"),
            data.get("soil_percent", "?"),
        )

        store(data)
        check_alerts(data)

    except json.JSONDecodeError as exc:
        log.error("Bad JSON: %s", exc)
    except Exception as exc:
        log.error("Error: %s", exc)


def on_disconnect(client, userdata, rc):
    if rc != 0:
        log.warning("Unexpected disconnect (rc=%d)", rc)


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    init_db()

    client = mqtt.Client(client_id="seedling_pi", clean_session=True)
    client.on_connect    = on_connect
    client.on_message    = on_message
    client.on_disconnect = on_disconnect
    client.reconnect_delay_set(min_delay=2, max_delay=30)

    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)

    def _shutdown(sig, frame):
        log.info("Shutting down...")
        client.disconnect()
        sys.exit(0)

    signal.signal(signal.SIGINT,  _shutdown)
    signal.signal(signal.SIGTERM, _shutdown)

    log.info("Listening for sensor data...")
    client.loop_forever()


if __name__ == "__main__":
    main()
