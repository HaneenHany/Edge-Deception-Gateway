#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <time.h>
#include <HTTPClient.h>      // OTA
#include <HTTPUpdate.h>      // OTA

#define DHT_PIN 19
#define DHT_TYPE DHT11
#define VOLTAGE_PIN 32

#include "secrets.h"   // Wi-Fi / MQTT credentials and IPs (not committed, see secrets.h.example)

// ---------------- Wi-Fi ----------------
const char* WIFI_SSID     = SECRET_WIFI_SSID;
const char* WIFI_PASSWORD = SECRET_WIFI_PASSWORD;

// ---------------- MQTT (Mosquitto on Raspberry Pi, TLS) ----------------
const char* MQTT_SERVER = SECRET_MQTT_HOST;   // Raspberry Pi IP
const int   MQTT_PORT   = 8883;              // MQTT over TLS

const char* MQTT_CLIENT_ID  = "esp32-Device1";
const char* MQTT_USER       = "esp32-lab-01";
const char* MQTT_PASS       = SECRET_MQTT_PASS;

const char* TOPIC_TELEMETRY = "edgeguard/v1/telemetry";              // JSON with all values
const char* TOPIC_STATUS    = "edgeguard/v1/status/esp32-lab-01";    // online / offline (retained)

// ---------------- OTA (local HTTP file server) ----------------
// IP of the computer running the Python web server and the port it listens on.
// It must be reachable from the same network as the ESP32.
const char* SERVER_HOST = SECRET_OTA_SERVER_HOST;  // <-- your PC's LAN IP
const int   SERVER_PORT = 8080;

// CHANGE THIS FOR THE UPDATED FIRMWARE (v1.1) THAT YOU PLACE ON THE SERVER
const char* LOCAL_VERSION = "1.0";

const unsigned long otaCheckInterval = 30000;   // check for an update every 30 s
unsigned long lastOtaCheck = 0;

// ---------------- LED (pin 2) ----------------
const int LED_PIN = 2;
const unsigned long ledInterval = 500;   // toggles every 500 ms (1 s blink cycle)
unsigned long lastLedToggle = 0;
bool ledState = false;

// ---------------- CA certificate (IIoT-Local-CA) ----------------
const char* CA_CERT = " "

// ---------------- Voltage sensor ----------------
const float VOLTAGE_FACTOR = 7.5f;    // voltage sensor/divider ratio
float VOLTAGE_GAIN = 1.0f;            // V_multimeter / V_esp32

// ---------------- ACS712 current sensor (5A module) ----------------
// Wiring:
//   ACS712 VCC -> 5V (VIN)      ACS712 GND -> GND
//   ACS712 OUT -> 10k -> GPIO33 -> 20k -> GND   (voltage divider, ratio 2/3)
const int   ADC_PIN      = 33;
const float MV_PER_AMP   = 185.0;                 // 5A = 185, 20A = 100, 30A = 66
const float DIV_RATIO    = 20.0 / (10.0 + 20.0);  // 20k / (10k + 20k)
const bool  AC_MODE      = false;                 // true = AC (RMS), false = DC
const int   SAMPLE_MS    = 200;                   // 200ms = 10 cycles at 50Hz
const float DEADBAND_MA  = 5.0;                   // 5 mA or less is shown as 0

float offset_mV = 2500.0;                         // zero-current voltage (measured at startup)

DHT dht(DHT_PIN, DHT_TYPE);
WiFiClientSecure secureClient;                    // TLS connection
PubSubClient mqttClient(secureClient);

unsigned long lastPublish = 0;
const unsigned long publishInterval = 5000;

// ---------------- Voltage helpers ----------------
// Average many samples to reduce noise. Returns volts at the ADC pin.
float readAvgVoltage(int pin, int samples = 100) {
  uint32_t sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogReadMilliVolts(pin);
    delayMicroseconds(200);
  }
  return (sum / (float)samples) / 1000.0f;
}

float readVoltage() {
  return readAvgVoltage(VOLTAGE_PIN) * VOLTAGE_FACTOR * VOLTAGE_GAIN;
}

// ---------------- ACS712 current ----------------
// Sensor voltage in mV after undoing the divider
float readSensor_mV() {
  return analogReadMilliVolts(ADC_PIN) / DIV_RATIO;
}

// Measure the zero-current offset. The load must be disconnected.
void calibrateOffset() {
  Serial.println("Keep current at ZERO during ACS712 calibration!");
  delay(1000);

  double sum = 0;
  const int N = 1000;
  for (int i = 0; i < N; i++) {
    sum += readSensor_mV();
    delayMicroseconds(200);
  }
  offset_mV = sum / N;

  Serial.printf("ACS712 zero offset: %.1f mV (expected ~2500 mV)\n", offset_mV);
  if (offset_mV < 2300 || offset_mV > 2700) {
    Serial.println("WARNING: zero offset far from 2500 mV. Check 5V supply, common GND, divider.");
  }
}

