// Dedicated Calibration and Setup Program for ATMT Racing Truck
// Purpose: 
// - Perform compass calibration with manual rotation
// - Save calibration data to EEPROM for main program
// - Test all sensors and actuators
// - Setup configuration parameters

#include <Arduino.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <task_safe_wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>
#include <variables/setget.h>
#include <sensors/usensor.h>
#include <sensors/accsensor.h>
#include <sensors/compass.h>
#include <actuators/motor.h>
#include <actuators/steer.h>
#include <telemetry/mqtt.h>
#include <secrets.h>
#include <atmio.h>

// EEPROM layout
#define EEPROM_CAL_MAGIC_ADDR    100
#define EEPROM_CAL_DATA_ADDR     104
#define EEPROM_CONFIG_ADDR       150
#define CALIBRATION_MAGIC        0xCAFEBABE
#define CONFIG_MAGIC             0xDEADC0DE

// Global objects
Usensor ultraSound;
ACCsensor accelSensor;
Compass compass;
Motor motor;
Steer steer;
Mqtt mqtt;

String chipId;

// Calibration data structure (must match compass.cpp)
struct MagCalibration {
    float offsetX = 0.0f;
    float offsetY = 0.0f; 
    float offsetZ = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float scaleZ = 1.0f;
    float minX = -1000.0f, maxX = 1000.0f;
    float minY = -1000.0f, maxY = 1000.0f;
    float minZ = -1000.0f, maxZ = 1000.0f;
};

// Truck configuration
struct TruckConfig {
    // PWM settings
    int pwmExplore = 90;
    int pwmSlow = 80;
    int pwmReverse = -80;
    
    // Distance thresholds (cm)
    float frontStopDist = 12.0f;
    float frontSlowDist = 20.0f;
    float sideMinDist = 6.0f;
    float centerDeadband = 3.0f;
    
    // Filtering
    float usAlpha = 0.35f;
    float imuAlpha = 0.25f;
    
    // Timing
    uint32_t telemetryPeriodMs = 1000;
    uint32_t recoverReverseMs = 500;
    uint32_t recoverTurnMs = 600;
};

TruckConfig truckConfig;

// Save calibration to EEPROM
void saveCalibrationToEEPROM(const MagCalibration& cal) {
    EEPROM.begin(512);
    uint32_t magic = CALIBRATION_MAGIC;
    EEPROM.put(EEPROM_CAL_MAGIC_ADDR, magic);
    EEPROM.put(EEPROM_CAL_DATA_ADDR, cal);
    EEPROM.commit();
    Serial.println("✅ Calibration saved to EEPROM");
}

// Save config to EEPROM
void saveConfigToEEPROM(const TruckConfig& config) {
    EEPROM.begin(512);
    uint32_t magic = CONFIG_MAGIC;
    EEPROM.put(EEPROM_CONFIG_ADDR - 4, magic);
    EEPROM.put(EEPROM_CONFIG_ADDR, config);
    EEPROM.commit();
    Serial.println("✅ Configuration saved to EEPROM");
}

