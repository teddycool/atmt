#include "actuators/neopixel.h"

NeoPixel::NeoPixel() {
    
}

void NeoPixel::begin(uint8_t pini, uint16_t numPixelsi) {
    pin = pini;
    numPixels = numPixelsi;
    pixels = new uint8_t[numPixels * 3]; // Each pixel has 3 bytes (R, G, B)
    memset(pixels, 0, numPixels * 3);   // Initialize all pixels to OFF
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
    Serial.println("Updating NeoPixel...");
    noInterrupts(); // Disable interrupts for precise timing
    for (uint16_t i = 0; i < numPixels; i++) {
        Serial.print("Pixel ");
        Serial.print(i);
        Serial.print(": R=");
        Serial.print(pixels[i * 3 + 1]);
        Serial.print(", G=");
        Serial.print(pixels[i * 3]);
        Serial.print(", B=");
        Serial.println(pixels[i * 3 + 2]);
        sendPixel(pixels[i * 3], pixels[i * 3 + 1], pixels[i * 3 + 2]);
    }
    interrupts(); // Re-enable interrupts
    vTaskDelay(pdMS_TO_TICKS(0.5));; // Latch signal
}

void NeoPixel::sendBit(bool bitVal) {
    if (bitVal) {
        // Send a "1" bit
        digitalWrite(pin, HIGH);
        delayMicroseconds(1); // Adjusted T1H: 1µs
        digitalWrite(pin, LOW);
        delayMicroseconds(0.5); // Adjusted T1L: 500ns
    } else {
        // Send a "0" bit
        digitalWrite(pin, HIGH);
        delayMicroseconds(0.5); // Adjusted T0H: 500ns
        digitalWrite(pin, LOW);
        delayMicroseconds(1); // Adjusted T0L: 1µs
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