#include <task_safe_wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>
#include <variables/setget.h>
#include <freertos/FreeRTOS.h>
#include <sensors/compass.h>
#include <sensors/kalman_filter.h>
#include <math.h>
#include <EEPROM.h>

// EEPROM addresses for calibration storage
#define EEPROM_CAL_MAGIC_ADDR    100
#define EEPROM_CAL_DATA_ADDR     104
#define CALIBRATION_MAGIC        0xCAFEBABE

/* Assign a unique ID to this sensor at the same time */
Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);

// Kalman filter objects for each axis (less aggressive filtering)
KalmanFilter kfX(0.1f, 0.2f, 1.0f, 0.0f);
KalmanFilter kfY(0.1f, 0.2f, 1.0f, 0.0f);
KalmanFilter kfZ(0.1f, 0.2f, 1.0f, 0.0f);

// Debugging counter
static int debugCounter = 0;

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

// Global calibration data
static MagCalibration g_calibration;
static bool g_calibrationMode = false;
static int g_calibrationSamples = 0;
static const int CALIBRATION_SAMPLES_NEEDED = 100;

// Min/max tracking for calibration
static float g_calMinX = 1000.0f, g_calMaxX = -1000.0f;
static float g_calMinY = 1000.0f, g_calMaxY = -1000.0f;
static float g_calMinZ = 1000.0f, g_calMaxZ = -1000.0f;

// Calibration functions
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
        
        // Save calibration to EEPROM for future use
        saveCalibrationToEEPROM();
        
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

// Save calibration data to EEPROM
void saveCalibrationToEEPROM() {
    EEPROM.begin(512);
    uint32_t magic = CALIBRATION_MAGIC;
    EEPROM.put(EEPROM_CAL_MAGIC_ADDR, magic);
    EEPROM.put(EEPROM_CAL_DATA_ADDR, g_calibration);
    EEPROM.commit();
    Serial.println("[MAG] ✅ Calibration saved to EEPROM");
}

// Load calibration data from EEPROM  
bool loadCalibrationFromEEPROM() {
    EEPROM.begin(512);
    uint32_t magic;
    EEPROM.get(EEPROM_CAL_MAGIC_ADDR, magic);
    
    if (magic == CALIBRATION_MAGIC) {
        EEPROM.get(EEPROM_CAL_DATA_ADDR, g_calibration);
        Serial.println("[MAG] ✅ Loaded calibration from EEPROM (from calibration program):");
        Serial.printf("[MAG] Offsets: X=%.2f, Y=%.2f, Z=%.2f\n", 
                      g_calibration.offsetX, g_calibration.offsetY, g_calibration.offsetZ);
        Serial.printf("[MAG] Scales: X=%.3f, Y=%.3f, Z=%.3f\n",
                      g_calibration.scaleX, g_calibration.scaleY, g_calibration.scaleZ);
        return true;
    }
    
    Serial.println("[MAG] ⚠️  No calibration found in EEPROM - using background calibration");
    return false;
}

