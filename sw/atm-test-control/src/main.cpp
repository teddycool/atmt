#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <HTTPClient.h>
#include <ArduinoUniqueID.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <vector>

#include "motor.h"
#include "drive.h"
#include "steer.h"
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

String cpuid; // The unique hw id, actually arduino cpu-id

// Setup the needed components
Dynamics dynamics;
Usensor leftdistance(TLEFT, ELEFT);
Usensor rightdistance(TRIGHT, ERIGHT);
Usensor frontdistance(TFRONT, EFRONT);
Usensor reardistance(TREAR, EREAR);
Motor motor(M1E_PIN, M1F_PIN, M1R_PIN);
Drive drive(motor);
Motor steering(SENABLE, SLEFT, SRIGHT);
Steer steer(steering);
Light light;
std::vector<float> frontDist(3, 100);
std::vector<float> rearDist(3, 100);
std::vector<float> leftDist(3, 100);
std::vector<float> rightDist(3, 100);

int loopcount = 0;
int idx = 0;

// for logging
char payload[300];
DynamicJsonDocument doc(2048);
String topic;

// for mqtt listening
WiFiClient espClient;
PubSubClient client(espClient);

void Delay(int ms){
  vTaskDelay(pdMS_TO_TICKS(ms));
}

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

void mqttlog(String msg, String logType = "logging")
{

  WiFiClient wificlient;
  PubSubClient mqttClient(wificlient);
  mqttClient.setServer("192.168.2.2", 1883);
  mqttClient.connect(cpuid.c_str());

  //String topic = cpuid +"/" + logType;
  topic = cpuid +"/" + logType;
  //Serial.println("mqtt cpuid: "+ cpuid);
  //Serial.println("mqtt topic: "+ topic);
  //Serial.println("mqtt msg: " + msg);
  mqttClient.publish(topic.c_str(),msg.c_str());
}

void mqttmeasurements(){
  //String topic = "measurements";
  topic = "measurements";
  // Create the JSON document
  //DynamicJsonDocument doc(1024);
  doc["ID"] = cpuid;

  doc["T"] = int(millis());

  doc["it"] = loopcount;

  doc["DisFront"] = frontdistance.GetDistance();
  doc["DisRear"] = reardistance.GetDistance();
  doc["DisRight"] = rightdistance.GetDistance();
  doc["DisLeft"] = leftdistance.GetDistance();

  doc["AccX"] = dynamics.GetAccX();
  doc["AccY"] = dynamics.GetAccY();
  doc["AccZ"] = dynamics.GetAccZ();

  doc["GyX"] = dynamics.GetGyroX();
  doc["GyY"] = dynamics.GetGyroY();
  doc["GyZ"] = dynamics.GetGyroZ();

  doc["ComX"] = dynamics.GetCompX();
  doc["ComY"] = dynamics.GetCompY();
  doc["ComZ"] = dynamics.GetCompZ();
  /*
     JsonArray data = doc.createNestedArray("data");
     data.add(48.756080);
     data.add(2.302038);
     */

  // Serialize JSON to string
  //char payload[300];
  serializeJson(doc, payload, sizeof(payload));

  //Serial.println("payload: "+String(payload));

  mqttlog(payload,topic);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String msg = "";
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    msg+=(char)payload[i];
  }
  Serial.println();
  mqttlog(msg,"echo");
}

void reconnect() {
  // Loop until reconnected
  while (!client.connected()) {
    Serial.print(client.state());
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(cpuid.c_str())) {
      Serial.println("connected");
      // Subscribe to a topic
      topic = cpuid + "/" + "remote" ;
      Serial.println("Listening to topic: "+topic); 
      client.subscribe(topic.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      Delay(5000);
    }
  }
}

void mqttListen(){
  WiFiClient wificlient;
  PubSubClient mqttClient(wificlient);
  mqttClient.setServer("192.168.2.2", 1883);
  mqttClient.connect(cpuid.c_str());
  String topic = cpuid + "/" + "remote";
  mqttClient.subscribe(topic.c_str());
  mqttClient.setCallback(callback);

}

String postlog(String msg)
{
  mqttlog(msg);
  Serial.println(msg);
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

// void steerRight(){
//   steering.Reverse();

// }
// void steerLeft(){

//   steering.Start();
// }

void setup()
{

  Serial.begin(9600);
  cpuid = uids(); // Read the unique boxid
  Serial.print("Hello from ");
  Serial.println(cpuid);

  
  IPAddress gateway(192, 168, 2, 1);
  IPAddress subnet(255, 255, 0, 0);
  IPAddress primaryDNS(8, 8, 8, 8);
  IPAddress secondaryDNS(8, 8, 4, 4); // optional


  WiFi.mode(WIFI_STA);
 if (cpuid.startsWith("64b7084cff5c"))
  {
    IPAddress local_IP(192, 168, 2, 103);
     WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);

  }
  else{

     IPAddress local_IP(192, 168, 2, 104);
     WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);

  }
 

  WiFi.begin(ssid, password);

  Serial.println("");

  // Wait for connection
  // Connection-info in secrets.h
  while (WiFi.status() != WL_CONNECTED)
  {
    Delay(500);
    Serial.print(".");
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", "Hi! I am ESP32 in car running on " + cpuid); });

  AsyncElegantOTA.begin(&server); // Start ElegantOTA
  server.begin();
  postlog("OTA server started");
  postlog("ATMT started!");
  postlog("Mac: " + String(WiFi.macAddress()));
  frontdistance.SetUp();
  reardistance.SetUp();
  rightdistance.SetUp();
  leftdistance.SetUp();
  motor.SetUp();
  steering.SetUp();
  light.SetUp();
  dynamics.SetUp();

    // Set MQTT server details
  client.setServer("192.168.2.2", 1883);
  // Callback function to handle incoming messages
  client.setCallback(callback);

  Serial.println("Setup is done!");
}

void driveStrategy()
{
  drive.Forward(1);
  Delay(5000);
}

float GetCleanDist(std::vector<float> &vec){
  float res = 0.0;
  for (int i = 0; i < 3; i++)
  {
    res += vec.at(i);
  }
  return res / 3;
  
}

bool objectInRange(std::vector<float> &vec) {
  float dist = GetCleanDist(vec);

  return dist > 0 && dist < rng;

}

void updateDistSensors(){
  float fD = frontdistance.GetDistance();
  float reD = reardistance.GetDistance();
  float riD = rightdistance.GetDistance();
  float lD = leftdistance.GetDistance();
  frontDist[idx] = fD > 100.0 ? 100.0 : fD;
  rearDist[idx] = reD > 100.0 ? 100.0 : reD;
  rightDist[idx] = riD > 100.0 ? 100.0 : riD;
  leftDist[idx] = lD > 100.0 ? 100.0 : lD;
}



void masterStrat() {
  if(objectInRange(frontDist) && objectInRange(rearDist)){
    drive.Stop();
    light.Test();
  }
  else if (objectInRange(frontDist)){
    drive.Reverse(1);
    light.Off();

  } else { 
    drive.Forward(1);
    light.Off();
  }

  if (objectInRange(leftDist)) { 
    steer.Right();
  } else if (objectInRange(rightDist)) { 
    steer.Left();
  }else{
    steer.Stop();
  }
}

void loop()
{
  
  idx = loopcount % 3;
  updateDistSensors();
  if(idx == 0){
    masterStrat();
  }

 
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  mqttmeasurements();
  mqttlog("loop nr. "+String(loopcount)+" time elapsed since start: " + String(millis())+" ms.");

  Delay(10);
  loopcount++;
}
