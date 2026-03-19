// Minimal exploration controller skeleton for an ESP32-based truck
// Purpose:
// - Read filtered sensor values
// - Run a simple state machine: IDLE / EXPLORE / RECOVER / STOP
// - Apply basic wall-centering + obstacle avoidance
// - Publish telemetry periodically
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <stdarg.h>
#include <math.h>
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_bt.h"
#include <variables/setget.h>
#include <sensors/usensor.h>
#include <sensors/accsensor.h>
#include <sensors/compass.h>
#include <actuators/motor.h>
#include <actuators/steer.h>
#include <telemetry/mqtt.h>
#include <secrets.h>
#include <atmio.h>

// Global objects
Usensor ultraSound;
ACCsensor accelSensor;
Compass compass;
Motor motor;
Steer steer;
Mqtt mqtt;

char chipId[17];  // Fixed size char array for chip ID (16 hex chars + null terminator)
long int age;

// -----------------------------
// Configuration
// -----------------------------
enum class RunMode {
  REAL,     // Minimal serial output for production
  DEBUG     // Full serial output for development
};

struct ControlConfig {
  // Distances in cm
  float frontStopDist = 12.0f;
  float frontSlowDist = 20.0f;
  float sideMinDist   = 6.0f;

  // Corridor-centering
  float centerDeadband = 3.0f;     // |UR - UL| below this => go straight
  uint8_t steerHoldSamples = 3;    // hysteresis before changing steering

  // PWM
  int pwmExplore = 100;
  int pwmSlow    = 100;
  int pwmReverse = -100;

  // Timing
  uint32_t telemetryPeriodMs = 1000;  // 1 Hz to reduce heap pressure
  uint32_t recoverReverseMs  = 1000;
  uint32_t recoverTurnMs     = 1000;
  uint32_t stuckFrontMs      = 500;

  // Filtering
  float usAlpha = 0.35f;          // exponential smoothing factor
  float imuAlpha = 0.25f;
  
  // Run mode
  RunMode runMode = RunMode::REAL;  // Set to REAL for production
};

ControlConfig cfg;

// -----------------------------
// Basic types
// -----------------------------
enum class Mode {
  IDLE,
  EXPLORE,
  RECOVER,
  STOP
};

enum class SteerCmd {
  LEFT,
  STRAIGHT,
  RIGHT
};

struct RawSensors {
  float ul = NAN;       // ultrasonic left
  float ur = NAN;       // ultrasonic right
  float uf = NAN;       // ultrasonic front
  float ub = NAN;       // ultrasonic back
  float yawRate = NAN;  // deg/s
  float heading = NAN;  // deg
  // Compass values
  float magX = NAN;     // magnetic field X
  float magY = NAN;     // magnetic field Y
  float magZ = NAN;     // magnetic field Z
  float compass = NAN;  // calculated compass heading
  // Accelerometer values
  float accX = NAN;     // acceleration X
  float accY = NAN;     // acceleration Y
  float accZ = NAN;     // acceleration Z
};

struct FilteredSensors {
  float ul = NAN;
  float ur = NAN;
  float uf = NAN;
  float ub = NAN;
  float yawRate = NAN;
  float heading = NAN;
  // Compass values
  float magX = NAN;
  float magY = NAN;
  float magZ = NAN;
  float compass = NAN;
  // Accelerometer values
  float accX = NAN;
  float accY = NAN;
  float accZ = NAN;
};

struct Features {
  float width = NAN;         // ul + ur
  float centerError = NAN;   // ur - ul
  bool frontBlocked = false;
  bool rearBlocked = false;
  bool narrow = false;
};

struct Command {
  int motorPwm = 0;                  // negative = reverse
  SteerCmd steer = SteerCmd::STRAIGHT;
};

struct TelemetryFrame {
  uint32_t seq = 0;
  uint32_t tMs = 0;
  Mode mode = Mode::IDLE;

  RawSensors raw;
  FilteredSensors filt;
  Features feat;
  Command cmd;
};

