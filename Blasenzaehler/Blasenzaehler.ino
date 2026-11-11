#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <TimeLib.h>
#include <OneWire.h>
#include <DallasTemperature.h>

const int FW_VERSION = 2;
const char* fwUrlBase = "http://192.168.2.22/esp/";
#define LED D5
#define ONE_WIRE_BUS D6

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
WiFiManager wm;

#define DELAY 500
#define LONGDELAY 7000

uint8_t nextState = 0, fail = 0;
uint16_t IN1 = 0, LS1 = 0, BS1 = 0, OLDSTATUS1 = 0;
uint32_t mil = 0, diffms = 0, last1820 = 0, nextstage = 0;
uint32_t wertalt = 0, wertdiff = 0, wertehz = 0;
uint32_t scantime1 = 0, updatets = 0, id = 0, lastDebounce1 = 0, lastDebounce2 = 0, DD1 = 25, id2 = 0;
uint32_t updateinterval = 60000;

bool fault = false, done = false, blink = false;
bool is_online = false;
int j = 0, count1 = -1;

DynamicJsonDocument jsonBuffer(1024);
JsonObject root = jsonBuffer.to<JsonObject>();
const size_t MAX_CONTENT_SIZE = 512;
bool fwcheck = false;
ESP8266WebServer server(80);
String cnt = "";

uint32_t blas[] = {0, 0, 0, 0, 0};
uint16_t blasval[] = {0, 0, 0, 0, 0};
uint8_t bubbleHistoryCount = 0;
float tempbz = 0;
volatile uint32_t mqtt_bla = 0;

uint32_t last = 0;
uint8_t stage = 0;
uint16_t ar = 0, arm = 0, minw = 65535;

const uint32_t SAMPLE_INTERVAL_MS = 2;
const uint32_t MIN_WINDOW_MS = 1000;
const uint32_t MIN_HISTORY_SECONDS = 60;
const float faktor = 1.3f;

int sekMin[MIN_HISTORY_SECONDS];
uint32_t minWindowStart = 0;
uint8_t minHistoryIndex = 0;
bool minWindowValid = false;
int cachedThreshold = 0;

char mqtt_server[40];
int mqtt_port = 1883;
char username[40];
char password[40];
String BASETOPIC = "monitor/Blasenzaehler";
String website = "Ballon 1";

void loadConfig() {
  if (!LittleFS.exists("/config.json")) {
    Serial.println("Keine config.json gefunden – Standardwerte bleiben erhalten.");
    return;
  }

  File file = LittleFS.open("/config.json", "r");
  if (!file) {
    Serial.println("Fehler beim Öffnen der config.json!");
    return;
  }

  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.print("Fehler beim Lesen der JSON-Datei: ");
    Serial.println(error.c_str());
    return;
  }

  strlcpy(mqtt_server, doc["mqtt_server"] | "", sizeof(mqtt_server));
  mqtt_port = doc["mqtt_port"] | 1883;
  if (mqtt_port < 1 || mqtt_port > 65535) mqtt_port = 1883;
  strlcpy(username, doc["username"] | "", sizeof(username));
  strlcpy(password, doc["password"] | "", sizeof(password));
  BASETOPIC = doc["BASETOPIC"] | "monitor/Blasenzaehler";
  website = doc["website"] | "Ballon 1";

  Serial.println("Config geladen:");
  Serial.printf("Server: %s\n", mqtt_server);
  Serial.printf("Port: %d\n", mqtt_port);
  Serial.printf("User: %s\n", username);
  Serial.println("Pass: ********");
  Serial.printf("BaseTopic: %s\n", BASETOPIC.c_str());
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(LED, OUTPUT);

  if (!LittleFS.begin()) {
    Serial.println("FEHLER: LittleFS konnte nicht gemountet werden.");
  }

  sensors.begin();

  Serial.println("\nStarte Blasenzaehler...");
  wm.setConfigPortalBlocking(false);

  if (!wm.autoConnect("Blasenzaehler")) {
    Serial.println("Keine Verbindung, Config-Portal aktiv.");
  } else {
    Serial.print("Verbunden mit WiFi. IP: ");
    Serial.println(WiFi.localIP());
  }

  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("BSSID: ");
  Serial.println(WiFi.BSSIDstr());
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Status: ");
  Serial.println(WiFi.status());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("Netzmaske: ");
  Serial.println(WiFi.subnetMask());

  fail = 0;

  setupOTA();
  loadConfig();
  prepMQTT();

  uint32_t nowMs = millis();
  scantime1 = nowMs;
  updatets = nowMs;
  minWindowStart = nowMs;
  cachedThreshold = 0;
  last = nowMs;
  last1820 = nowMs;
  fwcheck = true;
  done = false;
  j = 0;

  server.on("/reboot", handleReboot);
  server.on("/data.json", handleDataJson);
  server.on("/config", handleConfig);
  server.on("/configsave", handleSaveConfig);
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();

  for (uint8_t i = 0; i < MIN_HISTORY_SECONDS; i++) {
    sekMin[i] = 65535;
  }
}

