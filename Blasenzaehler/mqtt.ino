#include <PubSubClient.h>


static char pubtopic[128];
static char subtopic[128];
static char deviceID[13];


WiFiClient espClient;
PubSubClient mqttclient(espClient);

void generateDeviceID()
{
  unsigned long chip = ESP.getChipId();
  unsigned long flash = ESP.getFlashChipId();
  snprintf(deviceID, sizeof(deviceID), "%06lx%02lx",
           chip & 0xFFFFFF, flash & 0xFF);
}


bool check_online()
{
  return (mqttclient.connected() && (WiFi.status() == WL_CONNECTED));
}


bool MQTT_reconnect()
{
  char clientID[128];
  bool mqtt_connected = false;
  snprintf(clientID, sizeof(clientID), "micro-wifi-%s", deviceID);
  Serial.print("Attempting to connect to MQTT as ");
  Serial.println(clientID);
  mqttclient.disconnect();
  if (mqttclient.connect(clientID, username, password)) {
    Serial.println("Connected to MQTT");
    mqttclient.subscribe(subtopic);
    mqtt_connected = true;
  } else {
    Serial.println("NOT connected to MQTT");
  }
  return (mqtt_connected);
}


void callback(char* topic, byte * payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
}


void prepMQTT() {
  generateDeviceID();
  snprintf(pubtopic, sizeof(pubtopic), "%s/%s", BASETOPIC.c_str(), "Blasen");
  snprintf(subtopic, sizeof(subtopic), "%s/%s/cmd", BASETOPIC.c_str(), deviceID);
  mqttclient.setServer(mqtt_server, mqtt_port);
  mqttclient.setCallback(callback);
}

static void putmqtt()
{
  if (mqtt_server == "") return;
  boolean rc;
  snprintf(pubtopic, sizeof(pubtopic), "%s/%s", BASETOPIC.c_str(), "Blasen");
  MQTT_reconnect();
  is_online = check_online();
  char payload[20];
  Serial.print("Status: ");
  Serial.println(is_online ? "Online" : "OFFline");
  String pls = "" + (String)mqtt_bla;
  Serial.println("Payload: " + pls);
  pls.toCharArray(payload, 20);

  if (is_online) {
    if ((rc = mqttclient.publish(pubtopic, payload, true)) == false) {
    }
    Serial.print(" publish = ");
    Serial.println(rc);
    mqtt_bla = 0;
    if (mqttclient.connected())
      mqttclient.loop();
  } else {
  }
}

static void putmqtttemp()
{
  if (mqtt_server == "") return;
  boolean rc;
  snprintf(pubtopic, sizeof(pubtopic), "%s/%s", BASETOPIC.c_str(), "Temperatur");
  MQTT_reconnect();
  is_online = check_online();
  char payload[20];
  Serial.print("Status: ");
  Serial.println(is_online ? "Online" : "OFFline");
  String pls = "" + (String)tempbz;
  Serial.println("Payload: " + pls);
  pls.toCharArray(payload, 20);

  if (is_online) {
    if ((rc = mqttclient.publish(pubtopic, payload, true)) == false) {
    }
    Serial.print(" publish = ");
    Serial.println(rc);
    mqtt_bla = 0;
    if (mqttclient.connected())
      mqttclient.loop();
  } else {
  }
}
