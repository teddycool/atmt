#include <Arduino.h>
#include <FastLED.h>
#include "light.h"
#define NUM_LEDS 4
#define CPIN 32

CRGB leds[NUM_LEDS];


void Light::SetUp(){
    pinMode(CPIN, OUTPUT);
    FastLED.addLeds<NEOPIXEL, CPIN>(leds, NUM_LEDS);
}

void Light::Set(int id, uint8_t color ){
    
}

void Light::Off(){
for(int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
}
    FastLED.show();
}

void Light::HeadLight(){
    leds[0] = CRGB::Black;
    leds[1] = CRGB::Black; 
    leds[2] = CRGB::White;
    leds[3] = CRGB::White; 
    FastLED.show();
}

void Light::BrakeLight(){
    leds[0] = CRGB::Red;
    leds[1] = CRGB::Red; 
    leds[2] = CRGB::Black;
    leds[3] = CRGB::Black; 
    FastLED.show();
}


void Light::Test(){
for(int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Red;
}
    FastLED.show();
}