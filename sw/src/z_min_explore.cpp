// Exploration controller for an ESP32-based autonomous truck
// Purpose:
// - Read filtered sensor values from real hardware
// - Run a simple state machine: IDLE / EXPLORE / RECOVER / STOP
// - Apply basic wall-centering + obstacle avoidance
// - Publish telemetry via MQTT
// Author: Updated for real sensor integration
// Date: 2026-03-06

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <secrets.h>
#include <config.h>
#include <telemetry/mqtt.h>
#include <variables/setget.h>
#include <sensors/compass.h>
#include <sensors/usensor.h>
#include <actuators/motor.h>
#include <actuators/steer.h>

// Ultrasonic sensor pins (same as z_main_streamer.cpp)
#define TRIGGER_PIN 16
#define ECHO_PIN 34
#define TRIGGER_PIN2 17
#define ECHO_PIN2 35
#define TRIGGER_PIN3 26
#define ECHO_PIN3 25
#define TRIGGER_PIN4 19
#define ECHO_PIN4 18

// Forward declarations
float readHeadingDeg();

// -----------------------------
// Configuration
// -----------------------------
struct ExploreConfig {
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
  uint32_t recoverReverseMs = 1000;
  uint32_t recoverTurnMs = 1500;
  uint32_t stuckFrontMs = 3000;
  uint32_t telemetryPeriodMs = 1000;

  // Filtering
  float usAlpha = 0.3f;    // EMA alpha for ultrasonic sensors
  float imuAlpha = 0.1f;   // EMA alpha for IMU data
};

ExploreConfig cfg;

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
};

