#include <WiFi.h>
#include <PubSubClient.h>

// Thay thế bằng thông tin WiFi của bạn
const char* ssid = "Huong Duong";
const char* password = "20122000";

// Thông tin broker MQTT
const char* mqtt_server = "192.168.1.10"; // Ví dụ: 192.168.1.100
const int mqtt_port = 1883;
const char* mqtt_user = "";
const char* mqtt_password = "";

WiFiClient espClient;
PubSubClient client(espClient);

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
  Serial.print("Message received [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Subscribe đến topic (ví dụ)
      client.subscribe("sensor/data");
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

  // Kết nối WiFi
  connectWifi();

  // Cấu hình MQTT
  client.setServer(mqtt_server, mqtt_port);

  // Hàm callback (xử lý tin nhắn nhận được, sẽ định nghĩa sau)
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Duy trì kết nối MQTT
}