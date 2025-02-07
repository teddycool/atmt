#include <Arduino.h>
#include <WiFiType.h>
#include <WiFi.h>
#include "secrets.h">

String cpuid; // The unique hw id, actually arduino cpu-id


void setup() {
   Serial.begin(9600);

  Serial.print("Hello from ESP32");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
  sleep(500);
    Serial.print(".");
  }
  Serial.println("Connected to the WiFi network");
  Serial.println("Starting main-loop");


}

void loop() {
  // put your main code here, to run repeatedly:

}

