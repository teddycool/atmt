#ifndef GY271_H
#define GY271_H

#include <Wire.h>

class GY271 {
public:
    GY271(uint8_t address = 0x0D);  // Constructor with default address
    bool begin();  // Initialize the sensor
    void readData(int16_t &x, int16_t &y, int16_t &z);  // Read magnetometer data
    int16_t getX();  // Get X-axis data
    int16_t getY();  // Get Y-axis data
    int16_t getZ();  // Get Z-axis data

    int getCompassDirection();  // Get the compass direction in tenths of degrees


private:
    uint8_t _address;  // I2C address
    int16_t _x, _y, _z;  // Sensor data

    void updateData();  // Internal function to read and store data
};

#endif
