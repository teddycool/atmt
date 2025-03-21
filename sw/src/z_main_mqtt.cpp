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
Steer steer;
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
	 // Parse the message as JSON
	 StaticJsonDocument<200> jsonDoc;
	 DeserializationError error = deserializeJson(jsonDoc, message);
 
	 if (error)
	 {
		 Serial.print("Failed to parse JSON: ");
		 Serial.println(error.c_str());
		 return;
	 }

	 // Access JSON values
	 if (jsonDoc.containsKey("motor"))
	 {
		 int motorSpeed = jsonDoc["motor"];
		 Serial.print("Setting motor speed to: ");
		 Serial.println(motorSpeed);
		 motor.driving(motorSpeed); // Assuming `motor.driving(int)` is a valid method
	 }

	 if (jsonDoc.containsKey("direction"))
	 {
		 int direction = jsonDoc["direction"];
		 Serial.print("Setting steer direction to: ");
		 Serial.println(steerDirection);
 
		 steer.direction(direction);
	 }
	
}


void setup()
{
	// Initialize serial communication for debugging
	Serial.begin(57600);
	uint64_t chipIdHex = ESP.getEfuseMac();
	chipid = String((uint32_t)(chipIdHex >> 32), HEX) + String((uint32_t)chipIdHex, HEX);

	// Connect to WiFi
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(1000);
		Serial.println("Connecting to WiFi...");
	}
	Serial.println("Connected to WiFi");
	// Here we should add a debug print of all sensor values before we start running to make sure everything is working.

	
	globalVar_init();
    steer.Begin();
	vTaskDelay(pdMS_TO_TICKS(500));

	Serial.println("Steer initiated)");
	vTaskDelay(pdMS_TO_TICKS(500));

	Serial.println();
	Serial.println("******************************************************");
	Serial.print("ESP32 Unique Chip ID (MAC): ");
	Serial.println(chipid);
	Serial.println("******************************************************");

	ultraSound.open(TRIGGER_PIN, ECHO_PIN, rawDistFront);
	ultraSound.open(TRIGGER_PIN2, ECHO_PIN2, rawDistRight);
	ultraSound.open(TRIGGER_PIN3, ECHO_PIN3, rawDistLeft);
	ultraSound.open(TRIGGER_PIN4, ECHO_PIN4, rawDistBack);

	

	mqtt.init(chipid);

	// Set the callback function
	mqtt.setCallback(mqttMessageCallback);

	mqtt.send("test", "Hello World");

	// Subscribe to a topic
	mqtt.subscribe("control");

	delay(1000);
	//
}

void loop()
{

	mqtt.loop();



	    // Create a JSON object
    StaticJsonDocument<200> jsonDoc;

    // Add sensor values to the JSON object
    jsonDoc["front"] = globalVar_get(rawDistFront, &age);
    jsonDoc["left"] = globalVar_get(rawDistLeft, &age);
    jsonDoc["right"] = globalVar_get(rawDistRight, &age);
    jsonDoc["back"] = globalVar_get(rawDistBack, &age);

    // Serialize the JSON object to a string
    String jsonString;
    serializeJson(jsonDoc, jsonString);

    // Send the JSON string via MQTT
    mqtt.send("distance", jsonString);
	// Delay for 500ms
   // vTaskDelay(pdMS_TO_TICKS(500));

	if(globalVar_get(rawDistFront, &age)< 20){
		motor.driving(0);
	}

	if(globalVar_get(rawDistBack, &age)< 20){
		motor.driving(0);
	}


}
