#include <sensors/accsensor.h>
#include <Arduino.h>
#include <variables/setget.h>
#include <actuators/motor.h>
#include <actuators/steer.h>
#include <sensors/usensor.h>
#include <atmio.h>

// Create vectors for the ultrasound sensor pins
int triggerPins[NUM_SENSORS] = {TRIGGER_PIN1, TRIGGER_PIN2, TRIGGER_PIN3, TRIGGER_PIN4};
int echoPins[NUM_SENSORS] = {ECHO_PIN1, ECHO_PIN2, ECHO_PIN3, ECHO_PIN4};

long int age;

Usensor ultraSound;
ACCsensor asens;

Motor drive;
Steer steer;

String chipId;

void setup()
{
    Serial.begin(57600);
   
    Serial.println("Starting setup");
    uint64_t chipId = ESP.getEfuseMac();
	Serial.println();
	Serial.println("******************************************************");
	Serial.print("ESP32 Unique Chip ID (MAC): ");
	Serial.println(chipId, HEX);  // Print the ESP32 EFUSE MAC address in hexadecimal format
    globalVar_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    asens.Begin();
    vTaskDelay(pdMS_TO_TICKS(100));
    steer.Begin();
    vTaskDelay(pdMS_TO_TICKS(100));
	Serial.println(ultraSound.open(TRIGGER_PIN1, ECHO_PIN1, rawDistFront));
	Serial.println(ultraSound.open(TRIGGER_PIN2, ECHO_PIN2, rawDistRight));
	Serial.println(ultraSound.open(TRIGGER_PIN3, ECHO_PIN3, rawDistLeft));
	Serial.println(ultraSound.open(TRIGGER_PIN4, ECHO_PIN4, rawDistBack));  
    vTaskDelay(pdMS_TO_TICKS(100));
}

void loop()
{
    // Serial.println("Compass values: ");
    // Serial.print("MagX: ");
    // Serial.print(globalVar_get(rawMagX));
    // Serial.print("   MagY:");
    // Serial.print(globalVar_get(rawMagY));
    // Serial.print("   MagZ:");
    // Serial.println(globalVar_get(rawMagZ));
    // vTaskDelay(pdMS_TO_TICKS(270));
    Serial.println("Accellerometer values: ");
    Serial.print("AX:");
    Serial.print(globalVar_get(rawAccX));
    Serial.print("   AY:");
    Serial.print(globalVar_get(rawAccY));
    Serial.print("   AZ:");
    Serial.println(globalVar_get(rawAccZ));
    vTaskDelay(pdMS_TO_TICKS(270));
    Serial.println("Ultrasonic sensor values: ");
    Serial.print("  Front: ");
    Serial.print(globalVar_get(rawDistFront, &age));
    Serial.print(" cm (");
    Serial.print("  Left: ");
    Serial.print(globalVar_get(rawDistLeft, &age));
    Serial.print(" cm (");
    Serial.print("  Right: ");
    Serial.print(globalVar_get(rawDistRight, &age));
    Serial.print(" cm (");
    Serial.print("  Back: ");
    Serial.print(globalVar_get(rawDistBack, &age));
    Serial.println(" cm (");
    vTaskDelay(pdMS_TO_TICKS(1000));
    drive.driving(100);
    vTaskDelay(pdMS_TO_TICKS(1000));
    drive.driving(-100);
    vTaskDelay(pdMS_TO_TICKS(200));
    steer.Right();
    vTaskDelay(pdMS_TO_TICKS(200));
    steer.Left();
    vTaskDelay(pdMS_TO_TICKS(200));
    steer.Stop();
    vTaskDelay(pdMS_TO_TICKS(200));

}