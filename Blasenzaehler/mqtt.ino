#include <PubSubClient.h>

static char pubtopic[128];
static char subtopic[128];
static char deviceID[13];

WiFiClient espClient;
PubSubClient mqttclient(espClient);

void generateDeviceID() {
  unsigned long chip = ESP.getChipId();
  unsigned long flash = ESP.getFlashChipId();
  snprintf(deviceID, sizeof(deviceID), "%06lx%02lx",
           chip & 0xFFFFFF, flash & 0xFF);
}

bool check_online() {
  return (mqttclient.connected() && (WiFi.status() == WL_CONNECTED));
}

bool MQTT_reconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    is_online = false;
    return false;
  }

  if (mqttclient.connected()) {
    is_online = true;
    return true;
  }

  char clientID[128];
  bool mqtt_connected = false;

  snprintf(clientID, sizeof(clientID), "micro-wifi-%s", deviceID);

  Serial.print("Attempting to connect to MQTT as ");
  Serial.println(clientID);

  if (mqttclient.connect(clientID, username, password)) {
    Serial.println("Connected to MQTT");
    mqttclient.subscribe(subtopic);
    mqtt_connected = true;
  } else {
    Serial.print("NOT connected to MQTT, state=");
    Serial.println(mqttclient.state());
  }

  is_online = mqtt_connected;
  return mqtt_connected;
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("]");
}

void prepMQTT() {
  generateDeviceID();

  snprintf(pubtopic, sizeof(pubtopic), "%s/%s",
           BASETOPIC.c_str(), "Blasen");
  snprintf(subtopic, sizeof(subtopic), "%s/%s/cmd",
           BASETOPIC.c_str(), deviceID);

  mqttclient.setServer(mqtt_server, mqtt_port);
  mqttclient.setCallback(callback);
  mqttclient.setKeepAlive(30);
}

static bool mqttConfigured() {
  return mqtt_server[0] != '\0';
}

static void putmqtt() {
  if (!mqttConfigured()) return;

  snprintf(pubtopic, sizeof(pubtopic), "%s/%s",
           BASETOPIC.c_str(), "Blasen");

  if (!MQTT_reconnect()) {
    Serial.println("MQTT nicht verfügbar – Blasenzähler bleibt erhalten.");
    return;
  }

  char payload[20];
  snprintf(payload, sizeof(payload), "%lu",
           (unsigned long)mqtt_bla);

  Serial.print("MQTT Blasen-Payload: ");
  Serial.println(payload);

  bool rc = mqttclient.publish(pubtopic, payload, true);

  Serial.print("publish = ");
  Serial.println(rc ? "true" : "false");

  // Nur bei tatsächlich erfolgreichem Publish löschen.
  if (rc) {
    mqtt_bla = 0;
  }
}

static void putmqtttemp() {
  if (!mqttConfigured()) return;

  snprintf(pubtopic, sizeof(pubtopic), "%s/%s",
           BASETOPIC.c_str(), "Temperatur");

  if (!MQTT_reconnect()) {
    Serial.println("MQTT nicht verfügbar – Temperatur wird nicht publiziert.");
    return;
  }

  char payload[20];
  dtostrf(tempbz, 1, 2, payload);

  Serial.print("MQTT Temperatur-Payload: ");
  Serial.println(payload);

  bool rc = mqttclient.publish(pubtopic, payload, true);

  Serial.print("publish = ");
  Serial.println(rc ? "true" : "false");

  // WICHTIG: mqtt_bla wird hier NICHT verändert.
}
