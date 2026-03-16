 #include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <secrets.h>
#include <telemetry/mqtt.h>
#include <variables/setget.h>
#include <sensors/compass.h>
#include <sensors/usensor.h>

// Ultrasound number and pins
#define NUM_SENSORS 4
#define TRIGGER_PIN 16
#define ECHO_PIN 34
#define TRIGGER_PIN2 17
#define ECHO_PIN2 35
#define TRIGGER_PIN3 26
#define ECHO_PIN3 25
#define TRIGGER_PIN4 19
#define ECHO_PIN4 18

String chipid;
Mqtt mqtt;
Compass compass;
Usensor ultraSound;
long int age;

void setup()
{
	Serial.begin(57600);
	uint64_t chipIdHex = ESP.getEfuseMac();
	chipid = String((uint32_t)(chipIdHex >> 32), HEX) + String((uint32_t)chipIdHex, HEX);

	// WiFi.begin(ssid, password);
	// while (WiFi.status() != WL_CONNECTED)
	// {
	// 	delay(1000);
	// 	Serial.println("Connecting to WiFi...");
	// }
	// Serial.println("Connected to WiFi");

	globalVar_init();
	compass.Begin();
	vTaskDelay(pdMS_TO_TICKS(100));

	ultraSound.open(TRIGGER_PIN, ECHO_PIN, rawDistFront);
	ultraSound.open(TRIGGER_PIN2, ECHO_PIN2, rawDistRight);
	ultraSound.open(TRIGGER_PIN3, ECHO_PIN3, rawDistLeft);
	ultraSound.open(TRIGGER_PIN4, ECHO_PIN4, rawDistBack);

	//mqtt.init(chipid);
	//mqtt.send("stream", "streamer online");
}

void loop()
{
	//mqtt.loop();

	StaticJsonDocument<256> jsonDoc;

	jsonDoc["front"] = globalVar_get(rawDistFront, &age);
	jsonDoc["left"] = globalVar_get(rawDistLeft, &age);
	jsonDoc["right"] = globalVar_get(rawDistRight, &age);
	jsonDoc["back"] = globalVar_get(rawDistBack, &age);

	jsonDoc["magX"] = globalVar_get(rawMagX);
	jsonDoc["magY"] = globalVar_get(rawMagY);
	jsonDoc["magZ"] = globalVar_get(rawMagZ);
	//jsonDoc["heading"] = globalVar_get(calcHeading);

	String jsonString;
	serializeJson(jsonDoc, jsonString);
    Serial.println(jsonString);
	//mqtt.send("stream", jsonString);

	vTaskDelay(pdMS_TO_TICKS(250));
}
