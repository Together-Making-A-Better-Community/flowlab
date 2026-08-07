# Pi 5 Hub

> Central hub for the Flow Lab system. Receives sensor data, stores it, and serves it to a dashboard. Currently running the Seedling Monitor subsystem; will expand to handle Uno Q boards as they come online.

---

## What This Device Does

The Pi sits between the sensor hardware and anything a person actually looks at. Right now it runs two services for the seedling germination monitor:

- **MQTT subscriber** (`seedling.service`) — receives sensor readings published by the ESP8266 seedling sensor over MQTT, writes them to PostgreSQL.
- **Web dashboard** (`seedling_dashboard.service`) — a Flask app serving current/historical readings in a browser, on port 5000.

As additional devices (Uno Q boards) come online, this Pi will handle their MQTT topics and database writes too, following the same pattern.

---

## Folder Contents

```
devices/pi5-hub/
├── services/
│   ├── seedling_monitor.py         ← MQTT subscriber → writes to PostgreSQL
│   └── seedling_dashboard.py       ← Flask dashboard (port 5000)
├── scripts/
│   ├── seedling.service             ← systemd unit for the MQTT subscriber
│   └── seedling_dashboard.service   ← systemd unit for the Flask dashboard
├── tailscale/
│   └── setup.md                     ← remote SSH/admin access setup
└── README.md                        ← this file
```

---

## Services Running

| Service | What it does | Port |
|---|---|---|
| `seedling.service` | MQTT subscriber, logs readings to PostgreSQL | — |
| `seedling_dashboard.service` | Flask dashboard, view current/historical readings | 5000 |
| `mosquitto` | MQTT broker, receives data from the ESP8266 | 1883 |
| `postgresql` | Database — stores all sensor readings | 5432 |
| `tailscaled` | Remote SSH/admin access (see `tailscale/setup.md`) | — |

---

## Databases

Two separate PostgreSQL databases live on this Pi — **don't mix them up**:

| Database | Owner | Used for |
|---|---|---|
| `seedlingdb` | `seedling` | Seedling monitor readings (active) |
| `flowlab` | `postgres` / `flowlab_user` | Reserved for the broader Flow Lab system, Uno Q boards once online |

Schema for `seedlingdb` is versioned at `database/seedlingdb_schema.sql` in the repo root.

---

## Managing the Services

Always manage these through `systemctl`, never run the `.py` files directly in a terminal — doing both at once causes a port conflict (this happened once already with the dashboard; see Known Issues).

```bash
# Check status
sudo systemctl status seedling.service
sudo systemctl status seedling_dashboard.service

# Restart after a code change
sudo systemctl restart seedling.service
sudo systemctl restart seedling_dashboard.service

# View live logs
journalctl -u seedling.service -f
journalctl -u seedling_dashboard.service -f
```

---

## Setup on a Fresh Pi (or a Second Hub)

1. Clone with a sparse checkout so only this device's folder (plus `shared` and `database`) comes down:
   ```bash
   git clone --filter=blob:none --sparse https://github.com/Together-Making-A-Better-Community/flowlab.git
   cd flowlab
   git sparse-checkout set devices/pi5-hub shared database
   ```
2. Install PostgreSQL, Mosquitto, and Python dependencies.
3. Recreate the `seedlingdb` database and load `database/seedlingdb_schema.sql`.
4. Copy the two `.service` files from `devices/pi5-hub/scripts/` into `/etc/systemd/system/`, then:
   ```bash
   sudo systemctl daemon-reload
   sudo systemctl enable seedling.service seedling_dashboard.service
   sudo systemctl start seedling.service seedling_dashboard.service
   ```
5. Set up Tailscale for remote access — see `tailscale/setup.md`.
6. Set up an SSH key for pushing back to GitHub (password auth won't work) — see the repo's `Git and VS Code Command Walkthrough` note for the exact steps.

---

## Known Issues

- **Port conflict history:** `seedling_dashboard.service` once failed repeatedly because a manually-started copy of `seedling_dashboard.py` was already holding port 5000. Resolved by killing the manual process and letting systemd own it exclusively. If the dashboard shows `failed` in `systemctl status`, check for a stray manual process first: `ps aux | grep seedling_dashboard`.
- Dashboard is only reachable on the local network / via Tailscale currently — no public-facing access yet (Cloudflare Tunnel is the planned next step).
- Database credentials are currently hardcoded in the service scripts — should move to a gitignored config file before wider distribution.

---

## Next Steps

- [ ] Move hardcoded DB credentials into a gitignored `config.yaml` + `config.example.yaml` template
- [ ] Add Cloudflare Tunnel for public dashboard access
- [ ] Add `requirements.txt` for this device's Python dependencies
- [ ] Extend to handle Uno Q board MQTT topics once those come online

---

## References
- [[Seedling Sensor System]] — full engineering notebook entry for this subsystem
- [[Firmware Software Apps and Connectivity]] — how the layers connect
- `devices/esp8266-seedling/README.md` — the device this hub receives data from