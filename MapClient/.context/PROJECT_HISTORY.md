# Project History

## 2026-03-19 — Visualizer & simulator UX fixes (ajuan)
- `map_visualization.py`: boundary rectangle now always rendered in red; path line changed to dashed.
- `truck_simulator.py`: truck starts at random position ≥20 cm from any border (`START_MARGIN = 20.0`).
- `truck_simulator.py`: position printed every 5 seconds with `[POS]` prefix.
- Task closed.

## 2026-03-19 — MapClient bootstrap (ajuan)
- Created `truck_simulator.py`: 2-D field simulator publishing MQTT telemetry.
  - `ReactiveExplore` strategy: simple obstacle-avoidance.
  - `EmbeddedExplore` strategy: mirrors ESP32 `z_main_control_loop.cpp` exactly (EMA filter, corridor centering, hysteresis, timed recovery).
  - Field set to 120 × 240 cm.
  - `--strategy` CLI flag to select strategy at runtime.
- Created `map_visualization.py`: real-time MQTT subscriber + matplotlib visualizer.
  - Wildcard subscription `+/telemetry` discovers all trucks dynamically.
  - Dead-reckons position or uses ground-truth x/y from simulator payload.
  - Projects sensor rays to collect wall-hit points; fits estimated bounding rectangle.
  - Logs all incoming messages to stdout.
- Created `AGENTS.md` context workflow file for MapClient.
- Bootstrapped `.context/` infrastructure: GUARDRAILS, ARCHITECTURE, STATE, PLAN, TASK, PROJECT_HISTORY, TOOLS, ILLUSTRATION_FORMAT, resources.
- Created `CONFLICTS.md`, `BACKLOG.md`, `DECISIONS.md`.