struct FilteredSensors {
  float ul = NAN;
  float ur = NAN;
  float uf = NAN;
  float ub = NAN;
  float yawRate = NAN;
  float heading = NAN;
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
uint32_t g_frontBlockedSinceMs = 0;
uint32_t g_recoverStartMs = 0;

SteerCmd g_lastRequestedSteer = SteerCmd::STRAIGHT;
SteerCmd g_appliedSteer = SteerCmd::STRAIGHT;
uint8_t g_sameSteerCount = 0;

// Hardware instances
String chipid;
Mqtt mqtt;
Compass compass;
Usensor ultraSound;
Motor motor;
Steer steer;
long int age;

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
// Real hardware sensor functions
// -----------------------------
float readUltrasonicLeftCm() {
  return globalVar_get(rawDistLeft, &age);
}

float readUltrasonicRightCm() {
  return globalVar_get(rawDistRight, &age);
}

float readUltrasonicFrontCm() {
  return globalVar_get(rawDistFront, &age);
}

float readUltrasonicBackCm() {
  return globalVar_get(rawDistBack, &age);
}

float readYawRateDegPerSec() {
  // Calculate yaw rate from compass readings
  static float lastHeading = 0;
  static uint32_t lastTime = 0;
  float currentHeading = readHeadingDeg();
  uint32_t currentTime = millis();
  
  if (lastTime == 0) {
    lastTime = currentTime;
    lastHeading = currentHeading;
    return 0.0f;
  }
  
  float dt = (currentTime - lastTime) / 1000.0f;
  if (dt <= 0) return 0.0f;
  
  float deltaHeading = currentHeading - lastHeading;
  // Handle wraparound
  if (deltaHeading > 180) deltaHeading -= 360;
  if (deltaHeading < -180) deltaHeading += 360;
  
  lastTime = currentTime;
  lastHeading = currentHeading;
  
  return deltaHeading / dt;
}

float readHeadingDeg() {
  // Use computed heading from compass
  float heading = globalVar_get(calcHeading, &age) / 10.0f; // calcHeading is in 1/10 degrees
  return heading;
}

void setMotorPwm(int pwm) {
  // Map signed PWM (-100 to +100) to motor driver
  motor.driving(pwm);
}

void setSteering(SteerCmd steerCmd) {
  // Control steering hardware
  switch (steerCmd) {
    case SteerCmd::LEFT:
      steer.Left();
      break;
    case SteerCmd::RIGHT:
      steer.Right();
      break;
    case SteerCmd::STRAIGHT:
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
  // Publish comprehensive telemetry as JSON via MQTT
  StaticJsonDocument<512> jsonDoc;
  
  jsonDoc["truck_id"] = chipid;
  jsonDoc["seq"] = t.seq;
  jsonDoc["t_ms"] = t.tMs;
  jsonDoc["mode"] = modeToString(t.mode);
  
  // Raw sensor data
  jsonDoc["raw"]["ul"] = t.raw.ul;
  jsonDoc["raw"]["ur"] = t.raw.ur;
  jsonDoc["raw"]["uf"] = t.raw.uf;
  jsonDoc["raw"]["ub"] = t.raw.ub;
  jsonDoc["raw"]["yaw_rate"] = t.raw.yawRate;
  jsonDoc["raw"]["heading"] = t.raw.heading;
  
  // Filtered sensor data
  jsonDoc["filt"]["ul"] = t.filt.ul;
  jsonDoc["filt"]["ur"] = t.filt.ur;
  jsonDoc["filt"]["uf"] = t.filt.uf;
  jsonDoc["filt"]["ub"] = t.filt.ub;
  jsonDoc["filt"]["yaw_rate"] = t.filt.yawRate;
  jsonDoc["filt"]["heading"] = t.filt.heading;
  
  // Derived features
  jsonDoc["feat"]["width"] = t.feat.width;
  jsonDoc["feat"]["center_error"] = t.feat.centerError;
  jsonDoc["feat"]["front_blocked"] = t.feat.frontBlocked;
  jsonDoc["feat"]["rear_blocked"] = t.feat.rearBlocked;
  jsonDoc["feat"]["narrow"] = t.feat.narrow;
  
  // Commands
  jsonDoc["cmd"]["motor_pwm"] = t.cmd.motorPwm;
  jsonDoc["cmd"]["steer"] = steerToString(t.cmd.steer);
  
  // Magnetic compass data
  jsonDoc["mag"]["x"] = globalVar_get(rawMagX);
  jsonDoc["mag"]["y"] = globalVar_get(rawMagY);
  jsonDoc["mag"]["z"] = globalVar_get(rawMagZ);
  
  String jsonString;
  serializeJson(jsonDoc, jsonString);
  
  // Output to both Serial and MQTT
  Serial.println(jsonString);
  
  if (mqttConnected()) {
    mqtt.send("explore", jsonString);
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
}

void filterSensors(const RawSensors& raw, FilteredSensors& filt) {
  filt.ul = ema(filt.ul, raw.ul, cfg.usAlpha);
  filt.ur = ema(filt.ur, raw.ur, cfg.usAlpha);
  filt.uf = ema(filt.uf, raw.uf, cfg.usAlpha);
  filt.ub = ema(filt.ub, raw.ub, cfg.usAlpha);

  filt.yawRate = ema(filt.yawRate, raw.yawRate, cfg.imuAlpha);
  filt.heading = ema(filt.heading, raw.heading, cfg.imuAlpha);
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
  Serial.println("Starting Exploration Controller...");
  
  // Get chip ID for identification
  uint64_t chipIdHex = ESP.getEfuseMac();
  chipid = String((uint32_t)(chipIdHex >> 32), HEX) + String((uint32_t)chipIdHex, HEX);
  Serial.print("Chip ID: ");
  Serial.println(chipid);
  
  // Initialize global variables system
  globalVar_init();
  
  // Initialize compass sensor
  compass.Begin();
  vTaskDelay(pdMS_TO_TICKS(100));
  
  // Initialize ultrasonic sensors
  ultraSound.open(TRIGGER_PIN, ECHO_PIN, rawDistFront);
  ultraSound.open(TRIGGER_PIN2, ECHO_PIN2, rawDistRight);
  ultraSound.open(TRIGGER_PIN3, ECHO_PIN3, rawDistLeft);
  ultraSound.open(TRIGGER_PIN4, ECHO_PIN4, rawDistBack);
  
  // Initialize motor and steering
  steer.Begin();
  
  // Connect to WiFi and initialize MQTT
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  
  mqtt.init(chipid);
  mqtt.send("explore", "exploration controller online");
  
  // Initialize state
  g_mode = Mode::IDLE;
  g_cmd = {};
  
  Serial.println("Exploration Controller initialized. Starting in IDLE mode.");
}

void loop() {
  const uint32_t nowMs = millis();

  mqttLoop();

  // 1. Read + filter + derive
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