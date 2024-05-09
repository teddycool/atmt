#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include "secrets.h"


AsyncWebServer server(80);

int loopcount=0;

void setup()
{

  Serial.begin(9600);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  // Connection-info in secrets.h
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", "Hi! I am ESP32. For OTA, use /Update"); });

  AsyncElegantOTA.begin(&server); // Start ElegantOTA
  server.begin();
  Serial.print("OTA server started");
  Serial.println("Setup is done!");
}

void loop()
{
  loopcount++;
  Serial.println();
  Serial.println("still looping... count: " + String(loopcount));
  Serial.println("---------------------------------------------");
  delay(1500);
}