#include <sensors/accsensor.h>
#include <Arduino.h>
#include <variables/setget.h>


ACCsensor asens;

void setup(){
  Serial.begin(57600);
  globalVar_init();
  asens.Begin();
  vTaskDelay(pdMS_TO_TICKS(100));
}


void loop(){
    Serial.print("zeroAx: ");
    Serial.print(globalVar_get(zeroAx));
    Serial.print("     ms per tick: ");
    Serial.println(portTICK_PERIOD_MS);
    Serial.print("AX:");
    Serial.print(globalVar_get(rawAccX));
    Serial.print("   AY:");
    Serial.print(globalVar_get(rawAccY));
    Serial.print("   AZ:");
    Serial.println(globalVar_get(rawAccZ));
    Serial.print("Temp:");
    Serial.println(globalVar_get(rawTemp));
    Serial.print("GX:");
    Serial.print(globalVar_get(rawGyX) / 131); // degrees per second turn speed
    Serial.print("   GY:");
    Serial.print(globalVar_get(rawGyY) / 131);
    Serial.print("   GZ:");
    Serial.print(globalVar_get(rawGyZ) / 131); // this is the turning axle.
    Serial.print("   GZraw:");
    Serial.println(globalVar_get(rawGyZ)); // this is the turning axle.
    Serial.println();
    Serial.print("Heading: ");
    Serial.print(globalVar_get(calcHeading));
    Serial.print("   Distance: ");
    Serial.print(globalVar_get(calcDistance));
    Serial.print("   Distance (cm): ");
    Serial.print(globalVar_get(calcDistance)/1000);
    Serial.print("   Speed: ");
    Serial.print(globalVar_get(calcSpeed));
    Serial.print("   Speed (cm/s): ");
    Serial.print(globalVar_get(calcSpeed)/(8*2048));
    Serial.print("   Represents (km/h): ");
    Serial.print(globalVar_get(calcSpeed)*20*3.6/(8*2048)/100);
    Serial.println();
    Serial.println();
    vTaskDelay(pdMS_TO_TICKS(270));

}