// -----------------------------
// Global state
// -----------------------------
Mode g_mode = Mode::IDLE;
Command g_cmd;

RawSensors g_raw;
FilteredSensors g_filt;
Features g_feat;

uint32_t g_seq = 0;
uint32_t g_lastTelemetryMs = 0;
uint32_t g_lastMqttStatusMs = 0;  // Add MQTT status reporting timer
uint32_t g_frontBlockedSinceMs = 0;
uint32_t g_recoverStartMs = 0;

SteerCmd g_lastRequestedSteer = SteerCmd::STRAIGHT;
SteerCmd g_appliedSteer = SteerCmd::STRAIGHT;
uint8_t g_sameSteerCount = 0;

// -----------------------------
// Helpers
// -----------------------------
static void debugPrint(const String& message) {
  if (cfg.runMode == RunMode::DEBUG) {
    Serial.println(message);
  }
}

static void debugPrintf(const char* format, ...) {
  if (cfg.runMode == RunMode::DEBUG) {
    va_list args;
    va_start(args, format);
    Serial.printf(format, args);
    va_end(args);
  }
}

static float ema(float prev, float current, float alpha) {
  if (isnan(prev)) return current;
  if (isnan(current)) return prev;
  return alpha * current + (1.0f - alpha) * prev;
}

static const char* modeToString(Mode m) {
  switch (m) {
    case Mode::IDLE:    return "IDLE";
    case Mode::EXPLORE: return "EXPLORE";
    case Mode::RECOVER: return "RECOVER";
    case Mode::STOP:    return "STOP";
    default:            return "UNKNOWN";
  }
}

static const char* steerToString(SteerCmd s) {
  switch (s) {
    case SteerCmd::LEFT:     return "LEFT";
    case SteerCmd::STRAIGHT: return "STRAIGHT";
    case SteerCmd::RIGHT:    return "RIGHT";
    default:                 return "UNKNOWN";
  }
}

// -----------------------------
// Hardware abstraction stubs
// Replace these with your real implementations
// -----------------------------
float readUltrasonicLeftCm()   { 
  long val = globalVar_get(rawDistLeft, &age); 
  return (float)val;
}
float readUltrasonicRightCm()  { 
  long val = globalVar_get(rawDistRight, &age); 
  return (float)val;
}
float readUltrasonicFrontCm()  { 
  long val = globalVar_get(rawDistFront, &age); 
  return (float)val;
}
float readUltrasonicBackCm()   { 
  long val = globalVar_get(rawDistBack, &age); 
  return (float)val;
}
float readYawRateDegPerSec()   { 
  // Convert raw gyro Z to deg/s (divide by 131 for MPU6050)
  long raw = globalVar_get(rawGyZ, &age);
  return raw / 131.0f;
}
float readHeadingDeg()         { 
  // Get calculated heading from compass (in 1/10 degrees)
  long heading = globalVar_get(calcHeading, &age);
  if (heading == -1) {
    return NAN; // Invalid heading from compass task
  }
  return heading / 10.0f;
}

float readMagneticX() {
  long val = globalVar_get(rawMagX, &age);
  return (float)val;
}

float readMagneticY() {
  long val = globalVar_get(rawMagY, &age);
  return (float)val;
}

float readMagneticZ() {
  long val = globalVar_get(rawMagZ, &age);
  return (float)val;
}

float readAccelerationX() {
  long val = globalVar_get(rawAccX, &age);
  return (float)val;
}

float readAccelerationY() {
  long val = globalVar_get(rawAccY, &age);
  return (float)val;
}

float readAccelerationZ() {
  long val = globalVar_get(rawAccZ, &age);
  return (float)val;
}

void setMotorPwm(int pwm) {
  // Map signed PWM to motor driver direction + duty cycle
  // PWM range: -100 (full reverse) to +100 (full forward)
  int clampedPwm = constrain(pwm, -100, 100);
  motor.driving(clampedPwm);
}

