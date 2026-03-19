// GY-273 HMC5883L 3-Axis Magnetic Electronic Compass

#ifndef COMPASS_H
#define COMPASS_H

class Compass
{
public:
    Compass();
    void Begin();

private:
 
};

// Calibration functions
void forceCalibrationMode();  // Call this to manually start calibration
void updateBackgroundCalibration(float magX, float magY, float magZ);  // Auto-calibration during driving
bool loadCalibrationFromEEPROM();
void saveCalibrationToEEPROM();

#endif