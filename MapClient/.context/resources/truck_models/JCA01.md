# Truck Model: JCA01

## Overview
Small modular toy truck with ESP32 control board. Scale approximately 20:1.

## Drivetrain
- Dual independent rear wheel motors (left and right driven separately).
- Front steering via servo.

## Sensors
| Sensor | Count | Notes |
|--------|-------|-------|
| Ultrasonic (HC-SR04 or equivalent) | 4 | Front, back, left, right. Max range ~199 cm. |
| MPU-6050 | 1 | 6-axis IMU: accelerometer + gyroscope. |
| Compass (HMC5883 or equivalent) | 1 | Heading in degrees. |

## Expansion
- I2C expansion board for additional IO (lights, etc.).
- I2C switch for additional I2C devices (LV0x / LV5x TOF laser sensors).

## Control Board
- ESP32 (dual-core, Wi-Fi + Bluetooth).
- Firmware: PlatformIO project in `EmbeddedSw/`.

## Communication
- MQTT over Wi-Fi.
- Telemetry topic: `{TRUCK_ID}/telemetry` at 1 Hz (configurable).

## Known Characteristics
- Sensor polling at 50 Hz (20 ms loop).
- EMA filter alpha: 0.35 (ultrasonics), 0.25 (IMU).
- Front stop threshold: 12 cm. Slow threshold: 20 cm.
- Side collision threshold: 6 cm.
- Recovery: reverse 500 ms, turn 600 ms.

## Notes
- Values above are verified from `EmbeddedSw/src/z_main_control_loop.cpp`.
- Update this file if hardware or firmware parameters change.
