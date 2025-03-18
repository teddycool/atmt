#include "expander.h"
#include <Wire.h>

#define SWITCH_I2C_ADDR 0x70  // Example address for the 9548 switch
#define GPIO_I2C_ADDR 0x20    // Example address for the 23017 GPIO expander

// Constructor
EXPANDER::EXPANDER(uint8_t switchAddr, uint8_t gpioAddr)
    : switchAddr(switchAddr), gpioAddr(gpioAddr), switchStackIndex(0) {
    // Initialize the semaphore with 1 (binary semaphore)
    i2cSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(i2cSemaphore); // Initially available
}

// Private method to handle I2C initialization
void EXPANDER::beginI2C() {
    Wire.begin();
}

// Initialize the 9548 switch to default channel (0)
void EXPANDER::initSwitch() {
    beginI2C();
    setChannel(0);  // Set to channel 0 by default
}

// Set the channel for the 9548 switch (0-7)
void EXPANDER::setChannel(uint8_t channel) {
    if (channel > 7) return;  // Validate channel
    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);  // Take the semaphore for I2C transaction

    Wire.beginTransmission(switchAddr);
    Wire.write(1 << channel);  // Write the corresponding channel
    Wire.endTransmission();

    xSemaphoreGive(i2cSemaphore);  // Release semaphore after transaction
}

// Push the current channel onto the stack and switch to the new channel
void EXPANDER::pushChannel(uint8_t channel) {
    if (switchStackIndex < 8) {
        // Save the current channel (in case pop is called)
        switchStack[switchStackIndex++] = Wire.read();  // Read current channel before changing
        setChannel(channel);  // Set the new channel
    }
}

// Pop the last channel from the stack and switch back to it
void EXPANDER::popChannel() {
    if (switchStackIndex > 0) {
        uint8_t lastChannel = switchStack[--switchStackIndex];
        setChannel(lastChannel);  // Switch back to the last channel
    }
}

// Initialize GPIO Expander (23017)
void EXPANDER::initGPIO() {
    beginI2C();
    // Initialize pins to outputs by default (you can set them later)
        // Initialize Port A (GPA0 to GPA7) for MOSFET and LED control
        setGPIOPinMode(0, false);  // GPA0 as output for MOSFET 1 control
        setGPIOPinMode(1, false);  // GPA1 as output for MOSFET 2 control
        setGPIOPinMode(7, false); // GPA7 as output for LED control
    for (uint8_t i = 8; i < 15; i++) {   //GPIO
        setGPIOPinMode(i, true);  // Default to output
    }
}

// Set GPIO pin mode to input or output
void EXPANDER::setGPIOPinMode(uint8_t pin, bool input) {
    pushChannel(0);  // Switch to channel 0 to communicate with MCP23017
    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);
    Wire.beginTransmission(gpioAddr);
    Wire.write(0x00 + (pin / 8));  // IODIRA or IODIRB register
    uint8_t mask = 1 << (pin % 8);
    uint8_t currentMode;
    Wire.requestFrom(gpioAddr, (uint8_t)1);
    currentMode = Wire.read();
    if (input) {
        currentMode |= mask;  // Set bit for input
    } else {
        currentMode &= ~mask; // Clear bit for output
    }
    Wire.write(currentMode);
    Wire.endTransmission();
    xSemaphoreGive(i2cSemaphore);
    popChannel();  // Return to the previous channel
}

// Write high/low to a GPIO pin
void EXPANDER::writeGPIO(uint8_t pin, bool value) {
    pushChannel(0);  // Switch to channel 0 to communicate with MCP23017
    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);
    Wire.beginTransmission(gpioAddr);
    Wire.write(0x14 + (pin / 8));  // OLATA or OLATB register
    uint8_t mask = 1 << (pin % 8);
    Wire.write(value ? mask : 0x00);
    Wire.endTransmission();
    xSemaphoreGive(i2cSemaphore);
    popChannel();  // Return to the previous channel
}

// Read the state of a GPIO pin
bool EXPANDER::readGPIO(uint8_t pin) {
    pushChannel(0);  // Switch to channel 0 to communicate with MCP23017
    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);
    Wire.beginTransmission(gpioAddr);
    Wire.write(0x12 + (pin / 8));  // GPIOA or GPIOB register
    Wire.endTransmission();
    Wire.requestFrom(gpioAddr, (uint8_t)1);
    uint8_t pinState = Wire.read();
    xSemaphoreGive(i2cSemaphore);
    popChannel();  // Return to the previous channel
    return (pinState & (1 << (pin % 8))) != 0;
}

// Toggle the GPIO pin state (high/low)
void EXPANDER::toggleGPIO(uint8_t pin) {
    bool currentState = readGPIO(pin);
    writeGPIO(pin, !currentState);
}

// Set the LED state on GPA7
void EXPANDER::setLED(bool state) {
    writeGPIO(7, state);  // GPA7 controls the LED (set high to light up)
}
