#include <Arduino.h>
#include <sensors/GY271.h>


GY271 compass;  // Create an instance of the GY271 class

void setup() {
  Serial.begin(57600);

  if (compass.begin()) {
    Serial.println("Sensor initialized successfully!");
  } else {
    Serial.println("Sensor initialization failed!");
  }
}

void loop() {
  int16_t x, y, z;
  compass.readData(x, y, z);

  Serial.print("X: "); Serial.print(x);
  Serial.print(" Y: "); Serial.print(y);
  Serial.print(" Z: "); Serial.print(z);
  Serial.print("   Compass direction: ");
  Serial.println(compass.getCompassDirection()/10);

  delay(1000);  // Delay for 1 second
}
