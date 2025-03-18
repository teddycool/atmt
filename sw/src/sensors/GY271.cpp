#include <sensors/GY271.h>
#include <math.h>

#define PI 3.14159265358979323846  // Define PI manually


GY271::GY271(uint8_t address) : _address(address), _x(0), _y(0), _z(0) {
}

bool GY271::begin() {
    // Initialize the I2C communication
    Wire.begin();
  
    // Set the sensor to continuous measurement mode
    Wire.beginTransmission(_address);
    Wire.write(0x09);  // Register for mode configuration
    Wire.write(0x1D);  // Continuous measurement mode
    Wire.endTransmission();

        // Set to 2 Gauss sensitivity
        Wire.beginTransmission(_address);
        Wire.write(0x09);  // Control register
        Wire.write(0x01);  // Set to 2 Gauss sensitivity
        Wire.endTransmission();

    delay(100);  // Wait for the sensor to initialize

    return true;  // Always returns true for simplicity (can add checks if needed)
}

void GY271::readData(int16_t &x, int16_t &y, int16_t &z) {
    updateData();
    x = _x;
    y = _y;
    z = _z;
}

int16_t GY271::getX() {
    updateData();
    return _x;
}

int16_t GY271::getY() {
    updateData();
    return _y;
}

int16_t GY271::getZ() {
    updateData();
    return _z;
}

void GY271::updateData() {
    // Request the data starting from register 0x00 (the data output register)
    Wire.beginTransmission(_address);
    Wire.write(0x00);  // Register address for data output
    Wire.endTransmission();
    
    // Request 6 bytes of data (3 values: X, Y, Z)
    Wire.requestFrom(_address, 6);
    
    if (Wire.available() == 6) {
        _x = (Wire.read() << 8) | Wire.read();  // Combine high and low bytes for X
        _y = (Wire.read() << 8) | Wire.read();  // Combine high and low bytes for Y
        _z = (Wire.read() << 8) | Wire.read();  // Combine high and low bytes for Z
    } else {
        //Serial.println("Error reading sensor data!");
    }
}

#include <math.h>

// Assuming _x, _y, and _z hold the raw magnetometer data from the QMC5883L sensor

int GY271::getCompassDirection() {
    // Step 1: Get the raw magnetometer data
    updateData();  // Ensure the data is updated
    
    // Step 2: Earth's magnetic field at Stockholm, Sweden (in nT)
    float B_expected = 52075.3;  // Magnetic field strength in nT
    float inclination = 74.17;   // Magnetic inclination in degrees for Stockholm

    // Convert inclination angle to radians
    float inclination_radians = inclination * M_PI / 180.0;

    // Raw data from the magnetometer
    float xMag = (float)_x;
    float yMag = (float)_y;
    float zMag = (float)_z;
    
    // Step 3: Calculate the magnitude of the raw magnetic vector
    float magnitude = sqrt(xMag * xMag + yMag * yMag + zMag * zMag);
    
    // If magnitude is not zero, normalize the vector to the expected field strength
    if (magnitude > 0) {
        // Scale factor to adjust the vector's magnitude to the expected field strength
        float scaleFactor = B_expected / magnitude;
        
        // Normalize each component of the magnetic vector
        xMag *= scaleFactor;
        yMag *= scaleFactor;
        zMag *= scaleFactor;
    }

    // Step 4: Project the magnetic field back to the horizontal plane
    // Adjust the x and y components using the Earth's inclination
    float x_corrected = xMag * cos(inclination_radians) + zMag * sin(inclination_radians);
    float y_corrected = yMag * cos(inclination_radians) + zMag * sin(inclination_radians);
    
    // Step 5: Calculate the compass direction (course) in radians
    // Use atan2 to calculate the angle of the corrected vector
    float radians = atan2(y_corrected, x_corrected);  // atan2(y, x) returns the angle in radians
    
    // Step 6: Convert the radians to degrees
    float degrees = radians * 180.0 / M_PI;  // Convert radians to degrees
    
    // Step 7: Adjust the heading to make it clockwise and fit the 0-360 degree scale
    degrees = 90 - degrees;
    
    // Normalize to 0-360 degrees
    if (degrees < 0) {
        degrees += 360;
    }
    
    // Step 8: Return the course in tenths of degrees (as an integer)
    int direction = (int)(degrees * 10);  // Multiply by 10 for tenths of degrees
    
    return direction;
}
