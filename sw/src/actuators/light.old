#include <Arduino.h>
#include <FastLED.h>
#include "actuators/light.h"
#include "atmio.h"

CRGB leds[LIGHT_NUM_LEDS];

void Light::SetUp()
{
    pinMode(LIGHT_PIN, OUTPUT);
    FastLED.addLeds<NEOPIXEL, LIGHT_PIN>(leds, LIGHT_NUM_LEDS);

    for (int i = 0; i < LIGHT_NUM_LEDS; i++)
    {
        leds[i] = CRGB::Black;
    }
    FastLED.show();
}

void Light::Set(int id, uint8_t color)
{
    leds[id] = color;
    FastLED.show();
}

void Light::Off()
{
    for (int i = 0; i < LIGHT_NUM_LEDS; i++)
    {
        leds[i] = CRGB::Black;
    }
    FastLED.show();
}

void Light::HeadLightOn()
{

    leds[2] = CRGB::White;
    leds[3] = CRGB::White;
    FastLED.show();
}

void Light::HeadLightOff()
{
    leds[2] = CRGB::Black;
    leds[3] = CRGB::Black;
    FastLED.show();
}
void Light::BrakeLightOn()
{
    leds[0] = CRGB::Red;
    leds[1] = CRGB::Red;
    FastLED.show();
}
void Light::BrakeLightOff()
{
    leds[0] = CRGB::Black;
    leds[1] = CRGB::Black;
    FastLED.show();
}

void Light::Test()
{
    for (int i = 0; i < LIGHT_NUM_LEDS; i++)
    {
        leds[i] = CRGB::Blue;
    }
    FastLED.show();
}