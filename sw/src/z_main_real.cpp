#include <Arduino.h>
#include <variables/setget.h>
#include <sensors/usensor.h>
#include <atmio.h>



// Create vectors for the ultrasouns sensor pins
int triggerPins[NUM_SENSORS] = {TRIGGER_PIN1, TRIGGER_PIN2, TRIGGER_PIN3, TRIGGER_PIN4};
int echoPins[NUM_SENSORS] = {ECHO_PIN1, ECHO_PIN2, ECHO_PIN3, ECHO_PIN4};


Usensor ultraSound;

void setup()
{
	// Initialize serial communication for debugging
	Serial.begin(57600);

	globalVar_init();
	// Here we should add a debug print of all sensor values before we start running to make sure everything is working.
	//
	Serial.print("Number of VARs: ");
	Serial.println(NUM_VARS);
	ultraSound.open(TRIGGER_PIN1,ECHO_PIN1,rawDistFront);
	ultraSound.open(TRIGGER_PIN2,ECHO_PIN2,rawDistRight);
	ultraSound.open(TRIGGER_PIN3,ECHO_PIN3,rawDistLeft);
	ultraSound.open(TRIGGER_PIN4,ECHO_PIN4,rawDistBack);
	uint64_t chipId = ESP.getEfuseMac();
	Serial.println();
	Serial.println("******************************************************");
	Serial.print("ESP32 Unique Chip ID (MAC): ");
	Serial.println(chipId, HEX);  // Print the MAC address in hexadecimal format
	Serial.println("******************************************************");
	Serial.println();
	Serial.println("Below a continous measure of distances in cm and (age) in ms if the measure");
	Serial.println("The age depends on the POLL_INTERVAL set in the usensor packet");
	Serial.println();
	vTaskDelay(pdMS_TO_TICKS(500));
	//----------------------------------------------------------------------------------------------------------------
}
long int age;

void loop()
{
	
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