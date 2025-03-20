#include <WiFi.h>
#include <Arduino.h>
#include <telemetry/mqtt.h>
#include <secrets.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <actuators/motor.h>
#include <actuators/steer.h>
#include <variables/setget.h>
#include <sensors/usensor.h>

String chipid;
//Steer steer;
Motor motor;
Mqtt mqtt;
Usensor ultraSound;

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

long int age;

// Create vecors for the ultrasouns sensor pins
int triggerPins[NUM_SENSORS] = {TRIGGER_PIN, TRIGGER_PIN2, TRIGGER_PIN3, TRIGGER_PIN4};
int echoPins[NUM_SENSORS] = {ECHO_PIN, ECHO_PIN2, ECHO_PIN3, ECHO_PIN4};

// Define the callback function
void mqttMessageCallback(char *topic, byte *payload, unsigned int length)
{
	Serial.print("Message arrived on topic: ");
	Serial.println(topic);

	// Convert payload to a string
	String message = "";
	for (unsigned int i = 0; i < length; i++)
	{
		message += (char)payload[i];
	}

	Serial.print("Message: ");
	Serial.println(message);

	// Convert the string to an integer
	int value = message.toInt();

	// Print the integer value
	Serial.print("Converted integer: ");
	Serial.println(value);

	// Example: Use the integer value
	if (String(topic) == chipid + "/motor")
	{
		Serial.print("Setting motor speed to: ");
		Serial.println(value);
		motor.driving(value); // Assuming `motor.setSpeed(int)` is a valid method
	}

	// // Example: Use the integer value
	// if (String(topic) == chipid + "/steer")
	// {
	// 	Serial.print("Setting motor speed to: ");
	// 	Serial.println(value);
	// 	if (message == "left")
	// 	{
	// 		steer.Left(); // Assuming `motor.setSpeed(int)` is a valid method
	// 	}
	// 	if (message == "right")
	// 	{
	// 		steer.Right();
	// 	}
	// }
}


void setup()
{
	// Initialize serial communication for debugging
	Serial.begin(57600);
	uint64_t chipIdHex = ESP.getEfuseMac();
	chipid = String((uint32_t)(chipIdHex >> 32), HEX) + String((uint32_t)chipIdHex, HEX);

	//steer.Begin();
	globalVar_init();
	Serial.println("Steer initiated)");
	vTaskDelay(pdMS_TO_TICKS(1000));

	Serial.println();
	Serial.println("******************************************************");
	Serial.print("ESP32 Unique Chip ID (MAC): ");
	Serial.println(chipid);
	Serial.println("******************************************************");

	ultraSound.open(TRIGGER_PIN, ECHO_PIN, rawDistFront);
	ultraSound.open(TRIGGER_PIN2, ECHO_PIN2, rawDistLeft);
	ultraSound.open(TRIGGER_PIN3, ECHO_PIN3, rawDistRight);
	ultraSound.open(TRIGGER_PIN4, ECHO_PIN4, rawDistBack);

	// Connect to WiFi
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(1000);
		Serial.println("Connecting to WiFi...");
	}
	Serial.println("Connected to WiFi");
	// Here we should add a debug print of all sensor values before we start running to make sure everything is working.

	mqtt.init(chipid);

	// Set the callback function
	mqtt.setCallback(mqttMessageCallback);

	mqtt.send("test", "Hello World");

	// Subscribe to a topic
	mqtt.subscribe("motor");
	mqtt.subscribe("steer");

	delay(1000);
	//
}

void loop()
{

	mqtt.loop();

	mqtt.send("front", String(globalVar_get(rawDistBack, &age)));
	mqtt.send("left", String(globalVar_get(rawDistLeft, &age)));
	mqtt.send("right", String(globalVar_get(rawDistRight, &age)));
	mqtt.send("back", String(globalVar_get(rawDistBack, &age)));
	vTaskDelay(pdMS_TO_TICKS(500));
}