// Perform compass calibration
bool calibrateCompass() {
    Serial.println("\n🧭 === COMPASS CALIBRATION ===");
    Serial.println("This will take 15 seconds.");
    Serial.println("MANUALLY ROTATE the truck in ALL directions:");
    Serial.println("- Pitch up/down");
    Serial.println("- Roll left/right");  
    Serial.println("- Yaw (turn) left/right");
    Serial.println("\nPress ENTER to start calibration...");
    
    // Wait for user input
    while (!Serial.available()) {
        delay(100);
    }
    while (Serial.available()) Serial.read(); // Clear buffer
    
    Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);
    if (!mag.begin()) {
        Serial.println("❌ Failed to initialize compass!");
        return false;
    }
    
    MagCalibration cal;
    float minX = 1000.0f, maxX = -1000.0f;
    float minY = 1000.0f, maxY = -1000.0f;
    float minZ = 1000.0f, maxZ = -1000.0f;
    
    Serial.println("🔄 Calibration starting in 3 seconds...");
    delay(3000);
    
    Serial.println("🔄 ROTATE THE TRUCK NOW! (15 seconds)");
    uint32_t startTime = millis();
    int samples = 0;
    
    while ((millis() - startTime) < 15000) {
        sensors_event_t event;
        mag.getEvent(&event);
        
        float magX = event.magnetic.x;
        float magY = event.magnetic.y;
        float magZ = event.magnetic.z;
        
        if (magX < minX) minX = magX;
        if (magX > maxX) maxX = magX;
        if (magY < minY) minY = magY;
        if (magY > maxY) maxY = magY;
        if (magZ < minZ) minZ = magZ;
        if (magZ > maxZ) maxZ = magZ;
        
        samples++;
        
        // Progress indicator
        if (samples % 50 == 0) {
            int progress = ((millis() - startTime) * 100) / 15000;
            Serial.printf("Progress: %d%% (X: %.1f to %.1f, Y: %.1f to %.1f)\n", 
                         progress, minX, maxX, minY, maxY);
        }
        
        delay(20);
    }
    
    Serial.println("✅ Calibration data collection complete!");
    
    // Calculate calibration values  
    cal.offsetX = (maxX + minX) / 2.0f;
    cal.offsetY = (maxY + minY) / 2.0f;
    cal.offsetZ = (maxZ + minZ) / 2.0f;
    
    float rangeX = maxX - minX;
    float rangeY = maxY - minY;
    float rangeZ = maxZ - minZ;
    
    // Check if we got good ranges
    if (rangeX < 20.0f || rangeY < 20.0f) {
        Serial.printf("⚠️  WARNING: Limited rotation detected (X range: %.1f, Y range: %.1f)\n", rangeX, rangeY);
        Serial.println("For best accuracy, ensure truck was rotated in all directions");
    }
    
    // Calculate scale factors
    float avgRange = (rangeX + rangeY + rangeZ) / 3.0f;
    if (rangeX > 10.0f) cal.scaleX = avgRange / rangeX;
    if (rangeY > 10.0f) cal.scaleY = avgRange / rangeY;
    if (rangeZ > 10.0f) cal.scaleZ = avgRange / rangeZ;
    
    // Set validation ranges
    cal.minX = minX - 50.0f; cal.maxX = maxX + 50.0f;
    cal.minY = minY - 50.0f; cal.maxY = maxY + 50.0f;
    cal.minZ = minZ - 50.0f; cal.maxZ = maxZ + 50.0f;
    
    Serial.printf("\n📊 CALIBRATION RESULTS:\n");
    Serial.printf("Offsets: X=%.2f, Y=%.2f, Z=%.2f\n", cal.offsetX, cal.offsetY, cal.offsetZ);
    Serial.printf("Scales:  X=%.3f, Y=%.3f, Z=%.3f\n", cal.scaleX, cal.scaleY, cal.scaleZ);
    Serial.printf("Ranges:  X=%.1f, Y=%.1f, Z=%.1f\n", rangeX, rangeY, rangeZ);
    Serial.printf("Samples: %d\n", samples);
    
    saveCalibrationToEEPROM(cal);
    return true;
}

// Test all sensors
void testSensors() {
    Serial.println("\n🔬 === SENSOR TEST ===");
    
    // Test ultrasonic sensors
    Serial.println("Testing ultrasonic sensors...");
    globalVar_set(rawDistFront, 25);  // Simulate values for testing
    globalVar_set(rawDistLeft, 30);
    globalVar_set(rawDistRight, 35);
    globalVar_set(rawDistBack, 40);
    
    delay(1000);
    
    long distF = globalVar_get(rawDistFront, nullptr);
    long distL = globalVar_get(rawDistLeft, nullptr);
    long distR = globalVar_get(rawDistRight, nullptr);
    long distB = globalVar_get(rawDistBack, nullptr);
    
    Serial.printf("Distances - Front: %ld, Left: %ld, Right: %ld, Back: %ld\n", distF, distL, distR, distB);
    
    // Test compass
    Serial.println("Testing compass (5 second sample)...");
    for (int i = 0; i < 25; i++) {
        long magX = globalVar_get(rawMagX, nullptr);
        long magY = globalVar_get(rawMagY, nullptr);
        long heading = globalVar_get(calcHeading, nullptr);
        
        Serial.printf("Compass - MagX: %ld, MagY: %ld, Heading: %.1f°\n", 
                     magX, magY, heading / 10.0f);
        delay(200);
    }
    
    Serial.println("✅ Sensor test complete");
}

