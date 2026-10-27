// Library needed (Library Manager): "PubSubClient" by Nick O'Leary
// Include required libraries
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <time.h>

#include "secrets.h"   // Wi-Fi / MQTT credentials and IPs (not committed, see secrets.h.example)

unsigned long StartMillis;
unsigned long CurrentMillis;
const unsigned long Period = 1000 * 30;  // 30 second

// USER CONFIGURATION
const char* WIFI_SSID = SECRET_WIFI_SSID;
const char* WIFI_PASSWORD = SECRET_WIFI_PASSWORD;

// IP address of the computer running the Python web server (OTA),
// and the port you started it on.
const char* SERVER_HOST = SECRET_OTA_SERVER_HOST;  // <-- your PC's LAN IP
const int SERVER_PORT = 8080;

// MQTT broker (the Raspberry Pi, TLS) for honeypot alerts
const char* MQTT_HOST = SECRET_MQTT_HOST;    // <-- your Pi's LAN IP
const uint16_t MQTT_PORT = 8883;             // MQTT over TLS
const char* MQTT_CLIENT_ID = "esp32-Device1";   // same as firmware 1.0
const char* MQTT_USER = "esp32-lab-01";         // same as firmware 1.0
const char* MQTT_PASS = SECRET_MQTT_PASS;                // same as firmware 1.0
const char* DEVICE_ID = "esp32-lab-01";         // used in the topic and the JSON

const char* LOCAL_VERSION = "1.1";

#define CHECK_ON_BOOT false  // set true to auto-check at startup

// ---------------- CA certificate (IIoT-Local-CA) ----------------
const char* CA_CERT = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFETCCAvmgAwIBAgIUfw9CKnreFY0xQ1SVh/YuYdPXT8UwDQYJKoZIhvcNAQEL
BQAwGDEWMBQGA1UEAwwNSUlvVC1Mb2NhbC1DQTAeFw0yNjEwMTUwNTQ0MjJaFw0z
NjEwMTIwNTQ0MjJaMBgxFjAUBgNVBAMMDUlJb1QtTG9jYWwtQ0EwggIiMA0GCSqG
SIb3DQEBAQUAA4ICDwAwggIKAoICAQDe1UCdN2VOQnqvuI1AKPm6nJgsjL51p0qc
UyxGT1rNqV86UbC/w77iXGes0gOWbLWWheXdFBDHH+2vDTD2gZroJDmZzlQk2gXh
Im1MnSlEsfP7VICOT75b5GtQLeRepUsVZJY6YuQoHDQOP2Ht0xPt0q97DrdlQG4h
+J5cLjwq2vBpShMHWZ9da/CtzTN5puvsXQF47BYAyR3+GH8XDTchcYEY5QYJCygY
BkEXeuFnpCq4sQ2GhG7v5v+6L0Nx3o3OSUgFG8Kt0wwusz2+9y+ZrSdjxrc8nuLt
1eQ96X3MvBXulqZIdQb+OlN5Ed4WTGBk2p3gTsiKaagdLAawp3iG3KxUqKc0npHy
MlntTrz5fui7UcoScJcQxhf1Lj/vuM0Kdstk4byWqg7T5T+DCwBTizxqKIma076Z
As0pTnHaGFAZ2y8/RlLmMzV3ZhZEBaU/vbh7ctLpOHZHdK0yBO6MVjegAURlEgHw
gtR3u/5sUPJ8CzF4FvQS1V05i8ocygmiC8URdU8KG2USRSjrAjJtytsZlDQPKya2
uF+7X3AdMaJ7SD0jMnqEw5A8JS7hSEXTxNQagy054VOHygVicqKIp98kQozEG1SM
j04bzcQ1TbAKzqo7OxX4S8PHa3Jt/8KQeUiMZ06uTdL/pqfKf7ji4Pl9zAy/QfZk
QOU5t/SjWQIDAQABo1MwUTAdBgNVHQ4EFgQUld3G+HDppyXDW6yFxiH9Zk/X9Q4w
HwYDVR0jBBgwFoAUld3G+HDppyXDW6yFxiH9Zk/X9Q4wDwYDVR0TAQH/BAUwAwEB
/zANBgkqhkiG9w0BAQsFAAOCAgEAGk2KU8sjPmLyETfKik1Ta1N43gRG+DKXUcQb
o/fo7BT08Cveo176Dj9JL+xF7qknFZV+vydkBNVAREezftE4okTgzcZNF25u3iV1
P/O9Yuk2yDK+4gYscS2Qz0Bc9dSxkgkEfYvSW8w3FEul7kFF2kRHijK7hNUN7cBm
/4TX/XF8xOiBuhYZcH6A6cZ6GTbFOa7FpiS6x2VgA14B5ktyNMm7qVfR7p+gem/R
28j8b7dPAFZPiHMT5FEAPZQ+lgZtr7PZygIl+qtXkjmTHWWPQeWDdKceN95+RJ4D
Oh5TWAXJqCdSNgScZYq/ix1rSE3PSEs+yHNqgq6wpSQZZL9xxW3K6fieJ9vlMuGp
AnLDk52nLKYJdu4tFobiPG+BXfyWxzOdRd8WtRUHh1feJ54nmWUFBlh5bZmKKRkJ
8XhBpYZaptirvInkV+5E7mVJjUHQBkiggeTOQN2KZuJE1Aj3DsQCrNvA2uagd+6x
5+Cews8a7ZWlEzYCPyoRNjkg43F8956kcIUVnb6vGK5olt+RWUNTRfMpFBdKV0XC
JunxLpabdlrQCoKmquuHIYStJ7y7pj6rwTxceSpB5wBAAV8xYXOHb+tWRVANdGcE
DhjcrLTK+C+2mff3N1wSvIyrQQCWsr680ecrMRlUPG1nZwLr8qgp7+12myjPTsQH
AfxhfK8=
-----END CERTIFICATE-----
)EOF";