// One reading = current over one sampling window (RMS for AC, mean for DC), in mA
float readWindow_mA() {
  double sum = 0;
  long n = 0;
  unsigned long start = millis();

  while (millis() - start < SAMPLE_MS) {
    float v = readSensor_mV() - offset_mV;
    sum += AC_MODE ? (double)v * v : v;
    n++;
  }

  float mV = AC_MODE ? sqrt(sum / n) : (sum / n);
  return (mV / MV_PER_AMP) * 1000.0;   // mA
}

// Average several windows, then apply the deadband. Returns mA. (takes ~1 second)
float readCurrent() {
  const int AVG_COUNT = 5;             // 5 x 200ms = 1 second
  float average = 0;
  for (int i = 0; i < AVG_COUNT; i++) {
    average += readWindow_mA();
  }

  float mA = fabs(average / AVG_COUNT);
  if (mA <= DEADBAND_MA) mA = 0;
  return mA;
}

// ---------------- Connectivity ----------------
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// TLS certificate validation needs the real date/time
void syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.google.com");   // UTC

  Serial.print("Syncing time");
  time_t now = time(nullptr);
  int tries = 0;
  while (now < 1700000000 && tries < 30) {               // wait up to ~15 s
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    tries++;
  }

  if (now < 1700000000) {
    Serial.println("\nWARNING: time sync failed. TLS certificate check will fail.");
  } else {
    struct tm t;
    gmtime_r(&now, &t);
    Serial.printf("\nTime (UTC): %04d-%02d-%02d %02d:%02d:%02d\n",
                  t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                  t.tm_hour, t.tm_min, t.tm_sec);
  }
}

void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.printf("Connecting to Mosquitto (TLS) at %s:%d ...\n", MQTT_SERVER, MQTT_PORT);

    // Last Will: broker publishes "offline" (retained) if the ESP32 disconnects unexpectedly
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS,
                           TOPIC_STATUS, 1, true, "offline")) {
      Serial.println("Mosquitto connected!");
      mqttClient.publish(TOPIC_STATUS, "online", true);
    } else {
      Serial.print("MQTT error: ");
      Serial.println(mqttClient.state());
      char errBuf[100];
      secureClient.lastError(errBuf, sizeof(errBuf));
      Serial.printf("TLS error: %s\n", errBuf);
      delay(3000);
    }
  }
}

// ---------------- OTA update (pull from local file server) ----------------
String baseURL() {
  return "http://" + String(SERVER_HOST) + ":" + String(SERVER_PORT);
}

// Keep only version characters (digits and dots). This strips a UTF-8 BOM
// (Windows Notepad adds the hidden bytes EF BB BF to the start of a file),
// trailing newlines/spaces, and any other stray control characters that
// would otherwise corrupt the version comparison.
String sanitizeVersion(const String& raw) {
  String clean = "";
  for (size_t i = 0; i < raw.length(); i++) {
    char c = raw.charAt(i);
    if ((c >= '0' && c <= '9') || c == '.') clean += c;
  }
  return clean;
}

// Fetch the version string from the server (CodeVersion.txt).
// Retries once, since the first HTTP request occasionally returns -1.
String fetchServerVersion() {
  WiFiClient client;
  HTTPClient http;
  String url = baseURL() + "/CodeVersion.txt";

  http.begin(client, url);
  int code = http.GET();

  if (code <= 0) {  // transient failure - retry once
    Serial.printf("CodeVersion.txt GET failed (%d), retrying...\n", code);
    http.end();
    delay(200);
    http.begin(client, url);
    code = http.GET();
  }

  String ver = "";
  if (code == HTTP_CODE_OK) {  // status 200
    ver = http.getString();
    ver.trim();
    ver = sanitizeVersion(ver);
  } else {
    Serial.printf("CodeVersion.txt GET failed, code %d\n", code);
  }
  http.end();
  return ver;
}

// Returns true if "server" is newer than "local" (e.g. 1.0 vs 1.1 vs 2.0).
bool isNewer(const String& server, const String& local) {
  int sMaj = 0, sMin = 0, lMaj = 0, lMin = 0;
  sscanf(server.c_str(), "%d.%d", &sMaj, &sMin);
  sscanf(local.c_str(), "%d.%d", &lMaj, &lMin);
  if (sMaj != lMaj) return sMaj > lMaj;
  return sMin > lMin;
}

