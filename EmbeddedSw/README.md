# Autonomous Truck Control System

A real-time ESP32-based autonomous truck control system with sensor fusion, state machine control, and telemetry integration for warehouse/industrial navigation.

## 🚛 System Overview

This system implements an autonomous truck capable of:
- **Autonomous Navigation**: Wall-following and corridor-centering using ultrasonic sensors
- **Obstacle Avoidance**: Real-time collision detection and recovery maneuvers
- **State Management**: Robust state machine with IDLE/EXPLORE/RECOVER/STOP modes
- **Real-time Telemetry**: 1Hz MQTT telemetry with sensor data and control state
- **Remote Monitoring**: Web dashboard for live visualization and control

## 🏗️ Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   ESP32 Truck   │◄──►│  MQTT Broker     │◄──►│ Flask Dashboard │
│                 │    │ (192.168.2.2)    │    │ (WebSocket)     │
│ ┌─────────────┐ │    │                  │    │                 │
│ │Control Loop │ │    │ Topics:          │    │ - Live Sensors  │
│ │   1Hz       │ │    │ {chipId}/sensors │    │ - State Viz     │
│ │             │ │    │ {chipId}/control │    │ - Controls      │
│ └─────────────┘ │    │ {chipId}/config  │    │                 │
│                 │    └──────────────────┘    └─────────────────┘
│ Sensors:        │
│ - 4x Ultrasonic │
│ - MPU6050       │
│ - Magnetometer  │
│                 │
│ Actuators:      │
│ - Motor Control │
│ - Steering      │
└─────────────────┘
```

## 📁 Project Structure

```
EmbeddedSw/
├── src/
│   ├── z_main_control_loop.cpp     # Main control loop
│   ├── z_main_wifi_test.cpp        # Network connectivity test
│   ├── z_main_minimal_test.cpp     # Hardware recovery utility
│   ├── config.cpp                  # Hardware configuration
│   ├── secrets.cpp                 # WiFi credentials
│   ├── actuators/
│   │   ├── motor.cpp               # Motor PWM control
│   │   ├── steer.cpp               # Steering control
│   │   ├── neopixel.cpp            # LED indicators (optional)
│   │   └── light.cpp               # Light control (optional)
│   ├── sensors/
│   │   ├── accsensor.cpp           # MPU6050 accelerometer/gyro
│   │   ├── compass.cpp             # Magnetometer with Kalman filtering
│   │   ├── usensor.cpp             # Ultrasonic sensor array
│   │   └── kalman_filter.cpp       # Kalman filter implementation
│   ├── telemetry/
│   │   ├── mqtt.cpp                # MQTT communication
│   │   └── logger.cpp              # HTTP logging (optional)
│   └── variables/
│       └── setget.cpp              # Thread-safe global variables
├── include/                        # Header files
├── platformio.ini                  # Build configuration
└── truck_telemetry_dashboard/      # Python Flask dashboard
    ├── app.py                      # WebSocket server
    ├── templates/index.html        # Web interface
    └── requirements.txt            # Python dependencies
```

## 🎛️ Control Loop Architecture

### State Machine

The control loop operates as a finite state machine with four primary states:

```cpp
enum class Mode {
    IDLE,       // Stationary, awaiting commands
    EXPLORE,    // Active navigation with wall-following
    RECOVER,    // Obstacle avoidance maneuvers
    STOP        // Emergency stop condition
};
```

### Sensor Fusion Pipeline

#### 1. Ultrasonic Sensor Array
```cpp
struct UltrasonicReadings {
    float ul;  // Upper left  (45°)
    float ur;  // Upper right (45°) 
    float sl;  // Side left   (90°)
    float sr;  // Side right  (90°)
};
```

#### 2. IMU Processing
- **MPU6050**: Raw accelerometer (±2g) and gyroscope (±250°/s)
- **EMA Filtering**: Configurable smoothing (α = 0.1 default)
- **Compass**: HMC5883L with Kalman filtering for magnetic interference

#### 3. Feature Extraction
```cpp
struct Features {
    float width;        // Corridor width (ul + ur)
    float centerError;  // Distance from corridor center
    float frontDist;    // Minimum forward obstacle distance
    float sideDist;     // Minimum side clearance
};
```

### Control Algorithm

#### Wall-Following Logic
```cpp
// Corridor centering with configurable gains
float steerGain = 2.0f;       // Proportional steering response
float centerTarget = 0.0f;    // Desired center position
float steerCmd = steerGain * (centerTarget - centerError);
```

#### State Transitions
- **IDLE → EXPLORE**: Manual command or timer trigger
- **EXPLORE → RECOVER**: Front obstacle detected (`frontDist < stopDist`)
- **RECOVER → EXPLORE**: Clear path restored
- **ANY → STOP**: Emergency condition or manual override

#### Safety Parameters
```cpp
float stopDist = 15.0f;      // Emergency stop distance (cm)
float sideMinDist = 6.0f;    // Minimum side clearance (cm)
float maxSpeed = 150;        // Maximum motor PWM (0-255)
```

## 📡 Communication Protocol

### MQTT Topics

All topics are prefixed with unique `chipId` (ESP32 MAC-derived):

#### Telemetry (Outbound - 1Hz)
```
{chipId}/sensors
```

**Payload Structure:**
```json
{
  "truck_id": "abc123def456",
  "seq": 1234,
  "timestamp": 1734567890,
  "mode": "EXPLORE",
  "sensors": {
    "ul": 25.4, "ur": 23.8, "sl": 12.3, "sr": 15.7,
    "accX": 0.12, "accY": -0.05, "accZ": 9.81,
    "gyroZ": 2.3, "compass": 185.7
  },
  "features": {
    "width": 49.2,
    "centerError": -1.4,
    "frontDist": 45.6,
    "sideDist": 12.3
  },
  "control": {
    "motorPwm": 120,
    "steerCmd": "RIGHT",
    "steerPwm": 85
  }
}
```

#### Control Commands (Inbound)
```
{chipId}/control
```

**Supported Commands:**
```json
{"command": "start"}        // IDLE → EXPLORE
{"command": "stop"}         // ANY → STOP  
{"command": "emergency"}    // Immediate halt
```

#### Configuration (Inbound)
```
{chipId}/config
```

**Runtime Parameters:**
```json
{
  "stopDist": 15.0,
  "steerGain": 2.0,
  "maxSpeed": 150,
  "emaAlpha": 0.1
}
```

## 🛠️ Development Setup

### Prerequisites
- **PlatformIO** (VS Code extension recommended)
- **Python 3.8+** (for dashboard)
- **ESP32 Development Board**
- **MQTT Broker** (Mosquitto recommended)

### Hardware Configuration

#### Required Sensors
- **4x HC-SR04** ultrasonic sensors (trigger/echo pairs)
- **MPU6050** accelerometer/gyroscope (I2C)
- **HMC5883L** magnetometer (I2C)

#### Pin Assignments (see `include/atmio.h`)
```cpp
// Ultrasonic sensors
#define TRIG_PIN_UL  2   // Upper left trigger
#define ECHO_PIN_UL  4   // Upper left echo
// ... additional sensor pins

