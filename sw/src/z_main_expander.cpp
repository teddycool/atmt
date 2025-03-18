#include <Arduino.h>
#include <Wire.h>
#include <expander.h>

// Create an I2CDevices object with appropriate I2C addresses
EXPANDER i2cDevice(0x70, 0x20);  // Adjust I2C addresses as needed

void setup() {
  Serial.begin(57600);
  Wire.begin();  // Initialize Wire

  // Initialize the 9548 switch and GPIO expander
  i2cDevice.initSwitch();
  i2cDevice.initGPIO();

  // Test the 9548 channel switch
  Serial.println("Setting 9548 switch to channel 3");
  i2cDevice.setChannel(3);

  // Push channel 5 to the stack and switch to it
  Serial.println("Pushing channel 5");
  i2cDevice.pushChannel(5);

  // Pop the last channel (should revert to channel 3)
  Serial.println("Popping switch to the last channel");
  i2cDevice.popChannel();

  // Test the GPIO pins
  Serial.println("Setting GPIO pin 0 to HIGH");
  i2cDevice.writeGPIO(0, true);

  delay(1000);
  
  Serial.println("Reading GPIO pin 0 (Expecting HIGH)");
  Serial.println(i2cDevice.readGPIO(0) ? "HIGH" : "LOW");

  delay(1000);

  // Test toggling GPIO pin
  Serial.println("Toggling GPIO pin 0");
  i2cDevice.toggleGPIO(0);
  
  delay(1000);

  // Test LED on GPA7
  Serial.println("Turning on LED connected to GPA7");
  i2cDevice.setLED(true);
  delay(1000);
  i2cDevice.setLED(false);
}

void loop() {
  // Continuously toggle GPIO pin 1 for testing
  Serial.println("ON");
  i2cDevice.writeGPIO(7, true);  //LED
  vTaskDelay(pdMS_TO_TICKS(270));
  i2cDevice.writeGPIO(8,true);
  //vTaskDelay(pdMS_TO_TICKS(270));
  //i2cDevice.writeGPIO(9, true);
  //vTaskDelay(pdMS_TO_TICKS(270));
  i2cDevice.writeGPIO(0,true);   //full beam
  vTaskDelay(pdMS_TO_TICKS(200));
  Serial.println("OFF");
  i2cDevice.writeGPIO(7, false);
  //vTaskDelay(pdMS_TO_TICKS(270));
  //i2cDevice.writeGPIO(8,false);
  //vTaskDelay(pdMS_TO_TICKS(270));
  //i2cDevice.writeGPIO(9, false);
  //vTaskDelay(pdMS_TO_TICKS(270));
  i2cDevice.writeGPIO(0,false);
  vTaskDelay(pdMS_TO_TICKS(700));
  
}
