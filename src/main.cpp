#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

const char* ssid = "Huong Duong";
const char* password = "20122000";

// Thông tin broker MQTT
const char* mqtt_server = "192.168.1.10";
const int mqtt_port = 1883;
const char* mqtt_user = "";
const char* mqtt_password = "";

const char* deviceCode = "ESP32_ONE";

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
  DHT* dht;
};

SensorConfig sensors[10];
int sensorCount = 0;

bool deviceStatus = false;
bool isStatusReceived = false;

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
  Serial.print("] length: ");
  Serial.println(length);
  Serial.println(message);

  if (strcmp(topic, ("config/" + String(deviceCode)).c_str()) == 0) {
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    sensorCount = 0;
    for (int i = 0; i < sensorCount; i++) {
      delete sensors[i].dht;
    }

    if (doc.containsKey("sensors")) {
      JsonArray sensorArray = doc["sensors"];
      Serial.print("sensorArray size: ");
      Serial.println(sensorArray.size());
      for (JsonObject sensor : sensorArray) {
        if (sensorCount >= 10) {
          Serial.println("Max sensor limit reached!");
          break;
        }
        sensors[sensorCount].id = sensor["id"].as<String>();
        sensors[sensorCount].name = sensor["name"].as<String>();
        sensors[sensorCount].pin = sensor["pin"].as<int>();
        sensors[sensorCount].type = sensor["type"].as<String>();
        sensors[sensorCount].unit = sensor["unit"].as<String>();
        sensors[sensorCount].threshold = sensor["threshold"].as<int>();
        sensors[sensorCount].status = sensor["status"].as<bool>();
        sensors[sensorCount].dht = nullptr;

        if (sensors[sensorCount].type == "DHT") {
          sensors[sensorCount].dht = new DHT(sensors[sensorCount].pin, DHT22);
          sensors[sensorCount].dht->begin();
          Serial.print(sensors[sensorCount].name);
          Serial.println(" configured as DHT22 with dynamic pin");
        } else if (sensors[sensorCount].type == "MQ2") {
          pinMode(sensors[sensorCount].pin, INPUT);
          Serial.print(sensors[sensorCount].name);
          Serial.println(" configured as MQ2");
        }
        sensorCount++;
      }
    } else {
      Serial.println("No 'sensors' key found in config JSON!");
    }
    Serial.println("Configuration applied");
  } else if (strcmp(topic, ("status/" + String(deviceCode)).c_str()) == 0) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }
    bool newStatus = doc["status"].as<bool>();
    if (deviceStatus != newStatus) {
      deviceStatus = newStatus;
      isStatusReceived = true;
      Serial.print("Device status updated to: ");
      Serial.println(deviceStatus);
    }
  } else if (String(topic).startsWith("sensor/status/")) {
    String sensorId = String(topic).substring(strlen("sensor/status/"));
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }
    bool newStatus = doc["status"].as<bool>();

    for (int i = 0; i < sensorCount; i++) {
      if (sensors[i].id == sensorId) {
        if (sensors[i].status != newStatus) {
          sensors[i].status = newStatus;
          Serial.print("Sensor ");
          Serial.print(sensors[i].name);
          Serial.print(" status updated to: ");
          Serial.println(newStatus);
        }
        break;
      }
    }
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
      client.subscribe(("status/" + String(deviceCode)).c_str());
      client.subscribe("sensor/status/#");
      Serial.println("Subscribed to topics");
      client.publish("request/config", deviceCode, true); // Sử dụng QoS 1
      client.publish(("status/" + String(deviceCode)).c_str(), "", true); // Sử dụng QoS 1
      Serial.println("Published requests");
      isStatusReceived = false;
      delay(2000); // Chờ 2 giây để đảm bảo nhận thông điệp
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void sendSensorData() {
  if (!isStatusReceived) {
    Serial.println("Waiting for device status...");
    return;
  }
  if (!deviceStatus) {
    Serial.println("Device is disabled, skipping sensor data transmission");
    return;
  }
  if (sensorCount == 0) {
    Serial.println("No sensors configured, waiting for config...");
    return;
  }

  for (int i = 0; i < sensorCount; i++) {
    if (sensors[i].type == "DHT" && sensors[i].dht) {
      float temperature = sensors[i].dht->readTemperature();
      float humidity = sensors[i].dht->readHumidity();
      if (isnan(temperature) || isnan(humidity)) {
        Serial.print("Failed to read from ");
        Serial.print(sensors[i].name);
        Serial.println(" DHT22");
        continue;
      }

      if (sensors[i].status) {
        if (temperature >= 0) {
          DynamicJsonDocument doc(1024);
          doc["sensorId"] = sensors[i].id;
          doc["value"] = temperature;
          doc["unit"] = "°C";
          char buffer[128];
          serializeJson(doc, buffer);
          client.publish("sensor/data", buffer);
          Serial.print(sensors[i].name);
          Serial.print(" Temperature: ");
          Serial.println(temperature);
        }
        if (humidity >= 0) {
          DynamicJsonDocument doc(1024);
          doc["sensorId"] = sensors[i].id;
          doc["value"] = humidity;
          doc["unit"] = "%";
          char buffer[128];
          serializeJson(doc, buffer);
          client.publish("sensor/data", buffer);
          Serial.print(sensors[i].name);
          Serial.print(" Humidity: ");
          Serial.println(humidity);
        }
      } else {
        Serial.print("Sensor ");
        Serial.print(sensors[i].name);
        Serial.println(" is disabled, data not sent (but still reading)");
      }
    } else if (sensors[i].type == "MQ2") {
      int gasValue = analogRead(sensors[i].pin);
      if (sensors[i].status || gasValue >= sensors[i].threshold) {
        DynamicJsonDocument doc(1024);
        doc["sensorId"] = sensors[i].id;
        doc["value"] = gasValue;
        doc["unit"] = sensors[i].unit;
        char buffer[128];
        serializeJson(doc, buffer);
        client.publish("sensor/data", buffer);
      } else if (!sensors[i].status) {
        Serial.print("Sensor ");
        Serial.print(sensors[i].name);
        Serial.println(" is disabled, data not sent (but still reading)");
      }
      Serial.print("Gas Sensor ");
      Serial.print(sensors[i].name);
      Serial.print(": ");
      Serial.println(gasValue);
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
  sendSensorData();
  delay(2000);
}