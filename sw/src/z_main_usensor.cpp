#include <Arduino.h>
#include <setget.h>
#include <usensor.h>

#define PWM

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

//  Laser Lidar ToF __________________________
/*#define VL53L0X_REG_IDENTIFICATION_MODEL_ID 0xc0
#define VL53L0X_REG_IDENTIFICATION_REVISION_ID 0xc2
#define VL53L0X_REG_PRE_RANGE_CONFIG_VCSEL_PERIOD 0x50
#define VL53L0X_REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD 0x70
#define VL53L0X_REG_SYSRANGE_START 0x00

#define VL53L0X_ADDRESS 0x29 */

//------------------------------------



//Adafruit_VL53L0X lox = Adafruit_VL53L0X();

// Create vecors for the ultrasouns sensor pins
int triggerPins[NUM_SENSORS] = {TRIGGER_PIN, TRIGGER_PIN2, TRIGGER_PIN3, TRIGGER_PIN4};
int echoPins[NUM_SENSORS] = {ECHO_PIN, ECHO_PIN2, ECHO_PIN3, ECHO_PIN4};


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
  ultraSound.open(TRIGGER_PIN,ECHO_PIN,rawDistFront);
  ultraSound.open(TRIGGER_PIN2,ECHO_PIN2,rawDistLeft);
  ultraSound.open(TRIGGER_PIN3,ECHO_PIN3,rawDistRight);
  Serial.print(ultraSound.open(TRIGGER_PIN4,ECHO_PIN4,rawDistBack));
  vTaskDelay(pdMS_TO_TICKS(100));
  //----------------------------------------------------------------------------------------------------------------
}

void loop()
{
 
  Serial.print("    Front: ");
  Serial.print(globalVar_get(rawDistFront));
  Serial.print(" cm");
  Serial.print("Left: ");
  Serial.print(globalVar_get(rawDistLeft));
  Serial.print(" cm");
  Serial.print("      Right: ");
  Serial.print(globalVar_get(rawDistRight));
  Serial.print(" cm");
  Serial.print("      Back: ");
  Serial.print(globalVar_get(rawDistBack));
  Serial.println(" cm");
   vTaskDelay(pdMS_TO_TICKS(1000));
  
};
