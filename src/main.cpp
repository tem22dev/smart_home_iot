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
};

SensorConfig sensors[10];
int sensorCount = 0;

DHT* dhtInstance = nullptr;
bool deviceStatus = false;
bool isStatusReceived = false; // Biến kiểm tra xem đã nhận trạng thái chưa

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
    delete dhtInstance;
    dhtInstance = nullptr;

    JsonArray sensorArray = doc["sensors"];
    for (JsonObject sensor : sensorArray) {
      sensors[sensorCount].id = sensor["id"].as<String>();
      sensors[sensorCount].name = sensor["name"].as<String>();
      sensors[sensorCount].pin = sensor["pin"].as<int>();
      sensors[sensorCount].type = sensor["type"].as<String>();
      sensors[sensorCount].unit = sensor["unit"].as<String>();
      sensors[sensorCount].threshold = sensor["threshold"].as<int>();
      sensors[sensorCount].status = sensor["status"].as<bool>();

      if (sensors[sensorCount].type == "DHT") {
        dhtInstance = new DHT(sensors[sensorCount].pin, DHT22);
        dhtInstance->begin();
        Serial.print(sensors[sensorCount].name);
        Serial.println(" configured as DHT22 with dynamic pin");
      } else if (sensors[sensorCount].type == "MQ2") {
        pinMode(sensors[sensorCount].pin, INPUT);
        Serial.print(sensors[sensorCount].name);
        Serial.println(" configured as MQ2");
      }
      sensorCount++;
    }
    Serial.println("Configuration applied");
  } else if (strcmp(topic, ("status/" + String(deviceCode)).c_str()) == 0) {
    DynamicJsonDocument doc(128);
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }
    bool newStatus = doc["status"].as<bool>();
    if (deviceStatus != newStatus) {
      deviceStatus = newStatus;
      isStatusReceived = true; // Đánh dấu đã nhận trạng thái
      Serial.print("Device status updated to: ");
      Serial.println(deviceStatus);
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
      client.publish("request/config", deviceCode);
      // Yêu cầu trạng thái Device ngay khi kết nối
      client.publish(("status/" + String(deviceCode)).c_str(), ""); // Yêu cầu trạng thái từ server
      isStatusReceived = false; // Đặt lại trạng thái khi kết nối lại
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

  for (int i = 0; i < sensorCount; i++) {
    if (sensors[i].type == "DHT" && dhtInstance) {
      float temperature = dhtInstance->readTemperature();
      float humidity = dhtInstance->readHumidity();
      if (isnan(temperature) || isnan(humidity)) {
        Serial.println("Failed to read from DHT22");
        return;
      }

      // Gửi nhiệt độ
      if (temperature >= 0) {
        DynamicJsonDocument doc(128);
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

      // Gửi độ ẩm
      if (humidity >= 0) {
        DynamicJsonDocument doc(128);
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
    } else if (sensors[i].type == "MQ2") {
      int gasValue = analogRead(sensors[i].pin);
      if (gasValue >= sensors[i].threshold) {
        DynamicJsonDocument doc(128);
        doc["sensorId"] = sensors[i].id;
        doc["value"] = gasValue;
        doc["unit"] = ""; // Không có unit cho MQ2
        char buffer[128];
        serializeJson(doc, buffer);
        client.publish("sensor/data", buffer);
        Serial.print("Gas Sensor: ");
        Serial.println(gasValue);
      }
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