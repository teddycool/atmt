#include <Arduino.h>
#include <actuators/neopixel.h>


NeoPixel neoPixel;

void setup(){
  neoPixel = NeoPixel(32, 1);
  Serial.begin(57600);
  Serial.println("Light test setup");
  neoPixel.begin();
    
}


void loop(){
    neoPixel.setPixelColor(0, NeoPixelColor::BLUE);
    Serial.println("Light test blue");
    neoPixel.show(); 
    vTaskDelay(pdMS_TO_TICKS(1000));
    neoPixel.setPixelColor(0, NeoPixelColor::RED);
    Serial.println("Light test red");
    neoPixel.show();
    vTaskDelay(pdMS_TO_TICKS(1000));
}