// Test actuators  
void testActuators() {
    Serial.println("\n⚙️  === ACTUATOR TEST ===");
    Serial.println("Testing motor and steering...");
    
    Serial.println("Motor forward...");
    motor.driving(30);
    delay(2000);
    
    Serial.println("Motor stop...");
    motor.driving(0);
    delay(1000);
    
    Serial.println("Motor reverse...");
    motor.driving(-30);
    delay(2000);
    
    Serial.println("Motor stop...");
    motor.driving(0);
    delay(1000);
    
    Serial.println("Steering left...");
    steer.Left();
    delay(2000);
    
    Serial.println("Steering right...");
    steer.Right(); 
    delay(2000);
    
    Serial.println("Steering straight...");
    steer.Straight();
    delay(1000);
    
    Serial.println("✅ Actuator test complete");
}

// Configuration menu
void configureSettings() {
    Serial.println("\n⚙️  === CONFIGURATION ===");
    Serial.println("Current settings:");
    Serial.printf("PWM Explore: %d\n", truckConfig.pwmExplore);
    Serial.printf("PWM Slow: %d\n", truckConfig.pwmSlow);
    Serial.printf("Front Stop Distance: %.1f cm\n", truckConfig.frontStopDist);
    Serial.printf("Side Min Distance: %.1f cm\n", truckConfig.sideMinDist);
    
    Serial.println("\nPress 's' to save current config, or ENTER to skip:");
    
    while (!Serial.available()) {
        delay(100);
    }
    
    char input = Serial.read();
    while (Serial.available()) Serial.read(); // Clear buffer
    
    if (input == 's' || input == 'S') {
        saveConfigToEEPROM(truckConfig);
    } else {
        Serial.println("Configuration not saved");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n🏁 === ATMT RACING TRUCK CALIBRATION & SETUP ===");
    Serial.println("This program calibrates and configures your racing truck");
    
    // Get chip ID
    uint64_t chipIdHex = ESP.getEfuseMac();
    chipId = String((uint32_t)(chipIdHex >> 32), HEX) + String((uint32_t)chipIdHex, HEX);
    Serial.printf("Truck ID: %s\n", chipId.c_str());
    
    // Initialize global variables
    globalVar_init();
    
    // Initialize sensors
    Serial.println("Initializing sensors...");
    accelSensor.Begin();
    delay(500);
    compass.Begin();
    delay(500);
    
    // Initialize ultrasonic sensors
    ultraSound.open(TRIGGER_PIN1, ECHO_PIN1, rawDistFront);
    delay(100);
    ultraSound.open(TRIGGER_PIN2, ECHO_PIN2, rawDistRight);
    delay(100);
    ultraSound.open(TRIGGER_PIN3, ECHO_PIN3, rawDistLeft);
    delay(100);
    ultraSound.open(TRIGGER_PIN4, ECHO_PIN4, rawDistBack);
    delay(100);
    
    // Initialize actuators
    steer.Begin();
    delay(100);
    
    Serial.println("✅ Hardware initialized");
    
    // Main calibration menu
    bool done = false;
    while (!done) {
        Serial.println("\n🎯 === CALIBRATION MENU ===");
        Serial.println("1. Calibrate Compass");
        Serial.println("2. Test Sensors");
        Serial.println("3. Test Actuators");
        Serial.println("4. Configure Settings");
        Serial.println("5. Exit and Reboot");
        Serial.println("\nEnter choice (1-5): ");
        
        while (!Serial.available()) {
            delay(100);
        }
        
        char choice = Serial.read();
        while (Serial.available()) Serial.read(); // Clear buffer
        
        switch (choice) {
            case '1':
                calibrateCompass();
                break;
            case '2':
                testSensors();
                break;
            case '3':
                testActuators();
                break;
            case '4':
                configureSettings();
                break;
            case '5':
                done = true;
                break;
            default:
                Serial.println("Invalid choice");
                break;
        }
        
        delay(1000);
    }
    
    Serial.println("\n🎉 Calibration complete!");
    Serial.println("Load the main racing program to start driving.");
    Serial.println("Rebooting in 3 seconds...");
    
    delay(3000);
    ESP.restart();
}

void loop() {
    // Nothing here - everything happens in setup()
    delay(1000);
}