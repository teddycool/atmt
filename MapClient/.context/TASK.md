# Task

## Active Task
Port the ESP32 control loop from `EmbeddedSw/src/z_main_control_loop.cpp` to a clean
Python file (`MapClient/embedded_control.py`).

## Scope / Constraints
- Faithful translation of the C++ logic — same state machine, same config defaults, same algorithm order.
- Simple and readable; no extra features beyond what the C++ does.
- Must be compatible with `truck_simulator.py`'s `ExploreStrategy` interface (`step(truck, sensors)`).
- No modification to `truck_simulator.py` unless the user asks.

## Definition of Done
- `embedded_control.py` exists in `MapClient/`.
- Contains `EmbeddedController` with the same IDLE→EXPLORE→RECOVER state machine.
- Config defaults match the C++ `ControlConfig` struct exactly.
