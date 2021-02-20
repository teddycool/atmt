#include <Arduino.h>
#include <FastLED.h>
#include "light.h"

Light::Light(uint8_t cnt) : CPIN(cnt){    
}

void Light::SetUp(int cnt){
    pinMode(CPIN, OUTPUT);
}

void Light::Set(int id, uint8_t color ){
    
}