void setSteering(SteerCmd steerCmd) {
  // Control steering hardware based on command
  switch (steerCmd) {
    case SteerCmd::LEFT:
      steer.Left();
      break;
    case SteerCmd::RIGHT:
      steer.Right();
      break;
    case SteerCmd::STRAIGHT:
    default:
      steer.Straight();
      break;
  }
}

bool mqttConnected() {
  return mqtt.connected();
}

void mqttLoop() {
  // MQTT with enhanced thread safety
  if (ESP.getFreeHeap() < 6000) {
    return; // Skip MQTT when low memory
  }
  
  try {
    mqtt.loop();
  } catch (...) {
    // Ignore MQTT errors to prevent crashes
  }
  yield();
}

void publishTelemetry(const TelemetryFrame& t) {
  // MQTT publishing with enhanced safety
  if (!mqttConnected()) {
    return;  // Silent fail
  }
  
  // Check memory before proceeding
  if (ESP.getFreeHeap() < 8000) {
    return;  // Silent fail on low memory
  }
  
  yield();
  
  // Check chipId is valid before using it
  if (strlen(chipId) == 0) {
    uint64_t chipIdHex = ESP.getEfuseMac();
    snprintf(chipId, sizeof(chipId), "%x", (uint32_t)chipIdHex);
  }
  
  // Use static buffer to prevent any heap allocation
  static char jsonStr[512];  // Increased to fit full telemetry payload
  memset(jsonStr, 0, sizeof(jsonStr));  // Clear buffer completely
  
  int written = snprintf(jsonStr, sizeof(jsonStr),
    "{"
    "\"truck_id\":\"%s\","
    "\"seq\":%u,"
    "\"t_ms\":%lu,"
    "\"mode\":\"%s\","
    "\"ul\":%.1f,"
    "\"ur\":%.1f,"
    "\"uf\":%.1f,"
    "\"ub\":%.1f,"
    "\"yaw_rate\":%.2f,"
    "\"heading\":%.1f,"
    "\"compass\":%.1f,"
    "\"mag_x\":%.2f,"
    "\"mag_y\":%.2f,"
    "\"mag_z\":%.2f,"
    "\"acc_x\":%.2f,"
    "\"acc_y\":%.2f,"
    "\"acc_z\":%.2f,"
    "\"width\":%.1f,"
    "\"center_error\":%.1f,"
    "\"front_blocked\":%s,"
    "\"cmd_pwm\":%d,"
    "\"cmd_steer\":\"%s\""
    "}",
    chipId, (unsigned)t.seq, (unsigned long)t.tMs, modeToString(t.mode),
    t.filt.ul, t.filt.ur, t.filt.uf, t.filt.ub,
    t.filt.yawRate, t.filt.heading, t.filt.compass,
    t.filt.magX, t.filt.magY, t.filt.magZ,
    t.filt.accX, t.filt.accY, t.filt.accZ,
    t.feat.width, t.feat.centerError,
    t.feat.frontBlocked ? "true" : "false",
    t.cmd.motorPwm, steerToString(t.cmd.steer)
  );
  
  // Safety check - ensure we didn't overflow buffer
  if (written >= sizeof(jsonStr)) {
    return; // Skip this telemetry if too large
  }
  
  // Publish with basic error checking
  if (mqttConnected() && ESP.getFreeHeap() > 6000) {
    mqtt.send("telemetry", jsonStr);
  }
  yield();
}

