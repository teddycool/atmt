# Plan

## Active Task
Fix map_visualization.py real-time path rendering.

## Steps Completed
- (none yet)

## In Progress
- Rewriting run_visualizer() and TruckState to fix all issues.

## Steps Remaining
1. Track axis bounds incrementally in TruckState (min/max updated on each message).
2. Fix scatter: initialise with empty numpy array so set_offsets always works.
3. Add current-position dot artist per truck.
4. Remove the O(n) all_x/all_y scan from the update loop.
5. Update context files.
