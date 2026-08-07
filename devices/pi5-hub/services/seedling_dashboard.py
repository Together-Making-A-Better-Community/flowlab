#!/usr/bin/env python3
"""
Seedling Monitor Dashboard
Open http://192.168.7.129:5000 from any device on your network.

Run: python3 seedling_dashboard.py
"""

import psycopg2
import psycopg2.extras
from flask import Flask, jsonify, render_template_string

DB_HOST = "localhost"
DB_NAME = "seedlingdb"
DB_USER = "seedling"
DB_PASS = "seedling123"

SOIL_DRY_PERCENT = 25   # below this, the dashboard flags soil as needing water

app = Flask(__name__)


def get_conn():
    return psycopg2.connect(
        host=DB_HOST, dbname=DB_NAME, user=DB_USER, password=DB_PASS
    )


def query():
    conn = get_conn()
    cur  = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
    cur.execute("SELECT * FROM readings ORDER BY id DESC LIMIT 1")
    latest = cur.fetchone()
    cur.execute("SELECT * FROM readings ORDER BY id DESC LIMIT 30")
    history = cur.fetchall()
    cur.execute("SELECT COUNT(*) AS total FROM readings")
    total = cur.fetchone()["total"]
    cur.close()
    conn.close()
    return latest, history, total


PAGE = """
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="refresh" content="60">
<title>🌱 Seedling Monitor</title>
<style>
:root{--green:#22c55e;--amber:#f59e0b;--red:#ef4444;--blue:#38bdf8;
      --bg:#0f172a;--card:#1e293b;--muted:#94a3b8;--text:#e2e8f0;--border:#334155}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font-family:system-ui,sans-serif;
     padding:24px 20px;max-width:860px;margin:0 auto}
h1{color:var(--green);font-size:22px;margin-bottom:4px}
.sub{color:var(--muted);font-size:13px;margin-bottom:22px}
h2{font-size:13px;color:var(--muted);text-transform:uppercase;
   letter-spacing:.7px;margin:22px 0 10px}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));
       gap:12px;margin-bottom:16px}
.card{background:var(--card);border:1px solid var(--border);
      border-radius:10px;padding:18px 16px;text-align:center}
.card .val{font-size:34px;font-weight:700;line-height:1.1}
.card .lbl{font-size:12px;color:var(--muted);margin-top:5px}
.green{color:var(--green)}.amber{color:var(--amber)}
.red{color:var(--red)}.blue{color:var(--blue)}
.alert{border-radius:8px;padding:10px 14px;margin-bottom:10px;
       font-size:13px;border:1px solid}
.a-dry{background:#422006;border-color:#92400e;color:#fde68a}
.a-hot{background:#450a0a;border-color:#991b1b;color:#fca5a5}
.a-cold{background:#0c2a3d;border-color:#1e4a6e;color:#bae6fd}
table{width:100%;border-collapse:collapse;background:var(--card);
      border-radius:10px;overflow:hidden;border:1px solid var(--border);font-size:13px}
th{padding:10px 12px;text-align:left;color:var(--muted);
   font-size:11px;text-transform:uppercase;background:#0f172a}
td{padding:9px 12px;border-bottom:1px solid #0f172a}
tr:last-child td{border-bottom:none}
.pill{display:inline-block;padding:2px 10px;border-radius:999px;font-size:11px;font-weight:600}
.pill-wet{background:#14532d;color:#86efac}
.pill-dry{background:#450a0a;color:#fca5a5}
.trigger{font-size:10px;color:var(--muted);text-transform:uppercase}
footer{margin-top:20px;color:var(--muted);font-size:12px;text-align:center}
</style>
</head>
<body>

<h1>🌱 Seedling Monitor</h1>
<p class="sub">Auto-refreshes every 60 s · {{ device }} · PostgreSQL</p>

{% for a in alerts %}<div class="alert {{ a.cls }}">{{ a.msg }}</div>{% endfor %}

{% if latest %}
<div class="cards">

  <div class="card">
    <div class="val {{ 'red' if latest.air_temp_c and latest.air_temp_c > 35 else 'green' }}">
      {% if latest.air_temp_c is not none %}{{ '%.1f'|format(latest.air_temp_c * 9/5 + 32) }}°F
      {% else %}—{% endif %}
    </div>
    <div class="lbl">Air Temp</div>
  </div>

  <div class="card">
    <div class="val green">
      {% if latest.humidity is not none %}{{ '%.0f'|format(latest.humidity) }}%
      {% else %}—{% endif %}
    </div>
    <div class="lbl">Humidity</div>
  </div>

  <div class="card">
    <div class="val {{ 'amber' if latest.soil_temp_c and latest.soil_temp_c < 10 else 'blue' }}">
      {% if latest.soil_temp_c is not none %}{{ '%.1f'|format(latest.soil_temp_c * 9/5 + 32) }}°F
      {% else %}—{% endif %}
    </div>
    <div class="lbl">Soil Temp</div>
  </div>

  <div class="card">
    <div class="val {{ 'red' if latest.soil_percent is not none and latest.soil_percent < SOIL_DRY_PERCENT else 'green' }}">
      {% if latest.soil_percent is not none %}{{ latest.soil_percent }}%{% else %}—{% endif %}
    </div>
    <div class="lbl">Soil Moisture{% if latest.soil_raw is not none %} · raw {{ latest.soil_raw }}{% endif %}</div>
  </div>

</div>
<p style="font-size:12px;color:var(--muted);margin-bottom:22px">
  Last reading: {{ latest.received_at.strftime('%Y-%m-%d %H:%M:%S') }}
  {% if latest.trigger_type %}· <span class="trigger">{{ latest.trigger_type }}</span>{% endif %}
</p>
{% else %}
<p style="color:var(--muted);margin-bottom:22px">No readings yet — waiting for the sensor.</p>
{% endif %}

<h2>Recent History</h2>
<table>
  <tr>
    <th>Time</th><th>Air (°F)</th><th>Humidity</th>
    <th>Soil Temp (°F)</th><th>Soil %</th><th>Trigger</th>
  </tr>
  {% for r in history %}
  <tr>
    <td>{{ r.received_at.strftime('%m/%d %H:%M') }}</td>
    <td>{{ '%.1f'|format(r.air_temp_c * 9/5 + 32)   if r.air_temp_c   is not none else '—' }}</td>
    <td>{{ '%.0f'|format(r.humidity)      if r.humidity      is not none else '—' }}{{ '%' if r.humidity is not none }}</td>
    <td>{{ '%.1f'|format(r.soil_temp_c * 9/5 + 32) if r.soil_temp_c is not none else '—' }}</td>
    <td>
      {% if r.soil_percent is not none %}
        <span class="pill {{ 'pill-dry' if r.soil_percent < SOIL_DRY_PERCENT else 'pill-wet' }}">
          {{ r.soil_percent }}%
        </span>
      {% else %}—{% endif %}
    </td>
    <td>{{ r.trigger_type or '—' }}</td>
  </tr>
  {% endfor %}
</table>

<footer>{{ total }} readings · {{ db_name }}@{{ db_host }}</footer>
</body>
</html>
"""


