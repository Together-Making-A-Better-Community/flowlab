/*
 * Seedling Monitor — Main Sketch (v2)
 * ESP8266 SparkFun Thing Dev
 *
 * Runs continuously (no deep sleep — the tray is on wall power now).
 * Reads all sensors on a timer, publishes to MQTT, stays connected
 * so it can also:
 *   - accept OTA firmware updates over WiFi
 *   - take an immediate reading on request via MQTT, instead of
 *     waiting for the next scheduled interval
 *
 * Wiring (see seedling_monitor_wiring.html for the diagram):
 *   DHT11    DATA  → GPIO12  (module version, no pull-up needed)
 *   DS18B20  DATA  → GPIO13  (4.7kΩ pull-up between 3.3V and GPIO13)
 *   Soil     AOUT  → A0      (through a divider — see below)
 *   Soil     VCC   → GPIO14  (switched power prevents corrosion)
 *
 *   Soil divider (required — this sensor's AOUT swings above the
 *   ESP8266 ADC's ~1V ceiling): AOUT → R1 33kΩ → A0, and A0 → R2
 *   10kΩ → GND. See seedling_monitor_wiring.html for the picture.
 *
 *   GPIO16↔RST is NOT wired in this version — that was only needed
 *   for deep sleep, which this sketch no longer uses.
 *
 * Libraries (install via Arduino Library Manager):
 *   DHT sensor library  — by Adafruit
 *   OneWire             — by Paul Stoffregen
 *   DallasTemperature   — by Miles Burton
 *   PubSubClient        — by Nick O'Leary
 *   ArduinoJson          — by Benoit Blanchon
 *   ArduinoOTA           — ships with the ESP8266 core, no install needed
 *
 * Board: Tools → Board → ESP8266 Boards → SparkFun ESP8266 Thing Dev
 *
 * OTA usage: after the FIRST upload over USB, this board will show up
 * in Arduino IDE under Tools → Port as a network port named after
 * DEVICE_ID (mDNS). Select that network port for all future uploads
 * and you won't need to plug it in again — set OTA_PASSWORD below
 * before you rely on this, since an unprotected OTA port accepts
 * firmware from anyone on the network.
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>


// ─────────────────────────────────────────
// SETTINGS — edit before uploading
// ─────────────────────────────────────────

const char* WIFI_SSID  = "Lincoln Wifi 2";
const char* WIFI_PASS  = "LincolnTMBC";
const char* MQTT_HOST  = "192.168.7.129";  // Raspberry Pi IP
const int   MQTT_PORT  = 1883;
const char* MQTT_TOPIC = "seedling/data";
const char* DEVICE_ID  = "seedling_tray_01";

// Set this before relying on OTA — an empty password means anyone on
// the network can push firmware to this board.
const char* OTA_PASSWORD = "";

// Seconds between scheduled readings. 900 = 15 minutes.
// A manual request over MQTT (see MQTT_CMD_TOPIC below) doesn't wait
// for this — it triggers a reading right away.
const uint32_t READ_INTERVAL_SEC = 900;

// Seconds to wait for the initial WiFi connection before giving up.
const int WIFI_TIMEOUT_SEC = 20;

// Soil raw-value calibration.
// Run soil_sensor_serial_test.ino first: note the raw value in dry
// air and the raw value with the probe in water, then replace these.
const int SOIL_RAW_DRY = 900;   // TODO: measured dry-air raw value
const int SOIL_RAW_WET = 300;   // TODO: measured in-water raw value


// ─────────────────────────────────────────
// PINS
// ─────────────────────────────────────────

#define DHT_PIN        12   // DHT11 data
#define DHT_TYPE       DHT11
#define DS18B20_PIN    13   // DS18B20 OneWire data
#define SOIL_AOUT_PIN  A0   // Soil sensor analog output (through divider)
#define SOIL_PWR_PIN   14   // Soil sensor power (switched)

const int SOIL_SAMPLES_TO_AVERAGE = 8;


// ─────────────────────────────────────────
// OBJECTS
// ─────────────────────────────────────────

DHT               dht(DHT_PIN, DHT_TYPE);
OneWire           oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
WiFiClient        wifiClient;
PubSubClient      mqtt(wifiClient);

String mqttCmdTopic;              // built in setup() from DEVICE_ID
volatile bool manualReadRequested = false;

unsigned long lastReadMillis     = 0;
unsigned long lastWifiRetryMillis = 0;
unsigned long lastMqttRetryMillis = 0;


// ─────────────────────────────────────────
// SENSOR READS
// ─────────────────────────────────────────

int readSoilRaw() {
  // Power the probes only during the read.
  // Leaving a resistive sensor powered continuously causes
  // electrolytic corrosion on the probes — this prevents that.
  digitalWrite(SOIL_PWR_PIN, HIGH);
  delay(300);  // let the sensor settle after power-up

  long total = 0;
  for (int i = 0; i < SOIL_SAMPLES_TO_AVERAGE; i++) {
    total += analogRead(SOIL_AOUT_PIN);   // 0-1023 on ESP8266
    delay(10);
  }

  digitalWrite(SOIL_PWR_PIN, LOW);
  return total / SOIL_SAMPLES_TO_AVERAGE;
}

// Raw value drops as soil gets wetter (the probe pulls the divider
// node toward GND as its resistance falls) — so a LOW raw number is
// wet and a HIGH raw number is dry. This maps that onto a normal
// 0-100% scale where 100% = fully wet.
int soilPercentFromRaw(int raw) {
  long percent = map(raw, SOIL_RAW_DRY, SOIL_RAW_WET, 0, 100);
  return constrain(percent, 0, 100);
}

float readSoilTemp() {
  // OneWire protocol: first request a reading, then fetch it.
  // getTempCByIndex(0) gets the first sensor found on the bus.
  ds18b20.requestTemperatures();
  return ds18b20.getTempCByIndex(0);
}


// ─────────────────────────────────────────
// WIFI
// ─────────────────────────────────────────

bool connectWiFi() {
  Serial.printf("WiFi: connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  for (int i = 0; i < WIFI_TIMEOUT_SEC * 2; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("  connected (%s)\n", WiFi.localIP().toString().c_str());
      return true;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("  timed out");
  return false;
}

// Called from loop(). Non-blocking-ish: only tries every 10s so a
// dead network doesn't stall OTA/MQTT handling forever.
void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWifiRetryMillis < 10000) return;

  lastWifiRetryMillis = millis();
  Serial.println("WiFi: reconnecting...");
  WiFi.reconnect();
}


// ─────────────────────────────────────────
// MQTT
// ─────────────────────────────────────────

// Fires when a message arrives on mqttCmdTopic. Payload content
// doesn't matter — any message on this topic means "read now."
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT: command received on ");
  Serial.println(topic);
  manualReadRequested = true;
}

bool connectMQTT() {
  Serial.print("MQTT: connecting");

  for (int i = 0; i < 5; i++) {
    if (mqtt.connect(DEVICE_ID)) {
      Serial.println("  connected");
      mqtt.subscribe(mqttCmdTopic.c_str());
      Serial.printf("  listening on %s for read requests\n", mqttCmdTopic.c_str());
      return true;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("  failed");
  return false;
}

// Called from loop(). Only retries every 5s.
void maintainMQTT() {
  if (mqtt.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastMqttRetryMillis < 5000) return;

  lastMqttRetryMillis = millis();
  connectMQTT();
}


// ─────────────────────────────────────────
// TAKE A READING AND PUBLISH IT
// ─────────────────────────────────────────

void takeReadingAndPublish(const char* trigger) {
  float airTemp  = dht.readTemperature();
  float humidity = dht.readHumidity();
  float soilTemp = readSoilTemp();
  int   soilRaw  = readSoilRaw();
  int   soilPct  = soilPercentFromRaw(soilRaw);

  // Retry DHT11 once if it failed
  if (isnan(airTemp) || isnan(humidity)) {
    Serial.println("DHT11 retry...");
    delay(2500);
    airTemp  = dht.readTemperature();
    humidity = dht.readHumidity();
  }

  Serial.printf("  [%s] Air Temp:   %.1f°C\n", trigger, isnan(airTemp) ? 0 : airTemp);
  Serial.printf("  Humidity:   %.1f%%\n", isnan(humidity) ? 0 : humidity);

  if (soilTemp == DEVICE_DISCONNECTED_C) {
    Serial.println("  Soil Temp: SENSOR NOT FOUND");
  } else {
    Serial.printf("  Soil Temp: %.2f°C\n", soilTemp);
  }

  Serial.printf("  Soil raw:   %d  (%d%% wet)\n", soilRaw, soilPct);

  if (!mqtt.connected()) {
    Serial.println("  MQTT not connected — reading not published.");
    return;
  }

  StaticJsonDocument<256> doc;
  doc["device_id"] = DEVICE_ID;
  doc["trigger"]   = trigger;

  if (!isnan(airTemp))  doc["air_temp_c"] = serialized(String(airTemp, 1));
  if (!isnan(humidity)) doc["humidity"]   = serialized(String(humidity, 1));
  if (soilTemp != DEVICE_DISCONNECTED_C) doc["soil_temp_c"] = serialized(String(soilTemp, 2));

  doc["soil_raw"]     = soilRaw;
  doc["soil_percent"] = soilPct;

  char payload[256];
  serializeJson(doc, payload);
  Serial.printf("  Payload: %s\n", payload);

  // retained=true means anything subscribing later still gets the
  // last reading, even if it wasn't listening when this was sent
  bool sent = mqtt.publish(MQTT_TOPIC, payload, /*retained=*/true);
  Serial.printf("  Publish: %s\n", sent ? "OK" : "FAILED");
}


