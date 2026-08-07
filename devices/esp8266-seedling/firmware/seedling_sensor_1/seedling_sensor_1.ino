/*
 * Seedling Monitor — Main Sketch
 * ESP8266 SparkFun Thing Dev
 *
 * Reads all three sensors every 15 minutes, sends data to
 * Raspberry Pi via MQTT over WiFi, then deep sleeps.
 *
 * Wiring:
 *   DHT11    DATA → GPIO12  (module version, no pull-up needed)
 *   DS18B20  DATA → GPIO13  (4.7kΩ pull-up between 3.3V and GPIO13)
 *   Soil     DOUT → GPIO4   (use DOUT not AOUT)
 *   Soil     VCC  → GPIO14  (switched power prevents corrosion)
 *   GPIO16   → RST          (required for deep sleep to work)
 *
 * Libraries (install via Arduino Library Manager):
 *   DHT sensor library  — by Adafruit
 *   OneWire             — by Paul Stoffregen
 *   DallasTemperature   — by Miles Burton
 *   PubSubClient        — by Nick O'Leary
 *   ArduinoJson         — by Benoit Blanchon
 *
 * Board: Tools → Board → ESP8266 Boards → SparkFun ESP8266 Thing Dev
 */

#include <ESP8266WiFi.h>
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

// Seconds between readings. 900 = 15 minutes.
const uint32_t SLEEP_SEC = 900;

// Seconds to wait for WiFi before giving up.
const int WIFI_TIMEOUT_SEC = 20;


// ─────────────────────────────────────────
// PINS
// ─────────────────────────────────────────

#define DHT_PIN       12   // DHT11 data
#define DHT_TYPE      DHT11
#define DS18B20_PIN   13   // DS18B20 OneWire data
#define SOIL_DOUT_PIN  4   // Soil sensor digital output
#define SOIL_PWR_PIN  14   // Soil sensor power (switched)


// ─────────────────────────────────────────
// OBJECTS
// ─────────────────────────────────────────

DHT              dht(DHT_PIN, DHT_TYPE);
OneWire          oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
WiFiClient       wifiClient;
PubSubClient     mqtt(wifiClient);


// ─────────────────────────────────────────
// SENSOR READS
// ─────────────────────────────────────────

bool readSoilMoist() {
  // Power the probes only during the read.
  // Leaving a resistive sensor powered continuously causes
  // electrolytic corrosion on the probes — this prevents that.
  digitalWrite(SOIL_PWR_PIN, HIGH);
  delay(300);
  // DOUT: LOW = moist, HIGH = dry
  bool moist = !digitalRead(SOIL_DOUT_PIN);
  digitalWrite(SOIL_PWR_PIN, LOW);
  return moist;
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


// ─────────────────────────────────────────
// MQTT
// ─────────────────────────────────────────

bool connectMQTT() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  Serial.print("MQTT: connecting");

  for (int i = 0; i < 5; i++) {
    if (mqtt.connect(DEVICE_ID)) {
      Serial.println("  connected");
      return true;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("  failed");
  return false;
}


// ─────────────────────────────────────────
// SETUP — all logic runs here before sleep
// ─────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== Seedling Monitor ===");

  // Setup pins
  pinMode(SOIL_PWR_PIN, OUTPUT);
  digitalWrite(SOIL_PWR_PIN, LOW);
  pinMode(SOIL_DOUT_PIN, INPUT);

  // Start sensors
  dht.begin();
  ds18b20.begin();
  delay(2000);  // DHT11 needs 2 seconds after power-on

  // ── Read all sensors ──────────────────
  float airTemp   = dht.readTemperature();
  float humidity  = dht.readHumidity();
  float soilTemp = readSoilTemp();
  bool  soilMoist = readSoilMoist();

  // Retry DHT11 once if it failed
  if (isnan(airTemp) || isnan(humidity)) {
    Serial.println("DHT11 retry...");
    delay(2500);
    airTemp  = dht.readTemperature();
    humidity = dht.readHumidity();
  }

  // Print to serial for debugging
  Serial.printf("  Air Temp:   %.1f°C\n",  isnan(airTemp)  ? 0 : airTemp);
  Serial.printf("  Humidity:   %.1f%%\n",  isnan(humidity) ? 0 : humidity);

  if (soilTemp == DEVICE_DISCONNECTED_C) {
    Serial.println("  Soil Temp: SENSOR NOT FOUND");
  } else {
    Serial.printf("  Soil Temp: %.2f°C\n", soilTemp);
  }

  Serial.printf("  Soil:       %s\n", soilMoist ? "MOIST" : "DRY");

  // ── Send via MQTT ─────────────────────
  if (connectWiFi() && connectMQTT()) {

    // Build JSON payload
    // All three sensor values go into one message sent to the Pi
    StaticJsonDocument<200> doc;
    doc["device_id"] = DEVICE_ID;

    if (!isnan(airTemp))  doc["air_temp_c"]  = serialized(String(airTemp,  1));
    if (!isnan(humidity)) doc["humidity"]     = serialized(String(humidity, 1));

    if (soilTemp != DEVICE_DISCONNECTED_C) {
      doc["soil_temp_c"] = serialized(String(soilTemp, 2));
    }

    // 1 = moist, 0 = dry
    doc["soil_wet"] = soilMoist ? 1 : 0;

    char payload[200];
    serializeJson(doc, payload);
    Serial.printf("  Payload: %s\n", payload);

    // retained=true means the Pi gets the last reading
    // even if it was offline when this was sent
    bool sent = mqtt.publish(MQTT_TOPIC, payload, /*retained=*/true);
    Serial.printf("  Publish: %s\n", sent ? "OK" : "FAILED");

    mqtt.loop();
    delay(400);
    mqtt.disconnect();
    WiFi.disconnect(true);
    delay(200);

  } else {
    Serial.println("  No connection — reading skipped.");
  }

  // ── Deep sleep ────────────────────────
  // GPIO16 must be wired to RST for this to work.
  // The chip resets after SLEEP_SEC seconds and runs setup() again.
  Serial.printf("Sleeping %u seconds...\n\n", SLEEP_SEC);
  Serial.flush();
  ESP.deepSleep((uint64_t)SLEEP_SEC * 1000000UL, WAKE_RF_DEFAULT);
}


// loop() is never reached — deep sleep resets the chip each cycle.
void loop() {}
