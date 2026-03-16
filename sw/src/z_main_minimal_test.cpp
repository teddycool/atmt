#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=============================");
  Serial.println("MINIMAL ESP32 TEST - STARTED");
  Serial.println("=============================");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("Setup completed successfully");
}

void loop() {
  static int counter = 0;
  counter++;
  Serial.printf("Loop %d - Free heap: %d bytes\n", counter, ESP.getFreeHeap());
  delay(2000);
  
  if (counter >= 10) {
    Serial.println("=========================");
    Serial.println("TEST COMPLETED - SUCCESS!");
    Serial.println("=========================");
    counter = 0;
  }
}