# Flow Lab

> Open-source hydroponic and germination monitoring system for the Flow Lab + Sustainability Hub, built by Together Making A Better Community (TMBC). One monorepo, one folder per physical device.

**Current status: v0.2.0-in-progress** — Seedling Monitor subsystem is live and running. Broader Flow Lab hydroponics system (Uno Q boards) is in planning/build.

---

## What's Running Right Now

**Seedling Monitor** — an ESP8266-based sensor that tracks air temperature, humidity, soil temperature, and soil moisture for a germination tray, publishing readings over MQTT to a Raspberry Pi, which logs them to PostgreSQL and serves a live dashboard.

```
ESP8266 (firmware) → MQTT (Mosquitto on Pi) → PostgreSQL → Flask dashboard
```

See [`devices/esp8266-seedling/README.md`](devices/esp8266-seedling/README.md) and [`devices/pi5-hub/README.md`](devices/pi5-hub/README.md) for details on each piece.

---

## Repository Structure

```
flowlab/
├── devices/                 ← one folder per physical device
│   ├── esp8266-seedling/    ← seedling germination monitor (active)
│   └── pi5-hub/             ← central hub — MQTT broker, database, dashboard
├── database/                ← schema for every database in the system
├── shared/                  ← code shared across more than one device (empty for now)
├── docs/                    ← project-wide documentation
├── CHANGELOG.md
└── LICENSE                  ← MIT
```

Each device folder is self-contained with its own README. See [`docs`](docs/) or the individual device READMEs before making changes.

---

## Getting Set Up

**Full clone (laptop / dev machine):**
```bash
git clone https://github.com/Together-Making-A-Better-Community/flowlab.git
cd flowlab
```

**Sparse clone (a single field device, e.g. a Raspberry Pi that only runs one device folder):**
```bash
git clone --filter=blob:none --sparse https://github.com/Together-Making-A-Better-Community/flowlab.git
cd flowlab
git sparse-checkout set devices/pi5-hub shared database
```

---

## Branching

```
main       ← stable, deployed code only
  develop  ← integration branch, work lands here first
    feature/<device>-<what>   ← one branch per change
    fix/<device>-<what>
```

Day to day:
```bash
git checkout develop
git pull
git checkout -b feature/pi5-hub-my-change
# ...work, commit...
git push origin feature/pi5-hub-my-change
# open a PR: base develop, compare your feature branch
```

Never commit directly to `main` or `develop`. Devices in the field run off `main` only, tagged releases — `develop` is where in-progress work integrates safely without risking a live system.

---

## Versioning

Semantic versioning, one number for the whole repo (not per-device):
- **PATCH** — bug fix, no new capability
- **MINOR** — new sensor, new device folder, new feature, backward compatible
- **MAJOR** — breaking change (schema, MQTT topic structure, config format)

See `CHANGELOG.md` for release history.

---

## License

MIT — see `LICENSE`.