#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include <Arduino.h>

// Enum for basic colors
enum class NeoPixelColor {
    RED,
    GREEN,
    BLUE,
    WHITE,
    OFF
};

class NeoPixel {
public:
    NeoPixel() = default;
    NeoPixel(uint8_t pin, uint16_t numPixels);
    void begin();
    void setPixelColor(uint16_t pixel, NeoPixelColor color);
    void show();

private:
    uint8_t pin;
    uint16_t numPixels;
    uint8_t *pixels;

    void sendBit(bool bitVal);
    void sendByte(uint8_t byte);
    void sendPixel(uint8_t r, uint8_t g, uint8_t b);
};

#endif // NEOPIXEL_H