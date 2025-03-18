
#include <Arduino.h>
#include <actuators/light.h>


Light light;

void setup(){
  Serial.begin(57600);
  Serial.println("Light test setup");
  light.SetUp();
}


void loop(){
    Serial.println("Light test");
    light.Test();
    delay(1000);
    light.Off();
    delay(1000);
}