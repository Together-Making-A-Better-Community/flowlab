# ESP8266 Seedling Monitor

> Battery/wall-power environmental sensor for a seedling germination tray. Reads air temp, humidity, soil temp, and soil moisture, and publishes readings over MQTT to the Pi 5 Hub.

**Hardware:** SparkFun ESP8266 Thing Dev
**Status:** v2 (`seedling_monitor_main`) is the active firmware. v1 (`seedling_sensor_1`) is kept for reference only — see below.

---

## Folder Contents

```
devices/esp8266-seedling/
└── firmware/
    ├── seedling_monitor_main/     ← v2 — CURRENT, flash this one
    │   └── seedling_monitor_main.ino
    └── seedling_sensor_1/          ← v1 — original prototype, reference only
        └── seedling_sensor_1.ino
```

---

## v2 — `seedling_monitor_main` (Current)

Runs continuously on wall power (no deep sleep). Reads all sensors on a timer, publishes to MQTT, and stays connected so it can also accept OTA firmware updates and take an on-demand reading via MQTT instead of waiting for the next scheduled interval.

**Wiring:**

| Sensor | Pin | Notes |
|---|---|---|
| DHT11 (air temp + humidity) | GPIO12 | Module version, no external pull-up needed |
| DS18B20 (soil temp) | GPIO13 | 4.7kΩ pull-up between 3.3V and GPIO13 |
| Soil moisture | A0 | Through a voltage divider — AOUT → 33kΩ → A0, A0 → 10kΩ → GND (the ESP8266's ADC ceiling is ~1V, this sensor's AOUT swings higher without the divider) |
| Soil moisture (power) | GPIO14 | Switched power — prevents electrolytic corrosion on the probe |

`GPIO16 ↔ RST` is **not** wired in v2 — that was only needed for deep sleep, which this version doesn't use.

**Libraries (Arduino Library Manager):**
- DHT sensor library — Adafruit
- OneWire — Paul Stoffregen
- DallasTemperature — Miles Burton
- PubSubClient — Nick O'Leary
- ArduinoJson — Benoit Blanchon

**Board:** Tools → Board → ESP8266 Boards → SparkFun ESP8266 Thing Dev

---

## v1 — `seedling_sensor_1` (Reference Only)

The original prototype. Battery-oriented: reads sensors every 15 minutes, publishes over MQTT, then deep sleeps to conserve power. Used a digital (not analog) soil moisture reading.

Kept in the repo for reference since some wiring/library notes still apply, but this is **not** what should be flashed to the tray currently running on wall power. Superseded by v2.

---

## Known Issues / Next Steps

- [ ] WiFi credentials are still hardcoded in the `.ino` file — move to a config header (`config.example.h` pattern) before wider distribution, same fix planned for the Pi's config.
- [ ] Add a second tray (`seedling_tray_02`) to test multi-device MQTT once v2 is stable.
- [ ] Document OTA update process now that v2 supports it.

---

## References
- [[Seedling Sensor System]] — full engineering notebook entry
- `devices/pi5-hub/README.md` — what receives this device's MQTT data