@app.route("/")
def index():
    latest, history, total = query()
    alerts = []
    device = latest["device_id"] if latest else "no data yet"

    if latest:
        if latest["soil_percent"] is not None and latest["soil_percent"] < SOIL_DRY_PERCENT:
            alerts.append({
                "cls": "a-dry",
                "msg": f"⚠️ Soil moisture is low ({latest['soil_percent']}%) — seedlings may need water.",
            })
        if latest["air_temp_c"] and latest["air_temp_c"] > 35:
            air_f = latest["air_temp_c"] * 9/5 + 32
            alerts.append({"cls": "a-hot", "msg": f"🌡️ Air temperature is high ({air_f:.1f}°F)."})
        if latest["soil_temp_c"] and latest["soil_temp_c"] < 10:
            soil_f = latest["soil_temp_c"] * 9/5 + 32
            alerts.append({"cls": "a-cold", "msg": f"🌡️ Soil temperature is low ({soil_f:.1f}°F) — may stress roots."})

    return render_template_string(
        PAGE, latest=latest, history=history, alerts=alerts,
        total=total, device=device, db_name=DB_NAME, db_host=DB_HOST,
        SOIL_DRY_PERCENT=SOIL_DRY_PERCENT,
    )


@app.route("/api/latest")
def api_latest():
    latest, _, total = query()
    if not latest:
        return jsonify({"error": "no data yet"}), 404
    return jsonify({
        "received_at":   str(latest["received_at"]),
        "device_id":     latest["device_id"],
        "air_temp_c":    latest["air_temp_c"],
        "humidity":      latest["humidity"],
        "soil_temp_c":   latest["soil_temp_c"],
        "soil_raw":      latest["soil_raw"],
        "soil_percent":  latest["soil_percent"],
        "trigger":       latest["trigger_type"],
        "total":         total,
    })


@app.route("/api/history")
def api_history():
    _, history, _ = query()
    return jsonify([{
        "received_at":  str(r["received_at"]),
        "air_temp_c":   r["air_temp_c"],
        "humidity":     r["humidity"],
        "soil_temp_c":  r["soil_temp_c"],
        "soil_raw":     r["soil_raw"],
        "soil_percent": r["soil_percent"],
        "trigger":      r["trigger_type"],
    } for r in history])


if __name__ == "__main__":
    print("Dashboard → http://192.168.7.129:5000")
    app.run(host="0.0.0.0", port=5000, debug=False)