// Background calibration - updates during normal operation
void updateBackgroundCalibration(float magX, float magY, float magZ) {
    static bool backgroundCalActive = false;
    static bool hasStoredCalibration = false;
    static float bgMinX = 1000.0f, bgMaxX = -1000.0f;
    static float bgMinY = 1000.0f, bgMaxY = -1000.0f;
    static float bgMinZ = 1000.0f, bgMaxZ = -1000.0f;
    static int bgSamples = 0;
    static uint32_t lastUpdate = 0;
    
    // Only update every 100ms to avoid excessive processing
    uint32_t now = millis();
    if (now - lastUpdate < 100) return;
    lastUpdate = now;
    
    // Determine if we should enable background calibration
    if (!backgroundCalActive) {
        // Check if we have stored calibration or need to build from scratch
        hasStoredCalibration = (g_calibration.offsetX != 0.0f || g_calibration.offsetY != 0.0f);
        
        if (!hasStoredCalibration) {
            // No stored calibration - enable aggressive background calibration
            backgroundCalActive = true;
            Serial.println("[MAG] 🚗 Background calibration active - building calibration from driving data");
        } else {
            // Have stored calibration - enable conservative refinement
            backgroundCalActive = true;  
            Serial.println("[MAG] 🔧 Background refinement active - fine-tuning existing calibration");
        }
    }
    
    if (!backgroundCalActive) return;
    
    // Track extremes
    if (magX < bgMinX) bgMinX = magX;
    if (magX > bgMaxX) bgMaxX = magX;
    if (magY < bgMinY) bgMinY = magY;
    if (magY > bgMaxY) bgMaxY = magY;
    if (magZ < bgMinZ) bgMinZ = magZ;
    if (magZ > bgMaxZ) bgMaxZ = magZ;
    
    bgSamples++;
    
    // Update calibration - different intervals based on stored calibration status
    int samplesNeeded = hasStoredCalibration ? 500 : 200; // More conservative if we have stored cal
    
    if (bgSamples >= samplesNeeded) {
        float rangeX = bgMaxX - bgMinX;
        float rangeY = bgMaxY - bgMinY;
        float rangeZ = bgMaxZ - bgMinZ;
        
        // Only update if we got reasonable range variation (truck has turned enough)
        float minRange = hasStoredCalibration ? 30.0f : 20.0f; // Higher threshold if we have stored cal
        
        if (rangeX > minRange && rangeY > minRange) {
            if (hasStoredCalibration) {
                // Conservative refinement - average with existing calibration
                g_calibration.offsetX = (g_calibration.offsetX + (bgMaxX + bgMinX) / 2.0f) / 2.0f;
                g_calibration.offsetY = (g_calibration.offsetY + (bgMaxY + bgMinY) / 2.0f) / 2.0f;
                g_calibration.offsetZ = (g_calibration.offsetZ + (bgMaxZ + bgMinZ) / 2.0f) / 2.0f;
                Serial.println("[MAG] 🔧 Refined existing calibration from driving data");
            } else {
                // Build new calibration from scratch
                g_calibration.offsetX = (bgMaxX + bgMinX) / 2.0f;
                g_calibration.offsetY = (bgMaxY + bgMinY) / 2.0f;
                g_calibration.offsetZ = (bgMaxZ + bgMinZ) / 2.0f;
                
                // Calculate scale factors
                float avgRange = (rangeX + rangeY + rangeZ) / 3.0f;
                if (rangeX > 10.0f) g_calibration.scaleX = avgRange / rangeX;
                if (rangeY > 10.0f) g_calibration.scaleY = avgRange / rangeY;
                if (rangeZ > 10.0f) g_calibration.scaleZ = avgRange / rangeZ;
                
                Serial.println("[MAG] 🎯 Built new calibration from driving data");
                hasStoredCalibration = true; // Now we have calibration
            }
            
            // Save improved calibration
            saveCalibrationToEEPROM();
        } else {
            Serial.printf("[MAG] Insufficient range for calibration update (X:%.1f Y:%.1f - need >%.1f)\n", 
                         rangeX, rangeY, minRange);
        }
        
        // Reset for next round
        bgSamples = 0;
        bgMinX = bgMaxX = magX;
        bgMinY = bgMaxY = magY;
        bgMinZ = bgMaxZ = magZ;
    }
}

// Force calibration mode (call this during initial setup)
void forceCalibrationMode() {
    Serial.println("[MAG] 🔧 FORCED CALIBRATION MODE");
    Serial.println("[MAG] Manually rotate the truck in all directions for 10 seconds...");
    startCalibrationMode();
}

Compass::Compass()
{
}

