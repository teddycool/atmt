#include <Arduino.h>
#include <variables/setget.h>
#include <secrets.h>
#include <telemetry/mqtt.h>
#include <sensors/usensor.h>
#include <atmio.h>
#include <WiFi.h>

// Create vectors for the ultrasouns sensor pins
int triggerPins[NUM_SENSORS] = {TRIGGER_PIN1, TRIGGER_PIN2, TRIGGER_PIN3, TRIGGER_PIN4};
int echoPins[NUM_SENSORS] = {ECHO_PIN1, ECHO_PIN2, ECHO_PIN3, ECHO_PIN4};

Usensor ultraSound;
Mqtt mqtt;
long int age;
String chipId;

void setup()
{
	// Initialize serial communication for debugging
	Serial.begin(57600);
	Serial.println("Starting...");
	Serial.println();
	uint64_t chipIduit = ESP.getEfuseMac();
	chipId = String((uint32_t)(chipIduit >> 32), HEX) + String((uint32_t)chipIduit, HEX);

	Serial.println();
	Serial.println("******************************************************");
	Serial.print("ESP32 Unique Chip ID (MAC): ");
	Serial.println(chipId); 
	Serial.println("******************************************************");
	Serial.println();

	// Connect to WiFi
	Serial.println("Connecting: ");
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(1000);
		Serial.print(" * ");
	}
	Serial.println();
	Serial.println("Connected to WiFi");

	globalVar_init();
	// Here we should add a debug print of all sensor values before we start running to make sure everything is working.
	//
	Serial.print("Number of VARs: ");
	Serial.println(NUM_VARS);
	ultraSound.open(TRIGGER_PIN1, ECHO_PIN1, rawDistFront);
	ultraSound.open(TRIGGER_PIN2, ECHO_PIN2, rawDistRight);
	ultraSound.open(TRIGGER_PIN3, ECHO_PIN3, rawDistLeft);
	ultraSound.open(TRIGGER_PIN4, ECHO_PIN4, rawDistBack);
	
	Serial.println("Below a continous measure of distances in cm and (age) in ms if the measure");
	Serial.println("The age depends on the POLL_INTERVAL set in the usensor packet");
	Serial.println();
	vTaskDelay(pdMS_TO_TICKS(500));
}

void loop()
{

	String message;

	// Collect sensor data
	message += "{\"front\":";
	message += globalVar_get(rawDistFront, &age);
	message += ",\"back\":";
	message += globalVar_get(rawDistBack, &age);
	message += "}";

	// Send data to MQTT server
	mqtt.send(chipId,mqtt_topic, message);
	vTaskDelay(pdMS_TO_TICKS(500));

	Serial.print("  Front: ");
	Serial.print(globalVar_get(rawDistFront, &age));
	Serial.print(" cm (");
	Serial.print(age);
	Serial.print(")");

	Serial.print("  Back: ");
	Serial.print(globalVar_get(rawDistBack, &age));
	Serial.print(" cm (");
	Serial.print(age);
	Serial.println(")");
	vTaskDelay(pdMS_TO_TICKS(500));
}