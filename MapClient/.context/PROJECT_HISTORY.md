# Project History

## 2026-03-19 — Created embedded_control.py (ajuan)
- Faithful Python port of `EmbeddedSw/src/z_main_control_loop.cpp`.
- `EmbeddedController` class: IDLE→EXPLORE→RECOVER state machine, EMA sensor filtering,
  corridor centering, side avoidance, steering hysteresis, timed recovery sequence.
- Config defaults match C++ `ControlConfig` exactly (frontStopDist=12, recoverReverseMs=1000, etc.).
- Compatible with `truck_simulator.py` ExploreStrategy interface.

## 2026-03-19 — Added real-time path visualization to truck_simulator.py (ajuan)
- Added `SimVisualizer` class to `truck_simulator.py`: matplotlib window with red room boundary, red obstacle rectangles, dashed steelblue path line, current-position dot. FuncAnimation at 150 ms; thread-safe with a lock.
- Added `--visualize` CLI flag; matplotlib import guarded like paho-mqtt.
- Refactored `run()`: when `--visualize`, simulation runs in a daemon thread and matplotlib owns the main thread. Without the flag, behaviour is identical to before.
- No new files added.

## 2026-03-19 — Added SLAM to map_visualization.py (ajuan)
- Implemented `OccupancyGrid`: 400×400 cells at 2 cm/cell, log-odds updates, Bresenham ray tracing.
- Implemented `SLAMProcessor`: pose from payload x/y/heading (simulator) or dead-reckoned; one truck at a time.
- `MQTTListener` routes first discovered truck to SLAM.
- Figure restructured to 3-column GridSpec: 2×2 sensor charts left, SLAM map right.
- SLAM panel: live occupancy grid image (gray_r), dashed trajectory, position dot, heading arrow, auto-zoom.

## 2026-03-19 — Replaced map_visualization.py with sensor distance visualizer (ajuan)
- Removed all previous path/boundary visualization code.
- New implementation: 2×2 rolling time-series subplots, one per sensor (uf, ub, ul, ur).
- One coloured line per truck per subplot; trucks discovered dynamically via `+/telemetry`.
- Red/orange threshold lines at 12 cm (stop) and 20 cm (slow) on every subplot.
- Rolling window of 60 readings; updates every 200 ms.

## 2026-03-19 — Visualization real-time path fix (ajuan)
- Rewrote `run_visualizer()` and `TruckState` in `map_visualization.py`:
  - Added incremental axis bounds tracking (`_expand_bounds`) — O(1) per message, O(trucks) per frame instead of O(points).
  - Fixed wall scatter: initialised with `np.empty((0,2))` so `set_offsets` is safe from the first frame.
  - Added filled dot for current truck position.
  - Boundary rectangle label colour changed to red to match rect.
  - Path line clearly dashed, all artists update correctly each frame.

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
