#ifndef EXPANDER_H
#define EXPANDER_H

#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class EXPANDER {
public:
    // Constructor
    EXPANDER(uint8_t switchAddr, uint8_t gpioAddr);

    // 9548 Switch Methods
    void initSwitch();
    void setChannel(uint8_t channel);
    void pushChannel(uint8_t channel);  // Updated to accept the channel
    void popChannel();  // Restores the previous channel

    // 23017 GPIO Methods
    void initGPIO();
    void setGPIOPinMode(uint8_t pin, bool input);
    void writeGPIO(uint8_t pin, bool value);
    bool readGPIO(uint8_t pin);
    void toggleGPIO(uint8_t pin);
    void setLED(bool state);

private:
    uint8_t switchAddr;  // 9548 I2C address
    uint8_t gpioAddr;    // 23017 I2C address
    uint8_t switchStack[8];  // Stack for push/pop channel switching
    uint8_t switchStackIndex;  // Current stack index for the switch state
    SemaphoreHandle_t i2cSemaphore;  // Semaphore for protecting I2C communication

    void beginI2C();
};

#endif
