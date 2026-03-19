# Task

## Active Task
Add real-time path visualization to the truck simulator.

## Scope / Constraints
- Add `--visualize` flag to `truck_simulator.py`.
- When active: show a live matplotlib window with red room boundary, obstacles in red, dashed path line, and current-position dot.
- Simulation logic and MQTT publishing must be unchanged.
- Visualization runs on the main thread (matplotlib requirement); simulation runs in a daemon thread.
- No new files — add the visualizer class directly to `truck_simulator.py`.

## Definition of Done
- `python truck_simulator.py --dry-run --visualize` opens a live window that updates in real time.
- Room boundary is drawn in red; path is a dashed line; current position is a dot.
- Stopping with Ctrl-C closes cleanly.