void filterSensors(const RawSensors& raw, FilteredSensors& filt) {
  filt.ul = ema(filt.ul, raw.ul, cfg.usAlpha);
  filt.ur = ema(filt.ur, raw.ur, cfg.usAlpha);
  filt.uf = ema(filt.uf, raw.uf, cfg.usAlpha);
  filt.ub = ema(filt.ub, raw.ub, cfg.usAlpha);

  filt.yawRate = ema(filt.yawRate, raw.yawRate, cfg.imuAlpha);
  filt.heading = ema(filt.heading, raw.heading, cfg.imuAlpha);
  
  // Filter compass values
  filt.magX = ema(filt.magX, raw.magX, cfg.imuAlpha);
  filt.magY = ema(filt.magY, raw.magY, cfg.imuAlpha);
  filt.magZ = ema(filt.magZ, raw.magZ, cfg.imuAlpha);
  // Compass is same as heading now
  filt.compass = filt.heading;
  
  // Filter accelerometer values
  filt.accX = ema(filt.accX, raw.accX, cfg.imuAlpha);
  filt.accY = ema(filt.accY, raw.accY, cfg.imuAlpha);
  filt.accZ = ema(filt.accZ, raw.accZ, cfg.imuAlpha);
}

void deriveFeatures(const FilteredSensors& filt, Features& feat) {
  feat.width = filt.ul + filt.ur;
  feat.centerError = filt.ur - filt.ul;

  feat.frontBlocked = !isnan(filt.uf) && filt.uf < cfg.frontStopDist;
  feat.rearBlocked  = !isnan(filt.ub) && filt.ub < cfg.frontStopDist;
  feat.narrow = !isnan(feat.width) && feat.width < 22.0f;  // tune for your track
}

// -----------------------------
// Steering hysteresis
// -----------------------------
SteerCmd requestSteerWithHysteresis(SteerCmd requested) {
  if (requested == g_lastRequestedSteer) {
    if (g_sameSteerCount < 255) g_sameSteerCount++;
  } else {
    g_lastRequestedSteer = requested;
    g_sameSteerCount = 1;
  }

  if (g_sameSteerCount >= cfg.steerHoldSamples) {
    g_appliedSteer = requested;
  }

  return g_appliedSteer;
}

// -----------------------------
// Controller logic
// -----------------------------
Command computeExploreCommand(const Features& feat, const FilteredSensors& filt) {
  Command cmd;
  cmd.motorPwm = cfg.pwmExplore;
  cmd.steer = SteerCmd::STRAIGHT;

  // Front safety
  if (!isnan(filt.uf) && filt.uf < cfg.frontStopDist) {
    cmd.motorPwm = 0;

    // Turn away from the closer wall
    if (filt.ul < filt.ur) {
      cmd.steer = SteerCmd::RIGHT;
    } else {
      cmd.steer = SteerCmd::LEFT;
    }
    return cmd;
  }

  if (!isnan(filt.uf) && filt.uf < cfg.frontSlowDist) {
    cmd.motorPwm = cfg.pwmSlow;
  }

  // Corridor-centering
  if (!isnan(feat.centerError)) {
    if (feat.centerError > cfg.centerDeadband) {
      // Right side farther away => truck is biased left => steer right
      cmd.steer = SteerCmd::RIGHT;
    } else if (feat.centerError < -cfg.centerDeadband) {
      cmd.steer = SteerCmd::LEFT;
    } else {
      cmd.steer = SteerCmd::STRAIGHT;
    }
  }

  // Extra safety if too close to side
  if (!isnan(filt.ul) && filt.ul < cfg.sideMinDist) {
    cmd.steer = SteerCmd::RIGHT;
    cmd.motorPwm = cfg.pwmSlow;
  }
  if (!isnan(filt.ur) && filt.ur < cfg.sideMinDist) {
    cmd.steer = SteerCmd::LEFT;
    cmd.motorPwm = cfg.pwmSlow;
  }

  cmd.steer = requestSteerWithHysteresis(cmd.steer);
  return cmd;
}

