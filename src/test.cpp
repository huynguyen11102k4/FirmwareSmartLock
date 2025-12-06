// #include <Arduino.h>
// #include <WiFi.h>
// #include <WiFiClientSecure.h>
// #include <PubSubClient.h>
// #include <ca_cert.h>

// // =============================
// // CHỈ THAY 3 DÒNG NÀY
// // =============================
// const char* WIFI_SSID     = "toikhonghai";
// const char* WIFI_PASS     = "11102004";

// const char* MQTT_HOST     = "d2684409b6644e89b97ca34b695085ae.s1.eu.hivemq.cloud"; 
// const int   MQTT_PORT     = 8883;
// const char* MQTT_USER     = "toikhonghai";
// const char* MQTT_PASS     = "Huy123456";
// // =============================

// WiFiClientSecure espClient;
// PubSubClient mqtt(espClient);

// void setup() {
//   Serial.begin(115200);
//   delay(1000);
//   Serial.println();
//   Serial.println("=== MQTT TLS TEST START ===");

//   // -------- WIFI ----------
//   WiFi.mode(WIFI_STA);
//   WiFi.begin(WIFI_SSID, WIFI_PASS);

//   Serial.print("Connecting WiFi");
//   while (WiFi.status() != WL_CONNECTED) {
//     Serial.print(".");
//     delay(500);
//   }
//   Serial.println("\nWiFi connected!");
//   Serial.println("IP: " + WiFi.localIP().toString());

//   // -------- TLS ----------
//   Serial.println("Setting CA certificate...");
//   espClient.setCACert(hivemq_root_ca);
//   mqtt.setServer(MQTT_HOST, MQTT_PORT);
// }

// void loop() {
//   if (!mqtt.connected()) {
//     Serial.println("\nConnecting to HiveMQ Cloud...");
    
//     bool ok = mqtt.connect("ESP32TestClient", MQTT_USER, MQTT_PASS);

//     if (ok) {
//       Serial.println("MQTT connected successfully!");
//       mqtt.publish("test/topic", "Hello from ESP32 TLS!");
//     } else {
//       Serial.print("MQTT connect failed, rc=");
//       Serial.println(mqtt.state());
//     }

//     delay(5000);
//     return;
//   }

//   mqtt.loop();
// }
