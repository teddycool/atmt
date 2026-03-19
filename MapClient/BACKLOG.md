# Backlog

| # | Item | Added | Notes |
|---|------|-------|-------|
| 1 | Fix simulator payload to emit real heading/cmd_pwm/cmd_steer values | 2026-03-19 | Needed for accurate dead-reckoning in map_visualization.py when x/y not present |
| 2 | Add `--strategy` selector to map_visualization.py for replay/comparison mode | 2026-03-19 | Would allow overlaying two strategy paths |
| 3 | Validate EmbeddedExplore timing — simulator tick is 200 ms vs embedded 20 ms | 2026-03-19 | Recovery phase durations may need scaling |
