// Minimal exploration controller skeleton for an ESP32-based truck
// Purpose:
// - Read filtered sensor values
// - Run a simple state machine: IDLE / EXPLORE / RECOVER / STOP
// - Apply basic wall-centering + obstacle avoidance
// - Publish telemetry periodically
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
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

String chipId;
long int age;

// -----------------------------
// Configuration
// -----------------------------
struct ControlConfig {
  // Distances in cm
  float frontStopDist = 12.0f;
  float frontSlowDist = 20.0f;
  float sideMinDist   = 6.0f;

  // Corridor-centering
  float centerDeadband = 3.0f;     // |UR - UL| below this => go straight
  uint8_t steerHoldSamples = 3;    // hysteresis before changing steering

  // PWM
  int pwmExplore = 70;
  int pwmSlow    = 45;
  int pwmReverse = -50;

  // Timing
  uint32_t telemetryPeriodMs = 1000;  // 1 Hz (memory-safe for long runtime)
  uint32_t recoverReverseMs  = 500;
  uint32_t recoverTurnMs     = 600;
  uint32_t stuckFrontMs      = 700;

  // Filtering
  float usAlpha = 0.35f;          // exponential smoothing factor
  float imuAlpha = 0.25f;
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
  // long heading = globalVar_get(calcHeading, &age);
  // return heading / 10.0f;
  return 0.0f;  // Return 0 when compass is disabled
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

float readCompassHeading() {
  // Calculate compass heading from magnetic field X and Y
  float magX = readMagneticX();
  float magY = readMagneticY();
  
  if (isnan(magX) || isnan(magY)) {
    return NAN;
  }
  
  float heading = atan2(magY, magX) * 180.0 / PI;
  if (heading < 0) {
    heading += 360.0;
  }
  return heading;
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
  mqtt.loop();
}

void publishTelemetry(const TelemetryFrame& t) {
  // Publish telemetry as JSON on MQTT
  if (!mqttConnected()) {
    Serial.println("MQTT not connected, skipping telemetry");
    return;
  }
  
  // Use char buffer instead of String concatenation to prevent heap fragmentation
  char jsonStr[512];  // Fixed size buffer
  snprintf(jsonStr, sizeof(jsonStr), 
    "{"
    "\"truck_id\":\"%s\","
    "\"seq\":%d,"
    "\"t_ms\":%lu,"
    "\"mode\":\"%s\","
    "\"ul\":%.1f,"
    "\"ur\":%.1f,"
    "\"uf\":%.1f,"
    "\"ub\":%.1f,"
    "\"yaw_rate\":%.2f,"
    "\"heading\":%.1f,"
    "\"compass\":%.1f,"
    "\"mag_x\":%.1f,"
    "\"mag_y\":%.1f,"
    "\"mag_z\":%.1f,"
    "\"acc_x\":%.1f,"
    "\"acc_y\":%.1f,"
    "\"acc_z\":%.1f,"
    "\"width\":%.1f,"
    "\"center_error\":%.1f,"
    "\"front_blocked\":%s,"
    "\"cmd_pwm\":%d,"
    "\"cmd_steer\":\"%s\""
    "}",
    chipId.c_str(), t.seq, t.tMs, modeToString(t.mode),
    t.filt.ul, t.filt.ur, t.filt.uf, t.filt.ub, t.filt.yawRate,
    t.filt.heading, t.filt.compass, t.filt.magX, t.filt.magY, t.filt.magZ,
    t.filt.accX, t.filt.accY, t.filt.accZ, t.feat.width, t.feat.centerError,
    t.feat.frontBlocked ? "true" : "false", t.cmd.motorPwm, steerToString(t.cmd.steer)
  );
  
  // Publish to truck-specific topic (chipId is auto-prepended by mqtt.send)
  mqtt.send("telemetry", String(jsonStr));
  
  // Print memory usage periodically
  static uint32_t lastMemCheck = 0;
  if ((millis() - lastMemCheck) > 5000) {
    lastMemCheck = millis();
    Serial.printf("[MEMORY] Free heap: %d bytes\n", ESP.getFreeHeap());
  }
}

// -----------------------------
// Sensor pipeline
// -----------------------------
void readSensors(RawSensors& raw) {
  raw.ul = readUltrasonicLeftCm();
  raw.ur = readUltrasonicRightCm();
  raw.uf = readUltrasonicFrontCm();
  raw.ub = readUltrasonicBackCm();
  raw.yawRate = readYawRateDegPerSec();
  raw.heading = readHeadingDeg();
  
  // Read compass values
  raw.magX = readMagneticX();
  raw.magY = readMagneticY();
  raw.magZ = readMagneticZ();
  raw.compass = readCompassHeading();
  
  // Read accelerometer values
  raw.accX = readAccelerationX();
  raw.accY = readAccelerationY();
  raw.accZ = readAccelerationZ();
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
  filt.compass = ema(filt.compass, raw.compass, cfg.imuAlpha);
  
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
  Serial.begin(115200);
  Serial.println("Starting Control Loop...");

  // Get chip ID for MQTT
  uint64_t chipIdHex = ESP.getEfuseMac();
  chipId = String((uint32_t)(chipIdHex >> 32), HEX) + String((uint32_t)chipIdHex, HEX);
  Serial.println("Chip ID: " + chipId);

  // Set WiFi hostname to chipId
  WiFi.setHostname(chipId.c_str());
  Serial.println("WiFi hostname set to: " + chipId);

  // Initialize global variables
  globalVar_init();

  // Connect to Wi-Fi first (with timeout and error handling)
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  int wifiTimeout = 0;
  const int maxWifiTimeout = 30; // 30 second timeout
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    wifiTimeout++;
    
    if (wifiTimeout > maxWifiTimeout) {
      Serial.println("\nWiFi connection timeout! Restarting...");
      ESP.restart();
    }
  }
  Serial.println("\nWiFi connected!");