// ---------- Honeypot / MQTT objects ----------
WiFiClientSecure mqttNet;          // TLS connection
PubSubClient mqtt(mqttNet);
WebServer honeypot(80);

unsigned long lastMqttTry = 0;
unsigned long lastPublish = 0;
const unsigned long MIN_PUBLISH_GAP_MS = 500;  // simple rate limit

// =====================================================
//                      OTA PART
// =====================================================
String baseURL()
{
  return "http://" + String(SERVER_HOST) + ":" + String(SERVER_PORT);
}

String sanitizeVersion(const String& raw) {
  String clean = "";
  for (size_t i = 0; i < raw.length(); i++) {
    char c = raw.charAt(i);
    if ((c >= '0' && c <= '9') || c == '.') clean += c;
  }
  return clean;
}

String fetchServerVersion()
{
  WiFiClient client;
  HTTPClient http;
  String url = baseURL() + "/CodeVersion.txt";

  http.begin(client, url);
  int code = http.GET();

  if (code <= 0)
  {  // transient failure - retry once
    Serial.printf("CodeVersion.txt GET failed (%d), retrying...\n", code);
    http.end();
    delay(200);
    http.begin(client, url);
    code = http.GET();
  }

  String ver = "";
  if (code == HTTP_CODE_OK)  // status 200
  {
    ver = http.getString();
    ver.trim();
    ver = sanitizeVersion(ver);  // strip BOM / whitespace / junk bytes
  }
  else
  {
    Serial.printf("CodeVersion.txt GET failed, code %d\n", code);
  }
  http.end();
  return ver;
}

// Returns true if "server" is older than "local".
// Simple dotted-numeric compare (e.g. 1.0 vs 1.1 vs 2.0).
bool isOlder(const String& server, const String& local)
{
  int sMaj = 0, sMin = 0, lMaj = 0, lMin = 0;
  sscanf(server.c_str(), "%d.%d", &sMaj, &sMin);
  sscanf(local.c_str(), "%d.%d", &lMaj, &lMin);
  if (sMaj != lMaj) return sMaj < lMaj;
  return sMin < lMin;
}

