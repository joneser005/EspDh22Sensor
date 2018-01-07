/*
  ESP8266 + DHT22 sensor
*/
#include <ESP8266WiFi.h>  /* https://acrobotic.com/media/wysiwyg/products/esp8266_devkit_horizontal-01.png */
#include <PubSubClient.h>  /* https://pubsubclient.knolleary.net/api.html */

const unsigned long ENV_UPDATE_SECS = 30;
#define DHTPIN D2  /* GPIO numbers and pin numbers are maddeningly different! */

#include "dht.h"  /* https://playground.arduino.cc/Main/DHTLib */
dht DHT;
#include "local-config.h"


/****************************************************************************************/

WiFiClient wifiClient;
PubSubClient mqttClient(MQTT_HOST, MQTT_PORT, 0 /* no callback fn */, wifiClient);
float lastPubTime = 0;
// DHT11: Humidity sensor is reading low (the one on the 4-outlet box).
// Until we know more about its (in)accuracy, performing a
// crude correction to the value to known value.
// nov 2017 reads 21, actual is 45
// DHT22: Leave at zero if/until we need to change:
#define DHT_HUMID_OFFSET 0
/****************************************************************************************/

/****************************************************************************************/
/****************************************************************************************/
void setup() {
  Serial.begin(115200);
  delay(10);
  pinMode(DHTPIN, INPUT_PULLUP);
  delay(5000);
  Serial.println("Setup complete");
}
/****************************************************************************************/

float ctof(float c) {
  return (c * 1.8) + 32;
}

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_P);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("-");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Netmask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());
}

void disconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
  }
}

void connectMqtt() {
  if (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT server.....");
    char willTopic[27];
    sprintf(willTopic, "/status/%s", MQTT_CLIENT_ID);
    const char *willMsg = "Disconnected";

    mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_P, willTopic, 0, 1, willMsg);
    if (mqttClient.connected()) {
      Serial.println("OK");
    } else {
      Serial.println("FAILED!");
      Serial.print("MQTT state = ");
      Serial.println(mqttClient.state()); // neg=lost connection, pos=never had one
    }
  }
}

void disconnectMqtt() {
  if (mqttClient.connected()) {
    Serial.print("Disconnecting from MQTT server.....");
    mqttClient.disconnect();
  }
}

bool readDht22() {
  Serial.println("Reading DHT22.....");
  bool result = false;
  int chk = DHT.read22(DHTPIN);
  switch (chk) {
    case DHTLIB_OK:
      Serial.println("DHT22 READ OK");
      result = true;
      break;
    case DHTLIB_ERROR_CHECKSUM:
      Serial.println("DHT22 Checksum error");
      break;
    case DHTLIB_ERROR_TIMEOUT:
      Serial.println("DHT22 Time out error");
      break;
    default:
      Serial.println("DHT22 Unknown error");
      break;
  }
  return result;
}

void sendEnvReadings() {
  if (readDht22()) {
    Serial.print("Temp/humidity: ");
    Serial.print(DHT.temperature);
    Serial.print("/");
    Serial.print(DHT.humidity);

    connectWifi();
    connectMqtt();
    publishFloat(TOPIC_ENV_TEMP, ctof(DHT.temperature));
    float h = DHT.humidity + DHT_HUMID_OFFSET;
    publishFloat(TOPIC_ENV_HUMID, h);
    //disconnectMqtt();
    //disconnectWifi();
  }
}

void publishFloat(const char * topic, const float val) {
  char sval[10];
  dtostrf(val, 4, 2, sval);
  publishString(topic, sval);
}

void publishString(const char * topic, const char * val) {
  Serial.print("MQTT client is connected?  ");
  Serial.println(mqttClient.connected());
  Serial.print("Publish to topic [");
  Serial.print(topic);
  Serial.print("] value [");
  Serial.print(val);
  Serial.print("] ");
  if (mqttClient.publish(topic, val, true)) {
    lastPubTime = millis();
    Serial.println("succeeded");
  } else {
    Serial.println("FAILED!");
  }
}

/****************************************************************************************/
/****************************************************************************************/
void loop() {
  //  TODO: Deep sleep until time to next reading
  Serial.println("Updating environment readings.....");
  sendEnvReadings();
  mqttClient.loop();
  delay(1000 * ENV_UPDATE_SECS);
}