Command computeRecoverCommand(const Features& feat, const FilteredSensors& filt, uint32_t nowMs) {
  Command cmd;

  uint32_t dt = nowMs - g_recoverStartMs;

  // Step 1: reverse
  if (dt < cfg.recoverReverseMs) {
    cmd.motorPwm = cfg.pwmReverse;
    cmd.steer = SteerCmd::STRAIGHT;
    return cmd;
  }

  // Step 2: turn away from nearest wall
  if (dt < (cfg.recoverReverseMs + cfg.recoverTurnMs)) {
    cmd.motorPwm = cfg.pwmSlow;

    if (filt.ul < filt.ur) {
      cmd.steer = SteerCmd::RIGHT;
    } else {
      cmd.steer = SteerCmd::LEFT;
    }
    return cmd;
  }

  // Recovery finished, return to explore
  g_mode = Mode::EXPLORE;
  cmd.motorPwm = cfg.pwmSlow;
  cmd.steer = SteerCmd::STRAIGHT;
  return cmd;
}

// -----------------------------
// State machine
// -----------------------------
void enterRecover(uint32_t nowMs) {
  g_mode = Mode::RECOVER;
  g_recoverStartMs = nowMs;
}

void updateStateMachine(const Features& feat, const FilteredSensors& filt, uint32_t nowMs) {
  switch (g_mode) {
    case Mode::IDLE:
      // stay idle until you explicitly switch to EXPLORE
      break;

    case Mode::EXPLORE:
      if (feat.frontBlocked) {
        if (g_frontBlockedSinceMs == 0) {
          g_frontBlockedSinceMs = nowMs;
        } else if ((nowMs - g_frontBlockedSinceMs) > cfg.stuckFrontMs) {
          enterRecover(nowMs);
        }
      } else {
        g_frontBlockedSinceMs = 0;
      }
      break;

    case Mode::RECOVER:
      // handled by timer inside computeRecoverCommand()
      break;

    case Mode::STOP:
      break;
  }
}

// -----------------------------
// Command application
// -----------------------------
void applyCommand(const Command& cmd) {
  setSteering(cmd.steer);
  setMotorPwm(cmd.motorPwm);
}

// -----------------------------
// Telemetry
// -----------------------------
void maybePublishTelemetry(uint32_t nowMs) {
  if ((nowMs - g_lastTelemetryMs) < cfg.telemetryPeriodMs) {
    return;
  }
  g_lastTelemetryMs = nowMs;

  TelemetryFrame t;
  t.seq = g_seq++;
  t.tMs = nowMs;
  t.mode = g_mode;
  t.raw = g_raw;
  t.filt = g_filt;
  t.feat = g_feat;
  t.cmd = g_cmd;

  publishTelemetry(t);
}