// Motor control  
#define MOTOR_PWM_PIN    5
#define MOTOR_DIR_PIN    18

// Steering
#define STEER_PWM_PIN    19
#define STEER_DIR_PIN    21
```

### Building and Flashing

1. **Configure WiFi credentials** in `src/secrets.cpp`
2. **Build control loop**:
   ```bash
   pio run -e control_loop
   ```
3. **Flash firmware**:
   ```bash
   pio run -e control_loop --target upload
   ```
4. **Monitor serial output**:
   ```bash
   pio device monitor -e control_loop
   ```

### Dashboard Setup

1. **Install Python dependencies**:
   ```bash
   cd truck_telemetry_dashboard
   pip install -r requirements.txt
   ```

2. **Start dashboard**:
   ```bash
   ./start_dashboard.sh
   ```

3. **Access interface**: http://localhost:5000

## 🧪 Testing Utilities

### WiFi Connectivity Test
```bash
pio run -e wifi_test --target upload
```
Validates network connection and MQTT broker connectivity.

### Minimal Hardware Test  
```bash
pio run -e minimal_test --target upload
```
Basic GPIO/sensor validation - useful for hardware debugging.

### Recovery Mode
If ESP32 becomes unresponsive, flash `minimal_test` to restore basic functionality.

## 🔧 Configuration

### Compile-time Configuration (`include/config.h`)
```cpp
motorType = SINGLE;          // Motor configuration
steerType = MOTOR;           // Steering type (MOTOR/SERVO)
steer_servo_min = 60;        // Minimum steering PWM
steer_servo_max = 105;       // Maximum steering PWM
```

### Runtime Parameters (via MQTT)
- **Safety distances**: `stopDist`, `sideMinDist`
- **Control gains**: `steerGain`, `maxSpeed`
- **Filter coefficients**: `emaAlpha`

## 🚀 Adding New Features

### New Sensor Integration
1. **Add sensor class** in `src/sensors/`
2. **Define header** in `include/sensors/`
3. **Update telemetry structure** in `z_main_control_loop.cpp`
4. **Add to build filter** in `platformio.ini`

### Custom Control Modes
1. **Extend Mode enum** in control loop
2. **Implement state logic** in main switch statement
3. **Add transition conditions**
4. **Update telemetry reporting**

### MQTT Command Extensions
1. **Parse new commands** in `mqttCallback()`
2. **Implement command handlers**
3. **Update dashboard controls**
4. **Document protocol changes**

## ⚡ Performance Characteristics

- **Control Loop Frequency**: 1Hz (memory-optimized for stability)
- **Sensor Reading Rate**: ~10Hz internal, 1Hz telemetry
- **MQTT Latency**: <50ms typical
- **Memory Usage**: 
  - RAM: ~45KB (13.7% of 328KB)
  - Flash: ~790KB (60% of 1.3MB)

## 🛡️ Safety Features

- **Watchdog timer**: Auto-restart on system hang
- **Emergency stop**: Immediate motor shutoff via MQTT
- **Collision detection**: Multi-sensor obstacle avoidance
- **Recovery sequences**: Automated backup/turn maneuvers
- **Telemetry validation**: Sensor health monitoring

## 📊 Monitoring and Debugging

### Serial Output
Real-time debug information at 115200 baud:
- Sensor readings and filtered values
- State transitions and control decisions
- MQTT connection status and message handling
- Performance metrics and timing

### Dashboard Visualization
- Live sensor data with time-series plots
- State machine visualization
- Manual control interface
- System health indicators

---

## 📝 License

This project is developed for research and educational purposes. See project documentation for specific licensing terms.

## 🤝 Contributing

1. **Fork** the repository
2. **Create feature branch** (`git checkout -b feature/new-capability`)
3. **Test thoroughly** with hardware-in-the-loop
4. **Submit pull request** with comprehensive description

For questions or support, consult the project documentation or create an issue in the repository.