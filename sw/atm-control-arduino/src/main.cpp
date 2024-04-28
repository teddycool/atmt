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
// Motor
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
WiFiClient wificlient; // Used for sending http to url

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

String postLogMsg(String message)
{
  String ppost = "GET " + String(receivescripturl) + "?chipid=" + cpuid + "&msg=" + message +
                 " HTTP/1.1\r\n" + "Host: " + String(postserver) + "\r\n" + "Connection: close\r\n\r\n";
  Serial.println("Created measurement http request:");
  Serial.println(ppost);
  return ppost;
}
void setup()
{

  Serial.begin(9600);
  cpuid = uids(); // Read the unique boxid
  Serial.print("Hello from ");
  Serial.println(cpuid);
  Serial.println("Running setup()");
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
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", "Hi! I am ESP32 in car running on " + cpuid); });

  AsyncElegantOTA.begin(&server); // Start ElegantOTA
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("----------");
  Serial.println("ATMT started!");
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
  Serial.println(String(frontdistance.GetDistance()) + " : " + String(reardistance.GetDistance()) + " : " +
                 String(rightdistance.GetDistance()) + " : " + String(leftdistance.GetDistance()));

  Serial.println("Sending data to url..");
  //wificlient.connect(postserver, 80);
  //wificlient.print("Front-distance: " + String(frontdistance.GetDistance()));

  Serial.println("Reading the accelerometer values");
  Serial.println("ACCX  :   ACCY   :   ACCZ");
  Serial.println(String(dynamics.GetAccX()) + " : " + String(dynamics.GetAccY()) + " : " + String(dynamics.GetAccZ()));
  Serial.println("Reading the gyro values");
  Serial.println("GyroX  :   GyroY   :   GyroZ");
  Serial.println(String(dynamics.GetGyroX()) + " : " + String(dynamics.GetGyroY()) + " : " + String(dynamics.GetGyroZ()));
  delay(1000);
  steering.Reverse();
  motor.Reverse();
  light.Off();
  Serial.println("---------------------------------------------");
  delay(1500);
}