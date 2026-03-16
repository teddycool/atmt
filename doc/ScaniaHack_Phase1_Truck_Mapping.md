
# Autonomous Truck Track Mapping — Phase 1
### ScaniaHack 2026 Project Notes

## Goal of Phase 1

The goal of Phase‑1 is **not fast driving** and **not AI yet**.

The goal is:

> Make the truck autonomously explore the track slowly and stream clean data so the backend can build a first track model.

Success criteria:

- Truck completes laps autonomously
- Data is streamed via MQTT
- Backend logs and visualizes runs
- Backend builds a first **track representation**


# System Architecture (Phase 1)

```
+-------------------------------------------------------------+
|                        TRUCK (ESP32)                        |
|                                                             |
|  Sensors                                                    |
|  -------                                                    |
|  UL  UR  UF  UB  (ultrasonic sensors)                       |
|  IMU (gyro/acc)                                             |
|  Compass                                                    |
|                                                             |
|        ↓                                                     |
|  Sensor Filtering                                           |
|        ↓                                                     |
|  Feature Extraction                                         |
|    width = UL + UR                                          |
|    center_error = UR - UL                                   |
|    front_blocked                                            |
|                                                             |
|        ↓                                                     |
|  Exploration Controller                                     |
|  (wall following + obstacle avoidance)                      |
|                                                             |
|        ↓                                                     |
|  Motor + Steering                                           |
|                                                             |
|        ↓                                                     |
|  MQTT Telemetry (10Hz)                                      |
|                                                             |
+---------------------- MQTT Broker --------------------------+
                               |
                               |
                               v
+-------------------------------------------------------------+
|                    BACKEND (Pi5 / Jetson)                   |
|                                                             |
|  MQTT Logger                                                |
|  - Store all messages                                       |
|                                                             |
|        ↓                                                     |
|  Run Database                                               |
|                                                             |
|        ↓                                                     |
|  Data Processor                                             |
|  - smooth signals                                           |
|  - detect turns / straights                                 |
|  - calculate track width                                    |
|                                                             |
|        ↓                                                     |
|  Lap Visualizer                                             |
|  - plots                                                    |
|  - debugging                                                |
|                                                             |
|        ↓                                                     |
|  Track Map Builder (later phases)                           |
|                                                             |
+-------------------------------------------------------------+
```


# Truck Software Strategy (C++)

The truck should **not try to build the map itself**.

Its job is:

- move safely
- collect reliable data
- repeat behavior consistently

## Core idea

A **simple exploration robot**.

Controller behavior:

1. Move slowly forward
2. Stay centered between track sides
3. Avoid collisions
4. Recover if stuck


# Truck Software Architecture

Recommended module structure:

```
main.cpp
sensors.cpp
filters.cpp
features.cpp
control.cpp
state_machine.cpp
telemetry.cpp
```


# Truck State Machine

```
IDLE
  |
  v
EXPLORE  --->  RECOVER
  |
  v
STOP
```

### IDLE
Truck waits for start command.

### EXPLORE
Normal exploration mode.

### RECOVER
Triggered when the truck gets stuck.

### STOP
Emergency stop.


# Derived Features on Truck

Raw sensors alone are not enough.

Compute useful values:

```
width = UL + UR
center_error = UR - UL
front_blocked = UF < threshold
rear_blocked = UB < threshold
```

These help both the controller and the backend.


# Exploration Controller Logic

Pseudo rules:

```
if front_distance < stop_threshold
    stop
    turn away from closest wall

else
    if center_error > deadband
        steer right
    else if center_error < -deadband
        steer left
    else
        go straight
```

Speed control:

```
normal_speed
slow_down_if_front_near
```


# Recovery Behavior

Trigger when:

```
front blocked for > 700 ms
```

Recovery steps:

1. Stop
2. Reverse briefly
3. Turn away from closest wall
4. Resume exploration


# Minimal C++ Exploration Controller Skeleton

```cpp
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

struct Sensors {
  float ul;
  float ur;
  float uf;
  float ub;
  float yaw;
  float heading;
};

struct Features {
  float width;
  float centerError;
  bool frontBlocked;
};

struct Command {
  int motorPwm;
  SteerCmd steer;
};

Mode mode = Mode::IDLE;

void deriveFeatures(const Sensors& s, Features& f) {
  f.width = s.ul + s.ur;
  f.centerError = s.ur - s.ul;
  f.frontBlocked = s.uf < 12.0;
}

Command computeExplore(const Sensors& s, const Features& f) {

  Command cmd;

  if (f.frontBlocked) {
    cmd.motorPwm = 0;

    if (s.ul < s.ur)
      cmd.steer = SteerCmd::RIGHT;
    else
      cmd.steer = SteerCmd::LEFT;

    return cmd;
  }

  cmd.motorPwm = 70;

  if (f.centerError > 3)
    cmd.steer = SteerCmd::RIGHT;
  else if (f.centerError < -3)
    cmd.steer = SteerCmd::LEFT;
  else
    cmd.steer = SteerCmd::STRAIGHT;

  return cmd;
}
```


# Telemetry Format (MQTT)

Recommended JSON payload:

```json
{
 "truck_id": "truck1",
 "seq": 1823,
 "t_ms": 123456789,
 "mode": "EXPLORE",

 "ul": 24.1,
 "ur": 31.7,
 "uf": 18.5,
 "ub": 40.0,

 "yaw_rate": -12.4,
 "heading": 97.2,

 "width": 55.8,
 "center_error": 7.6,

 "motor_pwm": 68,
 "steer_cmd": "LEFT"
}
```

Send at:

```
10 Hz
```


# Backend Strategy (Python)

Backend responsibilities:

1. Record runs
2. Replay data
3. Detect laps
4. Build first track model

Recommended tools:

```
paho-mqtt
pandas
sqlite / duckdb
matplotlib
```


# Backend Processing Pipeline

```
MQTT
 ↓
Logger
 ↓
Database
 ↓
Signal smoothing
 ↓
Feature extraction
 ↓
Lap detection
 ↓
Segment detection
 ↓
Track model
```


# First Track Model (Simple)

Do **not try to build a perfect map first**.

Start with a **topological map**.

Example:

```
straight
left turn
straight narrow
right turn
chicane
straight wide
```

Each segment stores:

```
average width
average yaw
duration
recommended speed
```


# Backend Track Model Example

```python
track_map = {
    "segments": [
        {"id":0, "type":"straight", "width":46.2, "speed":70},
        {"id":1, "type":"left_turn", "width":35.0, "speed":45},
        {"id":2, "type":"chicane", "width":33.1, "speed":40}
    ]
}
```


# Phase‑1 Milestone

### Truck

- exploration controller works
- recovery works
- stable telemetry
- one autonomous lap possible

### Backend

- logs runs
- detects laps
- segments turns
- produces first map


# Next Phase (Phase 2)

Once Phase‑1 works:

- align multiple laps
- average track segments
- compute optimal speed
- send map back to truck
- start learning lap optimization