// -----------------------------
// Setup / loop
// -----------------------------
void setup() {
  // ---- Power / stability fixes for truck #2 ----
  // Disable brownout detector: WiFi radio draws ~400mA peak which can dip
  // the supply below the default 2.77V threshold on weaker power rails.
  // Remove this line once you have confirmed stable power on all trucks.
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // Release Bluetooth memory (~50 KB heap) and cut ~100 mA peak RF current.
  // We only use WiFi, so BT is never needed.
  esp_bt_controller_deinit();

  Serial.begin(115200);
  delay(200);  // let UART settle before first print
  yield(); // Early yield

  // Configure watchdog timer more safely
  esp_task_wdt_init(30, false);  // 30 second timeout, don't panic
  esp_task_wdt_add(NULL);  // Add current task to watchdog

  // Print the reset reason from the PREVIOUS run - useful for diagnosis
  esp_reset_reason_t rstReason = esp_reset_reason();
  const char* rstStr = "UNKNOWN";
  switch (rstReason) {
    case ESP_RST_POWERON:   rstStr = "POWER_ON"; break;
    case ESP_RST_SW:        rstStr = "SW_RESTART (esp_restart)"; break;
    case ESP_RST_PANIC:     rstStr = "PANIC (exception/abort)"; break;
    case ESP_RST_INT_WDT:   rstStr = "INT_WATCHDOG"; break;
    case ESP_RST_TASK_WDT:  rstStr = "TASK_WATCHDOG"; break;
    case ESP_RST_WDT:       rstStr = "OTHER_WATCHDOG"; break;
    case ESP_RST_DEEPSLEEP: rstStr = "DEEP_SLEEP"; break;
    case ESP_RST_BROWNOUT:  rstStr = "BROWNOUT"; break;
    default: break;
  }
  if (cfg.runMode == RunMode::DEBUG) {
    Serial.print("Last reset reason: ");
    Serial.print(rstStr);
    Serial.print(" (");
    Serial.print((int)rstReason);
    Serial.println(")");
  }

  debugPrint("🏁 Starting Racing Control Loop...");
  debugPrint("💡 For compass calibration, use the calibration program first!");
  debugPrint("   - Upload 'calibration' environment for setup");
  debugPrint("   - Upload 'control_loop' environment for racing");
  if (cfg.runMode == RunMode::DEBUG) {
    Serial.print("Run Mode: ");
    Serial.println(cfg.runMode == RunMode::DEBUG ? "DEBUG" : "REAL");
  }

  // Get chip ID for MQTT - use only lower 32 bits to avoid leading zeros
  uint64_t chipIdHex = ESP.getEfuseMac();
  snprintf(chipId, sizeof(chipId), "%x", (uint32_t)chipIdHex);
  if (cfg.runMode == RunMode::DEBUG) {
    Serial.print("Chip ID: ");
    Serial.println(chipId);
  }
  yield(); // Yield after chip ID generation

  // Initialize global variables (semaphores) before any FreeRTOS calls
  globalVar_init();
  yield(); // Yield after global var init

  // WiFi must be put in STA mode BEFORE setHostname() - in espressif32@3.5.0
  // tcpip_adapter_set_hostname() is called immediately and crashes if the
  // adapter isn't initialized yet (SW_CPU_RESET / NULL dereference).
  WiFi.mode(WIFI_STA);
  yield(); // Yield after mode set
  WiFi.disconnect(true);  // clear any stale credentials/state from NVS
  delay(100);
  yield(); // Yield after disconnect

  // Now safe to set hostname
  WiFi.setHostname(chipId);
  if (cfg.runMode == RunMode::DEBUG) {
    Serial.print("WiFi hostname set to: ");
    Serial.println(chipId);
  }
  yield(); // Yield after hostname set

  // Connect to Wi-Fi
  debugPrint("Connecting to WiFi...");
  Serial.flush();  // ensure output is flushed before WiFi radio starts
  yield(); // Yield before WiFi begin
  
  WiFi.begin(ssid, password);
  yield(); // Yield after WiFi begin
  
  int wifiTimeout = 0;
  const int maxWifiTimeout = 30; // 30 second timeout
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); // Shorter delay
    yield(); // Yield during WiFi connection attempts
    delay(500); // Split delay with yield
    if (cfg.runMode == RunMode::DEBUG) Serial.print(".");
    yield(); // Additional yield
    wifiTimeout++;
    
    if (wifiTimeout > maxWifiTimeout) {
      debugPrint("\nWiFi connection timeout! Restarting...");
      yield();
      ESP.restart();
    }
  }
  debugPrint("\nWiFi connected!");

  // Setup Arduino OTA
  debugPrint("Starting Arduino OTA...");
  char otaHostname[32];
  snprintf(otaHostname, sizeof(otaHostname), "truck-%s", chipId);
  ArduinoOTA.setHostname(otaHostname);
  ArduinoOTA.setPassword("truck123"); // Set OTA password
  
  ArduinoOTA.onStart([]() {
    if (cfg.runMode == RunMode::DEBUG) {
      Serial.print("Start updating ");
      Serial.println((ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem");
    }
  });
  
  ArduinoOTA.onEnd([]() {
    debugPrint("\nOTA Update Complete!");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (cfg.runMode == RunMode::DEBUG) Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    if (cfg.runMode == RunMode::DEBUG) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    }
  });
  
  ArduinoOTA.begin();
  if (cfg.runMode == RunMode::DEBUG) {
    Serial.print("Arduino OTA ready! Use hostname: ");
    Serial.println(otaHostname);
  }

  // Initialize sensors AFTER WiFi (with error checking and delays)
  debugPrint("Initializing sensors...");
  if (cfg.runMode == RunMode::DEBUG) Serial.printf("[MEMORY] Before sensor init: %d bytes\n", ESP.getFreeHeap());
  
  // Add delay before sensor init
  delay(1000);
  
  // Initialize accelerometer with error checking - no exceptions
  debugPrint("Initializing MPU6050...");
  yield();
  accelSensor.Begin();
  debugPrint("MPU6050 initialization completed");
  yield();
  
  delay(500);
  yield();
  
  // Initialize compass - no exceptions
  debugPrint("Initializing compass...");
  if (cfg.runMode == RunMode::DEBUG) Serial.printf("[MEMORY] Before compass init: %d bytes\n", ESP.getFreeHeap());
  
  compass.Begin();
  debugPrint("Compass initialization completed");
  yield();
  
  delay(500);
  if (cfg.runMode == RunMode::DEBUG) {
    Serial.printf("[MEMORY] After sensor init: %d bytes\n", ESP.getFreeHeap());
  }
  
  // Initialize ultrasonic sensors with delays and error handling
  debugPrint("Initializing ultrasonic sensors...");
  delay(100);
  
  // Initialize ultrasonic sensors without exceptions
  ultraSound.open(TRIGGER_PIN1, ECHO_PIN1, rawDistFront);
  delay(100);
  ultraSound.open(TRIGGER_PIN2, ECHO_PIN2, rawDistRight);
  delay(100);
  ultraSound.open(TRIGGER_PIN3, ECHO_PIN3, rawDistLeft);
  delay(100);
  ultraSound.open(TRIGGER_PIN4, ECHO_PIN4, rawDistBack);
  delay(100);
  
  if (cfg.runMode == RunMode::DEBUG) {
    Serial.printf("[MEMORY] After ultrasonic init: %d bytes\n", ESP.getFreeHeap());
  }

  // Initialize actuators - no exceptions
  debugPrint("Initializing actuators...");
  steer.Begin();
  delay(100);
  debugPrint("Actuators initialized successfully");
  
  if (cfg.runMode == RunMode::DEBUG) {
    Serial.printf("[MEMORY] After actuator init: %d bytes\n", ESP.getFreeHeap());
  }
  
  // motor object is ready to use

  // Initialize MQTT with timeout
  debugPrint("Initializing MQTT...");
  mqtt.init(chipId);
  
  // Test MQTT connection with timeout
  debugPrint("Testing MQTT connection...");
  int mqttRetries = 0;
  const int maxMqttRetries = 3;
  bool mqttSuccess = false;
  
  while (mqttRetries < maxMqttRetries && !mqttSuccess) {
    try {
      mqtt.send("status", "Control loop starting up");
      if (mqttConnected()) {
        debugPrint("MQTT connected successfully!");
        mqtt.subscribe("control"); // Subscribe to control commands
        mqttSuccess = true;
      } else {
        mqttRetries++;
        debugPrintf("MQTT attempt %d failed, retrying...\n", mqttRetries);
        delay(1000);
      }
    } catch (...) {
      mqttRetries++;
      debugPrintf("MQTT exception on attempt %d, retrying...\n", mqttRetries);
      delay(1000);
    }
  }
  
  if (!mqttSuccess) {
    debugPrint("WARNING: MQTT connection failed after retries, continuing anyway");
  }
  

  debugPrint("Setup complete!");
  g_mode = Mode::IDLE;
  g_cmd = {};
  
  debugPrintf("[MEMORY] Initial free heap: %d bytes\n", ESP.getFreeHeap());
  debugPrint("System ready - entering main loop");
}

