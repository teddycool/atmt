# State

## Active Task
Add real-time path visualization to truck_simulator.py — COMPLETE.

## Live Facts
- Active truck model: JCA01
- Simulator: field 120×240 cm, random start ≥20 cm from border, speed 0.5 cm/tick.
- `--visualize` flag added: opens matplotlib window, red boundary + obstacles, dashed path, current-position dot.
- Sim runs in daemon thread when `--visualize`; matplotlib owns the main thread.
- MQTT broker: 192.168.2.2:1883

## Blockers
None.
