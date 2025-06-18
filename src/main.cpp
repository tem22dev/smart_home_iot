#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

const char* ssid = "Huong Duong";
const char* password = "20122000";

// Thông tin broker MQTT
const char* mqtt_server = "192.168.1.10";
const int mqtt_port = 1883;
const char* mqtt_user = "";
const char* mqtt_password = "";

const char* deviceCode = "ESP32_ONE"; // ID duy nhất của ESP32

WiFiClient espClient;
PubSubClient client(espClient);

struct SensorConfig {
  String id;
  String name;
  int pin;
  String type;
  String unit;
  int threshold;
  bool status;
};

SensorConfig sensors[10]; // Mảng lưu cấu hình sensor, tối đa 10 sensor
int sensorCount = 0;

void connectWifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message received [");
  Serial.print(topic);
  Serial.println("] ");
  Serial.println(message);

  if (strcmp(topic, ("config/" + String(deviceCode)).c_str()) == 0) {
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    sensorCount = 0;
    JsonArray sensorArray = doc["sensors"];
    for (JsonObject sensor : sensorArray) {
      sensors[sensorCount].id = sensor["id"].as<String>();
      sensors[sensorCount].name = sensor["name"].as<String>();
      sensors[sensorCount].pin = sensor["pin"].as<int>();
      sensors[sensorCount].type = sensor["type"].as<String>();
      sensors[sensorCount].unit = sensor["unit"].as<String>();
      sensors[sensorCount].threshold = sensor["threshold"].as<int>();
      sensors[sensorCount].status = sensor["status"].as<bool>();

      if (sensors[sensorCount].type == "input") {
        pinMode(sensors[sensorCount].pin, INPUT);
        Serial.print(sensors[sensorCount].name);
        Serial.println(" configured as input");
      } else if (sensors[sensorCount].type == "output") {
        pinMode(sensors[sensorCount].pin, OUTPUT);
        digitalWrite(sensors[sensorCount].pin, sensors[sensorCount].status);
        Serial.print(sensors[sensorCount].name);
        Serial.println(sensors[sensorCount].status ? " ON" : " OFF");
      }
      sensorCount++;
    }
    Serial.println("Configuration applied");
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(("config/" + String(deviceCode)).c_str());
      client.publish("request/config", deviceCode);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  connectWifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}