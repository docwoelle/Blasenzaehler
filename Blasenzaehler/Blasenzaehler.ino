#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <TimeLib.h>

const int FW_VERSION = 1;
const char* fwUrlBase = "http://192.168.2.22/esp/";
#define LED D5

WiFiManager wm;

#define DELAY 500   // ms
#define LONGDELAY 7000 // ms
uint8_t nextState = 0, fail = 0;
uint16_t IN1, LS1, BS1, OLDSTATUS1;
uint32_t mil = 0, diffms = 0, lastok = 0, nextstage = 0;
uint32_t wertalt = 0, wertdiff = 0, wertehz = 0;
long scantime1, updatets, id, lastDebounce1, lastDebounce2, DD1 = 100, id2;
int updateinterval = 60000;
bool fault = false, done = false, blink = false;
bool is_online = false;    // true if WiFi and MQTT connected
int j = 0, count1 = -1;
DynamicJsonDocument jsonBuffer(1024);
JsonObject root = jsonBuffer.to<JsonObject>();
const size_t MAX_CONTENT_SIZE = 512;
boolean fwcheck = false;
ESP8266WebServer server(80);
String cnt = "";

float blas[] = {0, 0, 0, 0, 0};
uint16_t blasval[] = {0, 0, 0, 0, 0};

volatile uint32_t mqtt_bla = 0;

uint32_t last = 0;
uint8_t stage = 0;
uint16_t ar = 0, arm = 0, minw = 32767;
const int sekunden = 60; // Zeitraum zur Ermittlung des Minimums
const float faktor = 1.3; // Minimalwerk * Faktor als Schwellwert
int sekMin[sekunden];
int ind = 0;
int sekCounter = 0;

char mqtt_server[40];
int mqtt_port = 1883;
char username[40];
char password[40];
String BASETOPIC = "monitor/Blasenzaehler";
String website = "Ballon 1";

/*++++++++++++++++++++++++++++
   KONFIG LADEN
  +++++++++++++++++++++++++++*/

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

  // Strings in char[] kopieren
  strlcpy(mqtt_server, doc["mqtt_server"] | "", sizeof(mqtt_server));
  mqtt_port = doc["mqtt_port"] | 1883;
  strlcpy(username, doc["username"] | "", sizeof(username));
  strlcpy(password, doc["password"] | "", sizeof(password));
  BASETOPIC = doc["BASETOPIC"] | "monitor/Blasenzaehler";
  website = doc["website"] | "Ballon 1";

  Serial.println("Config geladen:");
  Serial.printf("Server: %s\n", mqtt_server);
  Serial.printf("Port: %d\n", mqtt_port);
  Serial.printf("User: %s\n", username);
  Serial.printf("Pass: %s\n", password);
  Serial.printf("BaseTopic: %s\n", BASETOPIC.c_str());
}


/*++++++++++++++++++++++++++++++++++++++++++
   +++++++++ SETUP +++++++++++++++++++++++++
   ++++++++++++++++++++++++++++++++++++++ */


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(200);
  pinMode(LED, OUTPUT);
  LittleFS.begin();
  Serial.println("\nStarte Blasenzaehler...");
 // wm.resetSettings();
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


  uint8_t i = 0;
  fail = 0;

  // Setup OTA-Routines
  setupOTA();
  loadConfig();
  prepMQTT();

  scantime1 = millis();
  updatets  = millis();
  fwcheck = true;
  done = false;
  j = 0;
  server.on("/reboot", handleReboot);
  server.on("/data.json", handleDataJson);
  server.on("/config", handleConfig);
  server.on("/configsave", handleSaveConfig);
  server.on("/", handleRoot);
  server.onNotFound ( handleNotFound );
  server.begin(); // Web server start
  for (int i = 0; i < sekunden; i++) {
    sekMin[i] = 32767; // Platzhalter
  }
}


/*#######################################################
  ############## LOOP ##################################
  #######################################################*/

// aktuelles Minimum der letzten <sekunden> Sekunden berechnen
int getsecMin() {
  int m = 32767;
  for (int i = 0; i < sekunden; i++) {
    if (sekMin[i] < m) m = sekMin[i];
  }
  return m * faktor;
}

void loop() {
  if (fwcheck) {
    checkForUpdates();
    fwcheck = false;
  }

  server.handleClient();

  wm.process();

  if (blink) {
    if (stage == 0) {
      nextstage = millis();
      digitalWrite(LED, HIGH);
      stage = 1;
    }
    if ((stage == 1) && (millis() - nextstage > 50)) {
      nextstage = millis();
      digitalWrite(LED, LOW);
      stage = 0;
      blink = false;
    }
  }

  /*
    ##################################################################
    ######################## BLASEN ##################################
    ##################################################################
  */

  if (millis() - last > 10) {  // nur alle 10 ms lesen
    ar = analogRead(A0);
    if (arm < ar) arm = ar;
    IN1 = (ar > getsecMin() ? 1 : 0);
    last = millis();

    if (ar < minw) minw = ar;
    sekCounter++;
    if (sekCounter >= 100) { // 100 Messungen = 1 Sekunde
      sekMin[ind] = minw;
      ind = (ind + 1) % sekunden; // Ringpuffer
      minw = 32767;
      sekCounter = 0;
    }
  }

  // Entprellen
  if (IN1 != LS1) {
    lastDebounce1 = millis();
  }

  if ((millis() - lastDebounce1) > DD1) {
    if (BS1 != IN1) {
      BS1 = IN1;
    }
  }

  if ((OLDSTATUS1 != BS1)) {
    OLDSTATUS1 = BS1;
    if (BS1 == 1) {
      mqtt_bla++;
      Serial.println(mqtt_bla);
      putmqtt();
      blink = true;
      blas[4] = blas[3];
      blas[3] = blas[2];
      blas[2] = blas[1];
      blas[1] = blas[0];
      blas[0] = millis();
      blasval[4] = blasval[3];
      blasval[3] = blasval[2];
      blasval[2] = blasval[1];
      blasval[1] = blasval[0];
      blasval[0] = arm;
      arm = 0;
    }
  }

  LS1 = IN1;




  if (millis() - updatets > updateinterval) {
    j++;
    if (j == 10) {
      fwcheck = false;
      j = 0;
    }
  }
  server.handleClient();
  // OTA
  ArduinoOTA.handle();

  if (fail > 10) {
    delay(10000);
    ESP.restart();
  }
}