static void compass_task(void *pvParameters)
{
  // Wait for system to stabilize
  vTaskDelay(pdMS_TO_TICKS(2000));
  
  // Try to load calibration from EEPROM (from calibration program)
  bool hasStoredCalibration = loadCalibrationFromEEPROM();
  
  if (hasStoredCalibration) {
    Serial.println("[MAG] 🎯 Using stored calibration from calibration program");
  } else {
    Serial.println("[MAG] 🚗 No stored calibration - enabling background auto-calibration");
    Serial.println("[MAG] Compass will improve accuracy as truck drives and turns");
    // Initialize with neutral defaults for background calibration
    g_calibration.offsetX = 0.0f;
    g_calibration.offsetY = 0.0f;
    g_calibration.offsetZ = 0.0f;
    g_calibration.scaleX = 1.0f;
    g_calibration.scaleY = 1.0f;
    g_calibration.scaleZ = 1.0f;
  }
  
  for (;;)
  {
    sensors_event_t event;
    if (!task_safe_wire_try_lock(5)) {
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }
    mag.getEvent(&event);
    task_safe_wire_unlock();

    float magx = event.magnetic.x;
    float magy = event.magnetic.y;
    float magz = event.magnetic.z;

    // Debug output every 50 readings (every ~1 second)
    debugCounter++;
    if (debugCounter >= 50) {
      Serial.printf("[MAG] Raw: X=%.2f, Y=%.2f, Z=%.2f uT\n", magx, magy, magz);
      debugCounter = 0;
    }

    // Handle calibration if in calibration mode
    if (!isCalibrationComplete()) {
      updateCalibration(magx, magy, magz);
    }
    
    // Background calibration during normal operation
    updateBackgroundCalibration(magx, magy, magz);
    
    // Apply calibration corrections
    if (isCalibrationComplete() || g_calibration.offsetX != 0.0f) {
      MagCalibration cal = getCalibrationData();
      applyCalibratedValues(magx, magy, magz, cal);
    }

    // Update estimates using Kalman filters
    float filteredMagX = kfX.updateEstimate(magx);
    float filteredMagY = kfY.updateEstimate(magy);
    float filteredMagZ = kfZ.updateEstimate(magz);

    // Debug filtered values occasionally
    static int filteredDebugCounter = 0;
    filteredDebugCounter++;
    if (filteredDebugCounter >= 100) {
      Serial.printf("[MAG] Filtered: X=%.2f, Y=%.2f, Z=%.2f uT\n", 
                    filteredMagX, filteredMagY, filteredMagZ);
      
      // Calculate and validate compass heading
      float magnitude = sqrt(filteredMagX * filteredMagX + filteredMagY * filteredMagY);
      if (magnitude > 1.0f) { // Only calculate if we have sufficient magnetic field
        float heading = atan2(filteredMagY, filteredMagX) * 180.0 / PI;
        if (heading < 0) heading += 360.0;
        Serial.printf("[MAG] Heading: %.1f° (magnitude: %.1f uT)\n", heading, magnitude);
      } else {
        Serial.println("[MAG] Warning: Insufficient magnetic field for compass");
      }
      filteredDebugCounter = 0;
    }

    globalVar_set(rawMagX, filteredMagX);
    globalVar_set(rawMagY, filteredMagY);
    globalVar_set(rawMagZ, filteredMagZ);
    
    // Calculate and store compass heading
    float magnitude = sqrt(filteredMagX * filteredMagX + filteredMagY * filteredMagY);
    if (magnitude > 1.0f) { // Only calculate if we have sufficient magnetic field
      float heading = atan2(filteredMagY, filteredMagX) * 180.0 / PI;
      if (heading < 0) heading += 360.0;
      
      // Store heading in 1/10 degrees as expected by the interface
      long headingTenths = (long)(heading * 10.0);
      globalVar_set(calcHeading, headingTenths);
    } else {
      // Insufficient magnetic field - set invalid heading
      globalVar_set(calcHeading, -1); // Use -1 to indicate invalid reading
    }
    
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}


void Compass::Begin()
{
  globalVar_set(rawMagX, 0);
  globalVar_set(rawMagY, 0);
  globalVar_set(rawMagZ, 0);
  // Wire initialization is handled by task_safe_wire
  
  task_safe_wire_lock();
  bool magOk = mag.begin();
  task_safe_wire_unlock();

  if (!magOk)
  {
    Serial.println("❌ Failed to initialize HMC5883 sensor! Check wiring/I2C address. Compass disabled.");
    return;  // Don't hang – let setup() continue without compass
  }

  // Display sensor details for debugging
  sensor_t sensor;
  task_safe_wire_lock();
  mag.getSensor(&sensor);
  task_safe_wire_unlock();
  Serial.printf("✅ Magnetometer initialized: %s\n", sensor.name);
  Serial.printf("   Range: %.1f to %.1f uT\n", sensor.min_value, sensor.max_value);
  Serial.printf("   Resolution: %.3f uT\n", sensor.resolution);

  Serial.println("🧭 Starting Compass task...");
  xTaskCreate(
      compass_task,  // Task function
      "compasstask", // Task name
      4096,        // Stack size (increased from 2000 to prevent overflow)
      NULL,        // Task input parameter
      1,           // Task priority (lowered from 2 to reduce conflicts)
      NULL         // Task handle
  );


}

void displaySensorDetails(void)
{
  sensor_t sensor;
  task_safe_wire_lock();
  mag.getSensor(&sensor);
  task_safe_wire_unlock();
  Serial.println("------------------------------------");
  Serial.print("Sensor:       ");
  Serial.println(sensor.name);
  Serial.print("Driver Ver:   ");
  Serial.println(sensor.version);
  Serial.print("Unique ID:    ");
  Serial.println(sensor.sensor_id);
  Serial.print("Max Value:    ");
  Serial.print(sensor.max_value);
  Serial.println(" uT");
  Serial.print("Min Value:    ");
  Serial.print(sensor.min_value);
  Serial.println(" uT");
  Serial.print("Resolution:   ");
  Serial.print(sensor.resolution);
  Serial.println(" uT");
  Serial.println("------------------------------------");
  Serial.println("");
  vTaskDelay(pdMS_TO_TICKS(500));
}


