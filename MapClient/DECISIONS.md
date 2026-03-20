# Decisions

| # | Date | User | Decision | Rationale |
|---|------|------|----------|-----------|
| 1 | 2026-03-19 | ajuan | Use modular `ExploreStrategy` pattern in truck_simulator.py | Allows swapping exploration logic without changing truck core; enables testing embedded algorithm in simulation |
| 2 | 2026-03-19 | ajuan | `EmbeddedExplore` keeps all state inside strategy, not on `Truck` | Strategy-specific state (turn counters, filters, recovery timer) should not pollute the truck model |
| 3 | 2026-03-19 | ajuan | Boundary inference uses percentile trimming not min/max | Robust to sensor noise outliers that would otherwise inflate the estimated boundary |
| 4 | 2026-03-19 | ajuan | Removed MCP references from AGENTS.md | MCP server not available in this project setup; local `.context/` is the sole authority |
