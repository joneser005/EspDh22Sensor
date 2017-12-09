/*
ESP8266 + DHT22 sensor
*/
#include <ESP8266WiFi.h>
#include <PubSubClient.h>  /* https://pubsubclient.knolleary.net/api.html */
#include "dht.h"

#include "local-config.h"

const unsigned long ENV_UPDATE_SECS = 5*60;
#define DHT22_PIN 5

/****************************************************************************************/
dht DHT;
WiFiClient wifiClient;
void mqtt_callback(char* topic, byte* payload, unsigned int length);
PubSubClient mqttClient(MQTT_HOST, MQTT_PORT, mqtt_callback, wifiClient);
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


bool readDht22() {
  bool result = false;
  int chk = DHT.read22(DHT22_PIN);
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
  connectWifi();
  connectMqtt();

  if (readDht22()) {
    publishFloat(TOPIC_ENV_TEMP, ctof(DHT.temperature));
    float h = DHT.humidity + DHT_HUMID_OFFSET;
    /*
     * from dht11 
     if (h > 100) {
      h = 99;
    }
    */
    publishFloat(TOPIC_ENV_HUMID, h);
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
  while (millis() < lastPubTime + MQTT_PUB_SPAN_MS) {
    delay(20);
  }
  if (mqttClient.publish(topic, val, true)) {
    lastPubTime = millis();
    Serial.println("succeeded");
  } else {
    Serial.println("FAILED!");
//    mqttClient.disconnect();
  }
}


float lastRecordedTemp = 0.0;
float lastRecordedHumidity = 0.0;
float tempChangeThreshold = 0.5;
float humidityChangeThreshold = 1.0;

unsigned long timeToSendEnvReadings = 0;
/****************************************************************************************/
/****************************************************************************************/
void loop() {
//  delay(250);
  mqttClient.loop();
  if (millis() > timeToSendEnvReadings) {
    Serial.println("Time to update environment readings.....");
    sendEnvReadings();
    timeToSendEnvReadings = millis() + (1000 * ENV_UPDATE_SECS);
  }
}
