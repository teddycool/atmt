#include <sensors/compass_cal.h>
#include <Arduino.h>
#include <math.h>

// Global calibration data
static MagCalibration g_calibration;
static bool g_calibrationMode = false;
static int g_calibrationSamples = 0;
static const int CALIBRATION_SAMPLES_NEEDED = 100;

// Min/max tracking for calibration
static float g_calMinX = 1000.0f, g_calMaxX = -1000.0f;
static float g_calMinY = 1000.0f, g_calMaxY = -1000.0f;
static float g_calMinZ = 1000.0f, g_calMaxZ = -1000.0f;

void applyCalibratedValues(float &magX, float &magY, float &magZ, const MagCalibration &cal) {
    // Apply hard iron correction (offset removal)
    magX -= cal.offsetX;
    magY -= cal.offsetY;
    magZ -= cal.offsetZ;
    
    // Apply soft iron correction (scaling)
    magX *= cal.scaleX;
    magY *= cal.scaleY;
    magZ *= cal.scaleZ;
    
    // Validate ranges
    if (magX < cal.minX || magX > cal.maxX ||
        magY < cal.minY || magY > cal.maxY ||
        magZ < cal.minZ || magZ > cal.maxZ) {
        // Values out of expected range - might indicate interference
        Serial.printf("[MAG] Warning: Values out of range X=%.1f Y=%.1f Z=%.1f\n", magX, magY, magZ);
    }
}

void startCalibrationMode() {
    g_calibrationMode = true;
    g_calibrationSamples = 0;
    g_calMinX = 1000.0f; g_calMaxX = -1000.0f;
    g_calMinY = 1000.0f; g_calMaxY = -1000.0f;
    g_calMinZ = 1000.0f; g_calMaxZ = -1000.0f;
    
    Serial.println("[MAG] 🧭 Calibration mode started!");
    Serial.println("[MAG] Please rotate the device in all directions for 10 seconds...");
}

void updateCalibration(float magX, float magY, float magZ) {
    if (!g_calibrationMode) return;
    
    // Track min/max values
    if (magX < g_calMinX) g_calMinX = magX;
    if (magX > g_calMaxX) g_calMaxX = magX;
    if (magY < g_calMinY) g_calMinY = magY;
    if (magY > g_calMaxY) g_calMaxY = magY;
    if (magZ < g_calMinZ) g_calMinZ = magZ;
    if (magZ > g_calMaxZ) g_calMaxZ = magZ;
    
    g_calibrationSamples++;
    
    if (g_calibrationSamples >= CALIBRATION_SAMPLES_NEEDED) {
        // Calculate offsets (hard iron correction)
        g_calibration.offsetX = (g_calMaxX + g_calMinX) / 2.0f;
        g_calibration.offsetY = (g_calMaxY + g_calMinY) / 2.0f;
        g_calibration.offsetZ = (g_calMaxZ + g_calMinZ) / 2.0f;
        
        // Calculate scale factors (soft iron correction) 
        float rangeX = g_calMaxX - g_calMinX;
        float rangeY = g_calMaxY - g_calMinY;
        float rangeZ = g_calMaxZ - g_calMinZ;
        
        // Use average range as reference
        float avgRange = (rangeX + rangeY + rangeZ) / 3.0f;
        
        if (rangeX > 10.0f) g_calibration.scaleX = avgRange / rangeX;
        if (rangeY > 10.0f) g_calibration.scaleY = avgRange / rangeY;
        if (rangeZ > 10.0f) g_calibration.scaleZ = avgRange / rangeZ;
        
        // Set reasonable validation ranges
        g_calibration.minX = g_calMinX - 50.0f;
        g_calibration.maxX = g_calMaxX + 50.0f;
        g_calibration.minY = g_calMinY - 50.0f;
        g_calibration.maxY = g_calMaxY + 50.0f;
        g_calibration.minZ = g_calMinZ - 50.0f;
        g_calibration.maxZ = g_calMaxZ + 50.0f;
        
        g_calibrationMode = false;
        
        Serial.println("[MAG] ✅ Calibration complete!");
        Serial.printf("[MAG] Offsets: X=%.2f, Y=%.2f, Z=%.2f\n", 
                      g_calibration.offsetX, g_calibration.offsetY, g_calibration.offsetZ);
        Serial.printf("[MAG] Scales: X=%.3f, Y=%.3f, Z=%.3f\n",
                      g_calibration.scaleX, g_calibration.scaleY, g_calibration.scaleZ);
    } else if (g_calibrationSamples % 20 == 0) {
        Serial.printf("[MAG] Calibration: %d/%d samples\n", g_calibrationSamples, CALIBRATION_SAMPLES_NEEDED);
    }
}

bool isCalibrationComplete() {
    return !g_calibrationMode && g_calibrationSamples >= CALIBRATION_SAMPLES_NEEDED;
}

MagCalibration getCalibrationData() {
    return g_calibration;
}