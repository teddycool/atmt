# Plan

## Active Task
Add SLAM to map_visualization.py.

## Steps Completed
- [x] `bresenham()` ray iterator.
- [x] `OccupancyGrid`: log-odds grid, Bresenham ray tracing, probability_map().
- [x] `SLAMProcessor`: pose from payload (x/y/heading) or dead-reckoned; grid + trajectory.
- [x] `MQTTListener`: first discovered truck routed to SLAM.
- [x] Figure layout: GridSpec 2×3, left 2×2 sensor charts, right SLAM panel.
- [x] SLAM panel: occupancy grid image + dashed trajectory + position dot + heading arrow.
- [x] Auto-zoom SLAM view to visited area.
- [x] Context files updated.

## In Progress
- (none)

## Steps Remaining
- (none)
