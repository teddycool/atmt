# Task

## Active Task
Fix map_visualization.py real-time path rendering.

## Scope
- Path line must update correctly from x/y positions received via MQTT every second.
- Show current truck position as a dot + heading arrow.
- Fix auto-scaling: must not scan all collected points every frame.
- Fix wall scatter: must not fail when no wall points collected yet.

## Out of Scope
- Changes to MQTT, strategy logic, or simulator.

## Definition of Done
- Path draws correctly as a dashed line through all received x/y positions.
- Current position shown as a filled dot.
- Auto-scale and scatter are robust from the first message onward.
