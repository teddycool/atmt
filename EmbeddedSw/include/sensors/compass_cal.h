#ifndef COMPASS_CAL_H
#define COMPASS_CAL_H

// Magnetometer calibration structure
struct MagCalibration {
    // Hard iron offsets (bias)
    float offsetX = 0.0f;
    float offsetY = 0.0f; 
    float offsetZ = 0.0f;
    
    // Soft iron correction matrix (scale factors)
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float scaleZ = 1.0f;
    
    // Validation ranges
    float minX = -1000.0f, maxX = 1000.0f;
    float minY = -1000.0f, maxY = 1000.0f;
    float minZ = -1000.0f, maxZ = 1000.0f;
};

// Calibration functions
void applyCalibratedValues(float &magX, float &magY, float &magZ, const MagCalibration &cal);
void startCalibrationMode();
void updateCalibration(float magX, float magY, float magZ);
bool isCalibrationComplete();
MagCalibration getCalibrationData();

#endif