  // Setup Arduino OTA
  Serial.println("Starting Arduino OTA...");
  ArduinoOTA.setHostname(("truck-" + chipId).c_str());
  ArduinoOTA.setPassword("truck123"); // Set OTA password
  
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update Complete!");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
  Serial.println("Arduino OTA ready! Use hostname: truck-" + chipId);

  // Initialize sensors AFTER WiFi (with error checking and delays)
  Serial.println("Initializing sensors...");
  Serial.printf("[MEMORY] Before sensor init: %d bytes\n", ESP.getFreeHeap());
  
  // Add delay before sensor init
  delay(1000);
  
  // Initialize accelerometer with error checking
  Serial.println("Initializing MPU6050...");
  try {
    accelSensor.Begin();
    Serial.println("MPU6050 initialization completed");
  } catch (...) {
    Serial.println("MPU6050 initialization failed, continuing...");
  }
  
  delay(500); // Delay between sensor initializations
  
  // Skip compass initialization (was causing stack overflow)
  Serial.println("Compass disabled to prevent crashes");
  
  delay(500);
  Serial.printf("[MEMORY] After accel init: %d bytes\n", ESP.getFreeHeap());
  
  // Initialize ultrasonic sensors with delays and error handling
  Serial.println("Initializing ultrasonic sensors...");
  delay(100);
  
  try {
    ultraSound.open(TRIGGER_PIN1, ECHO_PIN1, rawDistFront);
    delay(100);
    ultraSound.open(TRIGGER_PIN2, ECHO_PIN2, rawDistRight);
    delay(100);
    ultraSound.open(TRIGGER_PIN3, ECHO_PIN3, rawDistLeft);
    delay(100);
    ultraSound.open(TRIGGER_PIN4, ECHO_PIN4, rawDistBack);
    delay(100);
    Serial.println("++++");
  } catch (...) {
    Serial.println("Ultrasonic sensor initialization error, continuing...");
  }
  
  Serial.printf("[MEMORY] After ultrasonic init: %d bytes\n", ESP.getFreeHeap());

  // Initialize actuators with error handling
  Serial.println("Initializing actuators...");
  try {
    steer.Begin();
    delay(100);
    Serial.println("Actuators initialized successfully");
  } catch (...) {
    Serial.println("Actuator initialization error, continuing...");
  }
  Serial.printf("[MEMORY] After actuator init: %d bytes\n", ESP.getFreeHeap());
  
  // motor object is ready to use

  // Initialize MQTT with timeout
  Serial.println("Initializing MQTT...");
  mqtt.init(chipId);
  
  // Test MQTT connection with timeout
  Serial.println("Testing MQTT connection...");
  
  int mqttRetries = 0;
  const int maxMqttRetries = 3;
  bool mqttSuccess = false;
  
  while (mqttRetries < maxMqttRetries && !mqttSuccess) {
    try {
      mqtt.send("status", "Control loop starting up");
      if (mqttConnected()) {
        Serial.println("MQTT connected successfully!");
        mqtt.subscribe("control"); // Subscribe to control commands
        mqttSuccess = true;
      } else {
        mqttRetries++;
        Serial.printf("MQTT attempt %d failed, retrying...\n", mqttRetries);
        delay(1000);
      }
    } catch (...) {
      mqttRetries++;
      Serial.printf("MQTT exception on attempt %d, retrying...\n", mqttRetries);
      delay(1000);
    }
  }
  
  if (!mqttSuccess) {
    Serial.println("WARNING: MQTT connection failed after retries, continuing anyway");
  }

  Serial.println("Setup complete!");
  g_mode = Mode::IDLE;
  g_cmd = {};
  
  Serial.printf("[MEMORY] Initial free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("System ready - entering main loop");
}

void loop() {
  const uint32_t nowMs = millis();

  // Handle OTA updates
  ArduinoOTA.handle();

  mqttLoop();
  
  // Periodic MQTT status check (every 10 seconds)
  if ((nowMs - g_lastMqttStatusMs) > 10000) {
    g_lastMqttStatusMs = nowMs;
    if (!mqttConnected()) {
      Serial.println("[WARNING] MQTT disconnected, attempting reconnect...");
    } else {
      Serial.println("[INFO] MQTT connected OK");
    }
  }

  // 1. Read + filter + derive with safety checks
  readSensors(g_raw);
  filterSensors(g_raw, g_filt);
  deriveFeatures(g_filt, g_feat);

  // 2. Example: start exploring automatically after boot
  if (g_mode == Mode::IDLE && nowMs > 2000) {
    g_mode = Mode::EXPLORE;
  }

  // 3. Update state machine
  updateStateMachine(g_feat, g_filt, nowMs);

  // 4. Compute command by mode
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

  // 5. Apply outputs
  applyCommand(g_cmd);

  // 6. Publish telemetry
  maybePublishTelemetry(nowMs);

  // Small loop delay if needed
  delay(20);  // ~50 Hz control loop
}