// ─────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== Seedling Monitor v2 ===");

  pinMode(SOIL_PWR_PIN, OUTPUT);
  digitalWrite(SOIL_PWR_PIN, LOW);
  pinMode(SOIL_AOUT_PIN, INPUT);

  dht.begin();
  ds18b20.begin();
  delay(2000);  // DHT11 needs 2 seconds after power-on

  connectWiFi();

  // ── OTA ────────────────────────────────
  ArduinoOTA.setHostname(DEVICE_ID);
  if (strlen(OTA_PASSWORD) > 0) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  }
  ArduinoOTA.onStart([]() {
    Serial.println("OTA: update starting...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA: update complete, rebooting.");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA: %u%%\r", (progress * 100) / total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error [%u]\n", error);
  });
  ArduinoOTA.begin();
  Serial.println("OTA: ready");

  // ── MQTT ───────────────────────────────
  mqttCmdTopic = String("seedling/") + DEVICE_ID + "/read";
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  connectMQTT();

  // Take one reading right at boot instead of waiting a full interval
  takeReadingAndPublish("boot");
  lastReadMillis = millis();
}


// ─────────────────────────────────────────
// LOOP — runs continuously, never sleeps
// ─────────────────────────────────────────

void loop() {
  ArduinoOTA.handle();
  maintainWiFi();
  maintainMQTT();
  mqtt.loop();

  if (manualReadRequested) {
    manualReadRequested = false;
    takeReadingAndPublish("manual");
    lastReadMillis = millis();
  }

  if (millis() - lastReadMillis >= (unsigned long)READ_INTERVAL_SEC * 1000UL) {
    takeReadingAndPublish("scheduled");
    lastReadMillis = millis();
  }
}
