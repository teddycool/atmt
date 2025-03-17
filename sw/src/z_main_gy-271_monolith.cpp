#include <Wire.h>
#include <Arduino.h>

#define SENSOR_ADDR 0x0D  // I2C address of the QMC5883L sensor

void readSensorData(int16_t* x, int16_t* y, int16_t* z) {
    // Request the data starting from register 0x00 (the data output register)
    Wire.beginTransmission(SENSOR_ADDR);
    Wire.write(0x00);  // Register address for data output
    Wire.endTransmission();
    
    // Request 6 bytes of data (3 values: X, Y, Z)
    Wire.requestFrom(SENSOR_ADDR, 6);
    
    if (Wire.available() == 6) {
      *x = (Wire.read() << 8) | Wire.read();  // Combine the high and low bytes for X
      *y = (Wire.read() << 8) | Wire.read();  // Combine the high and low bytes for Y
      *z = (Wire.read() << 8) | Wire.read();  // Combine the high and low bytes for Z
    } else {
      Serial.println("Error reading sensor data!");
    }
  }

void setup() {
  Serial.begin(57600);
  Wire.begin();  // Start I2C communication on the default ESP32 pins (SDA = GPIO21, SCL = GPIO22)
  
  // Initialize the QMC5883L sensor to continuous measurement mode
  Wire.beginTransmission(SENSOR_ADDR);
  Wire.write(0x09);  // Register for mode configuration
  Wire.write(0x1D);  // Continuous measurement mode
  Wire.endTransmission();
  
  delay(100);  // Wait for sensor to initialize
}

void loop() {
  int16_t x, y, z;
  readSensorData(&x, &y, &z);

  // Output the data to the Serial Monitor
  Serial.print("X: "); Serial.print(x);
  Serial.print(" Y: "); Serial.print(y);
  Serial.print(" Z: "); Serial.println(z);

  delay(1000);  // Delay for 1 second
}


