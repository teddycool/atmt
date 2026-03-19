"""
Embedded exploration controller for truck_simulator.py.

Logic (3-step loop):
  1. DRIVE   — go straight until front sensor < front_stop_dist.
  2. RECOVER — reverse while steering toward the side with more space
               (max(ul, ur)) for recover_ms milliseconds.
  3. Repeat from 1.
"""

import math
from dataclasses import dataclass

# ── Simulator motion constants — must match truck_simulator.py ────────────────
TRUCK_SPEED  = 2.0    # cm per tick at 100 % PWM
TURN_ANGLE   = 15.0   # degrees per tick when steering
FIELD_WIDTH  = 120.0  # cm
FIELD_HEIGHT = 240.0  # cm


@dataclass
class Config:
    front_stop_dist: float = 20.0   # cm — trigger recovery when uf drops below this
    pwm_forward:     int   = 100    # PWM while driving straight
    pwm_reverse:     int   = -100   # PWM while reversing
    recover_ms:      int   = 1500   # ms spent reversing + steering before driving again


class EmbeddedController:
    """
    Two-state explore controller compatible with truck_simulator.py ExploreStrategy.
    Call step(truck, sensors) once per simulation tick.
    """

    def __init__(self, cfg: Config = None):
        self.cfg   = cfg or Config()
        self.state = "DRIVE"
        self._recover_start_ms = 0
        self._recover_steer    = "STRAIGHT"  # committed at recovery entry

    def step(self, truck, raw_sensors):
        """
        truck:       object with .x, .y, .heading (deg), .t_ms (ms)
        raw_sensors: dict with ul, ur, uf, ub (cm)
        """
        cfg  = self.cfg
        t_ms = truck.t_ms
        uf   = raw_sensors.get("uf", cfg.front_stop_dist + 1)
        ul   = raw_sensors.get("ul", 0.0)
        ur   = raw_sensors.get("ur", 0.0)

        if self.state == "DRIVE":
            if uf < cfg.front_stop_dist:
                # Enter recovery: pick the side with more clearance once and hold it
                self._recover_steer    = "RIGHT" if ur > ul else "LEFT"
                self._recover_start_ms = t_ms
                self.state             = "RECOVER"
                # Stop this tick — do not move forward into the obstacle
                return
            self._apply(truck, cfg.pwm_forward, "STRAIGHT")

        else:  # RECOVER
            if (t_ms - self._recover_start_ms) >= cfg.recover_ms:
                self.state = "DRIVE"
                self._apply(truck, cfg.pwm_forward, "STRAIGHT")
            else:
                self._apply(truck, cfg.pwm_reverse, self._recover_steer)

    def _apply(self, truck, pwm, steer):
        """Translate (pwm, steer) into heading + position change."""
        if steer == "LEFT":
            truck.heading = (truck.heading - TURN_ANGLE) % 360
        elif steer == "RIGHT":
            truck.heading = (truck.heading + TURN_ANGLE) % 360

        dist  = TRUCK_SPEED * (pwm / 100.0)
        angle = math.radians(truck.heading)
        truck.x = max(1.0, min(FIELD_WIDTH  - 1.0, truck.x + dist * math.cos(angle)))
        truck.y = max(1.0, min(FIELD_HEIGHT - 1.0, truck.y + dist * math.sin(angle)))
