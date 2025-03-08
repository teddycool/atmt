#include <sensors/accsensor.h>
#include <Arduino.h>
#include <variables/setget.h>
#include <sensors/compass.h>


Compass compass;

void setup(){
  Serial.begin(57600);
  globalVar_init();
  compass.Begin();
  vTaskDelay(pdMS_TO_TICKS(100));
}


void loop(){
    Serial.print("MagX: ");
    Serial.print(globalVar_get(rawMagX));
    Serial.print("   MagY:");
    Serial.print(globalVar_get(rawMagY));
    Serial.print("   MagZ:");
    Serial.println(globalVar_get(rawMagZ));
    vTaskDelay(pdMS_TO_TICKS(270));

}