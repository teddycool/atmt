#include <Arduino.h>
#include <WiFi.h>
#include <secrets.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("===============================");
  Serial.println("ESP32 WIFI TEST - STARTED");
  Serial.println("===============================");
  Serial.printf("Free heap before WiFi: %d bytes\n", ESP.getFreeHeap());
  
  // Get chip ID
  uint64_t chipIdHex = ESP.getEfuseMac();
  String chipId = String((uint32_t)(chipIdHex >> 32), HEX) + String((uint32_t)chipIdHex, HEX);
  Serial.println("Chip ID: " + chipId);
  
  // Set WiFi hostname  
  WiFi.setHostname(chipId.c_str());
  Serial.println("WiFi hostname set to: " + chipId);
  
  // Connect to WiFi with timeout
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.printf("Free heap after WiFi: %d bytes\n", ESP.getFreeHeap());
  } else {
    Serial.println("\nWiFi connection failed!");
  }
  
  Serial.println("Setup completed");
}

void loop() {
  static int counter = 0;
  counter++;
  
  Serial.printf("Loop %d - WiFi: %s - Free heap: %d bytes\n", 
    counter, 
    WiFi.status() == WL_CONNECTED ? "connected" : "disconnected",
    ESP.getFreeHeap());
    
  delay(3000);
  
  if (counter >= 5) {
    Serial.println("============================");
    Serial.println("WIFI TEST COMPLETED SUCCESS!");
    Serial.println("============================");
    counter = 0;
  }
}