#include <PubSubClient.h>


static char pubtopic[128];
static char subtopic[128];
static char deviceID[9];


WiFiClient espClient;
PubSubClient mqttclient(espClient);

void generateDeviceID()
{
  char flashChipId[6 + 1];
  sprintf(flashChipId, "%06x", ESP.getFlashChipId());
  snprintf(deviceID, sizeof(deviceID), "%06x%s",
           ESP.getChipId(), flashChipId + strlen(flashChipId) - 2);
}

bool check_online()
{
  return (mqttclient.connected() && (WiFi.status() == WL_CONNECTED));
}


bool MQTT_reconnect()
{
  char clientID[128];
  static char *willPayload = "{\"_type\":\"lwt\",\"tst\":0}";
  bool willRetain = false;
  char *willTopic = pubtopic;
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
  snprintf(pubtopic, sizeof(pubtopic), "%s/%s", BASETOPIC, "Blasen");
  snprintf(subtopic, sizeof(subtopic), "%s/%s/cmd", BASETOPIC, deviceID);
  mqttclient.setServer(mqtt_server, mqtt_port);
  mqttclient.setCallback(callback);
}

static void putmqtt()
{
  boolean rc;
  MQTT_reconnect();
  is_online = check_online();
  char payload[20];
  Serial.print("Status: ");
  Serial.println(is_online ? "Online" : "OFFline");
  String pls = ""+(String)mqtt_bla;
  Serial.println("Payload: " + pls);
  pls.toCharArray(payload,20);

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
