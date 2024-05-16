#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <HTTPClient.h>
#include <ArduinoUniqueID.h>

#include "motor.h"
#include "usensor.h"
#include "light.h"
#include "dynamics.h"
#include "secrets.h"

// Define pin-name and GPIO#. Pin # are fixed but can ofcourse be named differently
// Motor test
#define M1E_PIN 2
#define M1F_PIN 4
#define M1R_PIN 5

// Steering
#define SENABLE 33
#define SRIGHT 27
#define SLEFT 23

// US sensors
#define TFRONT 16 // ECHO1
#define EFRONT 34
#define TLEFT 26 // ECHO3
#define ELEFT 25
#define TREAR 19 // ECHO4
#define EREAR 18
#define TRIGHT 17 // ECHO2
#define ERIGHT 35

AsyncWebServer server(80);

String cpuid; // The unique hw id, actually arduino cpu-id

// Setup the needed components
Dynamics dynamics;
Usensor leftdistance(TLEFT, ELEFT);
Usensor rightdistance(TRIGHT, ERIGHT);
Usensor frontdistance(TFRONT, EFRONT);
Usensor reardistance(TREAR, EREAR);
Motor motor(M1E_PIN, M1F_PIN, M1R_PIN);
Motor steering(SENABLE, SLEFT, SRIGHT);
Light light;

int loopcount = 0;
// Function for reading uniuque chipid, to keep track of logs
String uids()
{
  String uidds;
  for (size_t i = 0; i < UniqueIDsize; i++)
  {
    if (UniqueID[i] < 0x10)
    {
      uidds = uidds + "0";
    }
    uidds = uidds + String(UniqueID[i], HEX);
  }
  return uidds;
}

String postlog(String msg)
{
  WiFiClient wificlient; // Used for sending http to url
  HTTPClient http;

  msg.replace(" ", "%20");

  String url = "http://192.168.2.2/post_log.php?chipid=" + cpuid + "&msg=" + msg;
  http.begin(wificlient, url);
  int httpResponseCode = http.GET();
  String payload = "{}";

  if (httpResponseCode > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();
  return payload;
}

void setup()
{

  Serial.begin(9600);
  cpuid = uids(); // Read the unique boxid
  Serial.print("Hello from ");
  Serial.println(cpuid);
  WiFi.mode(WIFI_STA);
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
            { request->send(200, "text/plain", "Hi! I am ESP32 in car running on " + cpuid); });

  AsyncElegantOTA.begin(&server); // Start ElegantOTA
  server.begin();
  postlog("OTA server started");
  postlog("ATMT started!");
  frontdistance.SetUp();
  reardistance.SetUp();
  rightdistance.SetUp();
  leftdistance.SetUp();
  motor.SetUp();
  steering.SetUp();
  light.SetUp();
  dynamics.SetUp();

  Serial.println("Setup is done!");
}

void loop()
{
  loopcount++;
  Serial.println();
  Serial.println("still looping... count: " + String(loopcount));
  steering.Start();
  dynamics.Update();
  motor.Start();
  steering.Start();
  Serial.println("Testing light");
  light.Test();
  Serial.println("Reading the distance sensors");
  Serial.println("FRONT : REAR : RIGHT : LEFT");
  float frontdist = frontdistance.GetDistance();
  float reardist = reardistance.GetDistance();
  float rightdist = rightdistance.GetDistance();
  float leftdist = leftdistance.GetDistance();

  postlog("Front distance: " + String(frontdist) + " cm");
  postlog("Rear distance: " + String(reardist) + " cm");
  postlog("Right distance: " + String(rightdist) + " cm");
  postlog("Left distance: " + String(leftdist) + " cm");

  postlog("Accellerometer X: " + String(dynamics.GetAccX()));
  postlog("Accellerometer Y: " + String(dynamics.GetAccY()));
  postlog("Accellerometer Z: " + String(dynamics.GetAccZ()));

  postlog("Gyro X: " + String(dynamics.GetGyroX()));
  postlog("Gyro Y: " + String(dynamics.GetGyroY()));
  postlog("Gyro Z: " + String(dynamics.GetGyroZ()));

  postlog("Compass X: " + String(dynamics.GetCompX()));
  postlog("Compass Y: " + String(dynamics.GetCompY()));
  postlog("Compass Z: " + String(dynamics.GetCompZ()));

  delay(1000);
  steering.Reverse();
  motor.Reverse();
  light.Off();
  Serial.println("---------------------------------------------");
  delay(1500);
}