void loop() {
  // STEP 2: GRADUAL SENSOR RESTORATION + MQTT
  static uint32_t counter = 0;
  const uint32_t nowMs = millis();
  
  // Check heap every 1000 iterations
  if (++counter % 1000 == 0) {
    size_t freeHeap = ESP.getFreeHeap();
    Serial.printf("[%u] Heap: %d bytes, millis: %u\n", counter, freeHeap, nowMs);
    
    // Emergency restart if heap is critically low
    if (freeHeap < 3000) {
      Serial.println("[EMERGENCY] Heap critically low, restarting...");
      delay(100);
      ESP.restart();
    }
  }
  
  // Basic OTA handling
  static uint32_t lastOTA = 0;
  if ((nowMs - lastOTA) > 100) {
    lastOTA = nowMs;
    ArduinoOTA.handle();
  }
  yield();
  
  // MQTT loop with enhanced safety
  static uint32_t lastMqttLoop = 0;
  if ((nowMs - lastMqttLoop) > 100) {  // 10Hz MQTT loop
    lastMqttLoop = nowMs;
    if (ESP.getFreeHeap() > 6000) {  // Only if enough memory
      mqttLoop();
    }
  }
  yield();
  
  // Full sensor read + explore mode control
  static uint32_t lastSensorRead = 0;
  if ((nowMs - lastSensorRead) > 50) {  // 20Hz sensors
    lastSensorRead = nowMs;

    if (ESP.getFreeHeap() > 9000) {
      try {
        g_raw.ul = readUltrasonicLeftCm();
        g_raw.ur = readUltrasonicRightCm();
        g_raw.uf = readUltrasonicFrontCm();
        g_raw.ub = readUltrasonicBackCm();
        g_raw.yawRate = readYawRateDegPerSec();
        g_raw.heading = readHeadingDeg();
        g_raw.magX = readMagneticX();
        g_raw.magY = readMagneticY();
        g_raw.magZ = readMagneticZ();
        g_raw.compass = g_raw.heading;
        g_raw.accX = readAccelerationX();
        g_raw.accY = readAccelerationY();
        g_raw.accZ = readAccelerationZ();

        filterSensors(g_raw, g_filt);
        deriveFeatures(g_filt, g_feat);

        if (counter % 3000 == 0) {
          Serial.printf("[SENSORS] UL:%.1f UR:%.1f UF:%.1f UB:%.1f\n", g_raw.ul, g_raw.ur, g_raw.uf, g_raw.ub);
        }
      } catch (...) {
        Serial.println("[ERROR] Sensor read failed");
      }
    }
    yield();
  }

  // Auto explore mode
  if (g_mode == Mode::IDLE && nowMs > 2000) {
    g_mode = Mode::EXPLORE;
  }

  updateStateMachine(g_feat, g_filt, nowMs);

  switch (g_mode) {
    case Mode::IDLE:
      g_cmd.motorPwm = 0;
      g_cmd.steer = SteerCmd::STRAIGHT;
      break;
    case Mode::EXPLORE:
      g_cmd = computeExploreCommand(g_feat, g_filt);
      break;
    case Mode::RECOVER:
      g_cmd = computeRecoverCommand(g_feat, g_filt, nowMs);
      break;
    case Mode::STOP:
      g_cmd.motorPwm = 0;
      g_cmd.steer = SteerCmd::STRAIGHT;
      break;
  }

  if (ESP.getFreeHeap() > 4000) {
    applyCommand(g_cmd);
  } else {
    motor.driving(0);
    steer.Straight();
  }
  
  // Publish telemetry at low rate
  static uint32_t lastTelemetry = 0;
  if ((nowMs - lastTelemetry) > 500) {  // 2Hz telemetry
    lastTelemetry = nowMs;
    if (ESP.getFreeHeap() > 8000) {
      // Use minimal telemetry frame
      TelemetryFrame telemetry;
      telemetry.seq = ++g_seq;
      telemetry.tMs = nowMs;
      telemetry.mode = g_mode;
      telemetry.raw = g_raw;
      telemetry.filt = g_filt;
      telemetry.feat = g_feat;
      telemetry.cmd = g_cmd;
      publishTelemetry(telemetry);
    }
  }
  yield();
  
  // Moderate loop speed for stability
  delay(80);  // ~12Hz loop for stability
  yield();
}