void doDowngrade()
{
  String url = baseURL() + "/firmwareV1_0.bin";
  Serial.println("Downloading: " + url);

  WiFiClient client;

  // Throttled progress feedback. Printing to serial every packet is slow.
  httpUpdate.onProgress([](int cur, int total)
  {
    static unsigned int lastPct = 101;
    unsigned int pct = total ? (cur * 100) / total : 0;
    if (pct != lastPct)
    {
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
      // Board reboots automatically; this line rarely prints
      Serial.println("\nUpdate OK - rebooting");
      break;
  }
}

void checkForUpdate()
{
  Serial.println("Checking for update...");
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi down");
    return;
  }

  String serverVer = fetchServerVersion();
  if (serverVer.length() == 0)
  {
    return;
  }

  Serial.println("Current version: " + String(LOCAL_VERSION));
  Serial.println("Server version:  " + serverVer);

  if (isOlder(serverVer, LOCAL_VERSION))
  {
    Serial.println("Update available. Downloading older firmware...");
    delay(800);
    doDowngrade();
  }
  else
  {
    Serial.println("Up to date.");
  }
}

// =====================================================
//                   HONEYPOT PART
// =====================================================
static String honeypotTopic() {
  return String("edgeguard/v1/honeypot/") + DEVICE_ID;
}

static String jsonSafe(String s) {  // never put raw attacker text into JSON
  if (s.length() > 80) s = s.substring(0, 80);
  s.replace("\\", "/");
  s.replace("\"", "'");
  s.replace("\r", " ");
  s.replace("\n", " ");
  return s;
}

static void honeypotHandler() {
  const String ip = honeypot.client().remoteIP().toString();
  const String method = (honeypot.method() == HTTP_POST) ? "POST" : "GET";
  const String body = String("{\"device_id\":\"") + DEVICE_ID +
                      "\",\"ip\":\"" + ip +
                      "\",\"method\":\"" + method +
                      "\",\"uri\":\"" + jsonSafe(honeypot.uri()) + "\"}";

  Serial.println("HONEYPOT hit from " + ip + " " + method + " " + jsonSafe(honeypot.uri()));

  unsigned long now = millis();
  if (mqtt.connected() && (now - lastPublish >= MIN_PUBLISH_GAP_MS)) {
    mqtt.publish(honeypotTopic().c_str(), body.c_str());
    lastPublish = now;
  }

  // decoy page that keeps the visitor busy
  honeypot.send(200, "text/html",
                "<html><body><h3>Smart Meter - Admin Login</h3>"
                "<form method='post'><input name='user'><input name='pass' type='password'>"
                "<button>Login</button></form></body></html>");
}

void startHoneypot() {
  honeypot.on("/", honeypotHandler);
  honeypot.onNotFound(honeypotHandler);
  honeypot.begin();
  Serial.println("Honeypot web server started on port 80");
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
  Serial.println(now < 1700000000 ? "\nTime sync FAILED" : "\nTime OK");
}

// non-blocking reconnect: tries every 5 seconds so nothing else gets stuck
static void ensureMqtt() {
  if (mqtt.connected()) return;
  unsigned long now = millis();
  if (now - lastMqttTry < 5000) return;
  lastMqttTry = now;
  Serial.print("Connecting to MQTT... ");
  if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
    Serial.println("ok");
  } else {
    Serial.print("failed, rc=");
    Serial.println(mqtt.state());
    char errBuf[100];
    mqttNet.lastError(errBuf, sizeof(errBuf));
    Serial.printf("TLS error: %s\n", errBuf);
  }
}

// =====================================================
//                   SETUP / LOOP
// =====================================================
void setup()
{
  Serial.begin(115200);
  delay(300);
  StartMillis = millis();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Current version: " + String(LOCAL_VERSION));

  syncTime();
  mqttNet.setCACert(CA_CERT);        // verify the Pi's certificate against your CA
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  startHoneypot();

  if (CHECK_ON_BOOT) checkForUpdate();
}

void loop()
{
  // reconnect WiFi if it dropped
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost, reconnecting...");
    WiFi.reconnect();
    delay(1000);
  }

  ensureMqtt();
  mqtt.loop();
  honeypot.handleClient();

  CurrentMillis = millis();
  if (CurrentMillis - StartMillis >= Period)  // test whether the period has elapsed
  {
    checkForUpdate();
    StartMillis = CurrentMillis;
  }
}// Honeypot firmware (version 2.0): sends fake telemetry and logs unauthorized commands. Owner: Haneen.
