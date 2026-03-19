# Architecture

## Overview
Python tooling for simulating and visualising autonomous toy truck navigation.
Companion to the ESP32 firmware in `EmbeddedSw/`.

## Components

### truck_simulator.py
- Simulates one or more trucks in a 2-D field and publishes telemetry via MQTT.
- **Field**: configurable width × height in cm (currently 120 × 240 cm).
- **Obstacles**: static axis-aligned rectangles defined in `OBSTACLES`.
- **Sensors**: 4 ultrasonic rays (front, back, left, right) with Gaussian noise.
- **Strategy pattern**: `ExploreStrategy` base class; concrete strategies plugged into `Truck`.
  - `ReactiveExplore`: simple turn-away-from-obstacle logic.
  - `EmbeddedExplore`: mirrors the ESP32 embedded control loop exactly (EMA filtering, corridor centering, hysteresis, timed recovery).
- **MQTT topic**: `{TRUCK_ID}/telemetry` (default `ESP32_TRUCK_SIM/telemetry`).

### map_visualization.py
- Subscribes to `+/telemetry` (wildcard) and discovers trucks dynamically.
- Dead-reckons position from `compass`/`heading` + `cmd_pwm` if `x`/`y` not in payload; uses ground-truth `x`/`y` when available (simulator).
- Projects sensor rays to collect wall-hit points.
- Fits an axis-aligned bounding rectangle to wall points (percentile-trimmed).
- Renders path, wall scatter, heading arrow, and estimated boundary in real time via matplotlib `FuncAnimation`.

## MQTT
- Broker: `192.168.2.2:1883` (configurable via `--broker`/`--port`).
- Topic pattern: `{truck_id}/telemetry`.
- Payload: JSON — see `docs/VARIABLE_MODEL.md` for field definitions.

## Key Decisions
- Modular strategy pattern chosen so embedded exploration logic can be simulated without changing the truck core.
- `EmbeddedExplore` keeps all strategy-specific state (filtered values, steer hysteresis, recovery timer) inside the strategy, not on `Truck`.
- Boundary inference uses percentile trimming (not min/max) to be robust to sensor noise outliers.