// Gibt den Mindestwert der bereits vollständig erfassten 1-s-Fenster zurück.
// Während der ersten Sekunde wird noch kein gültiger Schwellwert verwendet.
int getsecMin() {
  if (!minWindowValid) return 0;

  int m = 65535;
  bool found = false;

  for (uint8_t i = 0; i < MIN_HISTORY_SECONDS; i++) {
    if (sekMin[i] != 65535) {
      if (sekMin[i] < m) m = sekMin[i];
      found = true;
    }
  }

  if (!found) return 0;

  long threshold = (long)(m * faktor);
  if (threshold > 1023) threshold = 1023;
  return (int)threshold;
}

void processAnalogSample(uint32_t nowMs) {
  ar = analogRead(A0);

  if (arm < ar) arm = ar;

  // In der Aufwärmphase noch keine Blase erkennen.
  IN1 = (cachedThreshold > 0 && ar > cachedThreshold) ? 1 : 0;

  if (ar < minw) minw = ar;

  if ((uint32_t)(nowMs - minWindowStart) >= MIN_WINDOW_MS) {
    sekMin[minHistoryIndex] = minw;
    minHistoryIndex = (minHistoryIndex + 1) % MIN_HISTORY_SECONDS;
    minw = 65535;
    minWindowStart += MIN_WINDOW_MS;
    minWindowValid = true;

    // Schwellwert nur einmal pro Fenster neu berechnen. Dadurch entfällt
    // das Scannen des kompletten 60-s-Ringpuffers bei jedem ADC-Sample.
    int m = 65535;
    for (uint8_t i = 0; i < MIN_HISTORY_SECONDS; i++) {
      if (sekMin[i] != 65535 && sekMin[i] < m) m = sekMin[i];
    }
    cachedThreshold = (m == 65535) ? 0 : min((int)(m * faktor), 1023);

    // Falls der ESP längere Zeit blockiert war, nicht viele veraltete
    // Sekundenfenster nachholen.
    if ((uint32_t)(nowMs - minWindowStart) >= MIN_WINDOW_MS) {
      minWindowStart = nowMs;
    }
  }
}

void loop() {
  uint32_t nowMs = millis();

  if (fwcheck) {
    checkForUpdates();
    fwcheck = false;
  }

  server.handleClient();
  wm.process();

  if (blink) {
    if (stage == 0) {
      nextstage = nowMs;
      digitalWrite(LED, HIGH);
      stage = 1;
    }

    if ((stage == 1) && ((uint32_t)(nowMs - nextstage) > 50)) {
      digitalWrite(LED, LOW);
      stage = 0;
      blink = false;
    }
  }

  // Analoge Messung exakt zeitgesteuert statt "100 Schleifendurchläufe = 1 s".
  if ((uint32_t)(nowMs - last) >= SAMPLE_INTERVAL_MS) {
    last = nowMs;
    processAnalogSample(nowMs);
    // analogRead(A0) ist synchron. Sofort danach dem ESP8266-WLAN-Stack
    // CPU-Zeit geben, damit Webserver/MQTT auch bei schneller Abtastung
    // nicht verhungern.
    yield();
  }

  if ((uint32_t)(nowMs - last1820) >= 60000UL) {
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);
    last1820 = nowMs;

    if (tempC != DEVICE_DISCONNECTED_C) {
      tempbz = tempC;
      putmqtttemp();
    }
  }

  // Entprellen
  if (IN1 != LS1) {
    lastDebounce1 = nowMs;
  }

  if ((uint32_t)(nowMs - lastDebounce1) > DD1) {
    if (BS1 != IN1) {
      BS1 = IN1;
    }
  }

  // Nur die steigende Flanke zählt als neue Blase.
  if (OLDSTATUS1 != BS1) {
    OLDSTATUS1 = BS1;

    if (BS1 == 1) {
      mqtt_bla++;
      Serial.print("Blasen seit letztem erfolgreichen MQTT-Publish: ");
      Serial.println(mqtt_bla);

      putmqtt();

      blink = true;

      blas[4] = blas[3];
      blas[3] = blas[2];
      blas[2] = blas[1];
      blas[1] = blas[0];
      blas[0] = nowMs;

      blasval[4] = blasval[3];
      blasval[3] = blasval[2];
      blasval[2] = blasval[1];
      blasval[1] = blasval[0];
      blasval[0] = arm;
      arm = 0;

      if (bubbleHistoryCount < 5) {
        bubbleHistoryCount++;
      }
    }
  }

  LS1 = IN1;

  // OTA/MQTT regelmäßig bedienen. mqtt_bla wird dabei NICHT zurückgesetzt.
  if (check_online()) {
    mqttclient.loop();
  }

  ArduinoOTA.handle();

  // Firmware-Updateprüfung alle 10 Minuten.
  if ((uint32_t)(nowMs - updatets) >= 600000UL) {
    updatets = nowMs;
    fwcheck = true;
  }

  if (fail > 10) {
    delay(10000);
    ESP.restart();
  }
}
