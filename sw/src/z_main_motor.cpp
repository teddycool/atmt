#include <Arduino.h>
#include <variables/setget.h>
#include <actuators/motor.h>


//#define PWM

// Ultrasound number and pins
// Motor


//  Laser Lidar ToF __________________________
/*#define VL53L0X_REG_IDENTIFICATION_MODEL_ID 0xc0
#define VL53L0X_REG_IDENTIFICATION_REVISION_ID 0xc2
#define VL53L0X_REG_PRE_RANGE_CONFIG_VCSEL_PERIOD 0x50
#define VL53L0X_REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD 0x70
#define VL53L0X_REG_SYSRANGE_START 0x00

#define VL53L0X_ADDRESS 0x29 */

Motor drive ;

void setup()
{
  // Initialize serial communication for debugging
  Serial.begin(57600);

  


  //globalVar_init();


  vTaskDelay(pdMS_TO_TICKS(100));
  //----------------------------------------------------------------------------------------------------------------
}

void loop()
{
   vTaskDelay(pdMS_TO_TICKS(1000));
   drive.driving(100);
   vTaskDelay(pdMS_TO_TICKS(1000));
   drive.driving(-100);
};
