# State

## Active Task
Port z_main_control_loop.cpp → embedded_control.py — COMPLETE.

## Live Facts
- Active truck model: JCA01
- `embedded_control.py` created in MapClient/.
- `EmbeddedController` is a faithful port of the C++ control loop (same config defaults, same algorithm order).
- Compatible with truck_simulator.py ExploreStrategy interface: `step(truck, sensors)`.
- MQTT broker: 192.168.2.2:1883

## Blockers
None.
