#include "actuators/neopixel.h"

NeoPixel::NeoPixel(uint8_t pin, uint16_t numPixels) : pin(pin), numPixels(numPixels) {
    pixels = new uint8_t[numPixels * 3]; // Each pixel has 3 bytes (R, G, B)
    memset(pixels, 0, numPixels * 3);   // Initialize all pixels to OFF
}

void NeoPixel::begin() {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void NeoPixel::setPixelColor(uint16_t pixel, NeoPixelColor color) {
    if (pixel >= numPixels) return; // Out of bounds check

    uint8_t r = 0, g = 0, b = 0;

    switch (color) {
        case NeoPixelColor::RED:
            r = 255;
            break;
        case NeoPixelColor::GREEN:
            g = 255;
            break;
        case NeoPixelColor::BLUE:
            b = 255;
            break;
        case NeoPixelColor::WHITE:
            r = g = b = 255;
            break;
        case NeoPixelColor::OFF:
        default:
            r = g = b = 0;
            break;
    }

    pixels[pixel * 3] = g;     // NeoPixels use GRB order
    pixels[pixel * 3 + 1] = r;
    pixels[pixel * 3 + 2] = b;
}

void NeoPixel::show() {
    noInterrupts(); // Disable interrupts for precise timing
    for (uint16_t i = 0; i < numPixels; i++) {
        sendPixel(pixels[i * 3], pixels[i * 3 + 1], pixels[i * 3 + 2]);
    }
    interrupts(); // Re-enable interrupts
    delayMicroseconds(50); // Latch signal
}

void NeoPixel::sendBit(bool bitVal) {
    if (bitVal) {
        digitalWrite(pin, HIGH);
        delayMicroseconds(0.8); // T1H: 800ns
        digitalWrite(pin, LOW);
        delayMicroseconds(0.45); // T1L: 450ns
    } else {
        digitalWrite(pin, HIGH);
        delayMicroseconds(0.4); // T0H: 400ns
        digitalWrite(pin, LOW);
        delayMicroseconds(0.85); // T0L: 850ns
    }
}

void NeoPixel::sendByte(uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        sendBit(byte & 0x80); // Send the most significant bit first
        byte <<= 1;
    }
}

void NeoPixel::sendPixel(uint8_t r, uint8_t g, uint8_t b) {
    sendByte(g); // NeoPixels use GRB order
    sendByte(r);
    sendByte(b);
}