void doUpdate() {
  String url = baseURL() + "/firmwareV1_1.bin";
  Serial.println("Downloading: " + url);

  WiFiClient client;

  // Throttled progress feedback
  httpUpdate.onProgress([](int cur, int total) {
    static unsigned int lastPct = 101;
    unsigned int pct = total ? (cur * 100) / total : 0;
    if (pct != lastPct) {
      lastPct = pct;
      Serial.printf("Update progress: %u%%\r", pct);
    }
  });

  httpUpdate.rebootOnUpdate(true);
  t_httpUpdate_return ret = httpUpdate.update(client, url);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("\nUpdate failed (%d): %s\n",
                    httpUpdate.getLastError(),
                    httpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("\nNo updates");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("\nUpdate OK - rebooting");
      break;
  }
}

void checkForUpdate() {
  Serial.println("Checking for update...");
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi down");
    return;
  }

  String serverVer = fetchServerVersion();
  if (serverVer.length() == 0) {
    return;
  }

  Serial.println("Current version: " + String(LOCAL_VERSION));
  Serial.println("Server version:  " + serverVer);

  if (isNewer(serverVer, LOCAL_VERSION)) {
    Serial.println("Update available. Downloading newer update...");
    delay(800);
    doUpdate();
  } else {
    Serial.println("Up to date.");
  }
}

// ---------------- Telemetry ----------------
void sendTelemetry() {
  float temperature = dht.readTemperature();
  float humidity    = dht.readHumidity();

  // Raw value for debugging
  float adcVoltageV = readAvgVoltage(VOLTAGE_PIN);

  float voltage    = readVoltage();
  float current_mA = readCurrent();
  float current_A  = current_mA / 1000.0f;
  float power      = voltage * current_A;  // valid for DC only

  // ---- Serial output with units ----
  Serial.println("--------------------------------");
  if (isnan(temperature)) Serial.println("Temperature : N/A (DHT11 read failed)");
  else                    Serial.printf("Temperature : %.1f C\n", temperature);

  if (isnan(humidity))    Serial.println("Humidity    : N/A (DHT11 read failed)");
  else                    Serial.printf("Humidity    : %.1f %%\n", humidity);

  Serial.printf("Voltage     : %.2f V\n", voltage);
  Serial.printf("Current     : %.1f mA (%.3f A)\n", current_mA, current_A);
  Serial.printf("Power       : %.2f W\n", power);
  Serial.println("[debug]");
  Serial.printf("ACS712 zero offset: %.1f mV\n", offset_mV);
  Serial.printf("ADC (voltage pin) : %.3f V\n", adcVoltageV);

  // ---- Build JSON, skipping failed DHT fields so the payload stays valid ----
  char payload[256];
  size_t n = 0;

  n += snprintf(payload + n, sizeof(payload) - n, "{");
  if (!isnan(temperature)) {
    n += snprintf(payload + n, sizeof(payload) - n, "\"temperature\":%.1f,", temperature);
  }
  if (!isnan(humidity)) {
    n += snprintf(payload + n, sizeof(payload) - n, "\"humidity\":%.1f,", humidity);
  }
  snprintf(payload + n, sizeof(payload) - n,
           "\"voltage\":%.2f,\"current\":%.3f,\"power\":%.2f}",
           voltage, current_A, power);

  bool sent = mqttClient.publish(TOPIC_TELEMETRY, payload);

  Serial.println(payload);
  Serial.println(sent ? "Telemetry sent!" : "Telemetry failed!");
}

// Send 'z' in the Serial Monitor (with NO current flowing) to re-zero the ACS712
void handleSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'z' || c == 'Z') {
      calibrateOffset();
    }
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  delay(1000);

  dht.begin();

  analogReadResolution(12);
  analogSetPinAttenuation(VOLTAGE_PIN, ADC_11db);
  analogSetPinAttenuation(ADC_PIN, ADC_11db);

  connectWiFi();
  syncTime();

  secureClient.setCACert(CA_CERT);   // verify the Pi's certificate against your CA
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setBufferSize(256);

  calibrateOffset();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
    if (time(nullptr) < 1700000000) {   // wait until NTP has set the clock
    Serial.println("Waiting for NTP time...");
    delay(1000);
    return;
  }

  if (!mqttClient.connected()) {
    connectMQTT();
  }

  mqttClient.loop();
  handleSerialCommands();

  // LED blink (non-blocking)
  if (millis() - lastLedToggle >= ledInterval) {
    lastLedToggle = millis();
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }

  if (millis() - lastPublish >= publishInterval) {
    lastPublish = millis();
    sendTelemetry();
  }

  // OTA: check the file server for a newer version every 30 s
  if (millis() - lastOtaCheck >= otaCheckInterval) {
    lastOtaCheck = millis();
    checkForUpdate();
  }
}// Main sketch: combines telemetry and OTA update
