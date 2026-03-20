"""
explore_strategy.py — Exploration strategies for the truck simulator.

Each strategy implements one method:

    step(truck, motion, sensors, dt) -> None

The strategy reads sensor distances and the truck state, then calls methods
on the TruckMotion object to set speed and steering for the current tick.

Available strategies
--------------------
  ReactiveExplore   — go straight, max-steer away from any obstacle
  EmbeddedController — drive/recover state machine (mirrors the C++ firmware)
  WallFollower       — maintain a fixed distance from the right-hand wall

Adding a new strategy
---------------------
  1. Subclass ExploreStrategy.
  2. Implement step().
  3. Register it in STRATEGIES at the bottom of this file.
"""

import math
from abc import ABC, abstractmethod
from truck import Truck
from truck_motion import TruckMotion, MAX_STEER, STEER_STEP


# ── Base class ────────────────────────────────────────────────────────────────

class ExploreStrategy(ABC):
    """
    Abstract base for all exploration strategies.

    Subclasses must implement step().  They may keep internal state
    (timers, state-machine variables) as instance attributes.
    """

    @abstractmethod
    def step(self, truck: Truck, motion: TruckMotion,
             sensors: dict, dt: float) -> None:
        """
        Decide and apply control for one simulation tick.

        Parameters
        ----------
        truck   : current truck state (read for position/heading/t_ms)
        motion  : motion controller  (write via steer_*/drive_*/stop)
        sensors : dict with keys 'uf', 'ub', 'ul', 'ur'  (cm)
        dt      : tick duration in seconds
        """

    def reset(self):
        """Optional: reset internal state (called when strategy is re-used)."""


# ── ReactiveExplore ───────────────────────────────────────────────────────────

class ReactiveExplore(ExploreStrategy):
    """
    Simple reactive exploration.

    Behaviour
    ---------
    - Drive forward at full speed with steering centred.
    - If the front sensor drops below STOP_DIST:
        • stop the motor
        • steer toward whichever side has more clearance (max angle)
    - Once the front clears CLEAR_DIST the steering re-centres and the
      truck drives forward again.

    This produces a bounce-around behaviour that gradually covers the room.
    """

    STOP_DIST  = 20.0   # cm — trigger obstacle avoidance
    CLEAR_DIST = 30.0   # cm — resume forward driving

    def __init__(self):
        self._avoiding = False

    def step(self, truck, motion, sensors, dt):
        uf, ul, ur = sensors["uf"], sensors["ul"], sensors["ur"]

        if self._avoiding:
            if uf >= self.CLEAR_DIST:
                # Obstacle cleared — straighten up and go
                motion.steer_center()
                motion.drive_forward()
                self._avoiding = False
            else:
                # Keep turning in place (reversing with full lock)
                motion.drive_reverse(pwm=60)
        else:
            if uf < self.STOP_DIST:
                self._avoiding = True
                motion.stop()
                # Choose the side with more clearance
                if ur >= ul:
                    motion.set_steer(-MAX_STEER)   # steer right
                else:
                    motion.set_steer(+MAX_STEER)   # steer left
            else:
                motion.drive_forward()
                motion.steer_center()

    def reset(self):
        self._avoiding = False


# ── EmbeddedController ────────────────────────────────────────────────────────

class EmbeddedController(ExploreStrategy):
    """
    Drive / Recover state machine — a faithful port of the C++ firmware logic
    (z_main_control_loop.cpp).

    States
    ------
    DRIVE   : full speed forward, steering centred.
              Transitions to RECOVER when uf < front_stop_dist.
    RECOVER : reverse with maximum steering toward the open side for
              recover_ms milliseconds, then back to DRIVE.

    Parameters
    ----------
    front_stop_dist : cm   — obstacle detection threshold
    pwm_forward     : %    — motor power while driving  (0–100)
    pwm_reverse     : %    — motor power while reversing (0–100)
    recover_ms      : ms   — duration of the reverse+steer phase
    """

    def __init__(self,
                 front_stop_dist: float = 20.0,
                 pwm_forward:     int   = 100,
                 pwm_reverse:     int   = 80,
                 recover_ms:      int   = 1500):
        self.front_stop_dist = front_stop_dist
        self.pwm_forward     = pwm_forward
        self.pwm_reverse     = pwm_reverse
        self.recover_ms      = recover_ms

        self._state            = "DRIVE"
        self._recover_start_ms = 0
        self._recover_steer    = +MAX_STEER   # decided at RECOVER entry

    def step(self, truck, motion, sensors, dt):
        uf, ul, ur = sensors["uf"], sensors["ul"], sensors["ur"]
        t_ms       = truck.t_ms

        if self._state == "DRIVE":
            if uf < self.front_stop_dist:
                # Enter recovery: pick the open side once and commit
                self._recover_steer    = +MAX_STEER if ul < ur else -MAX_STEER
                self._recover_start_ms = t_ms
                self._state            = "RECOVER"
                motion.stop()
            else:
                motion.drive_forward(self.pwm_forward)
                motion.steer_center()

        else:  # RECOVER
            elapsed = t_ms - self._recover_start_ms
            if elapsed >= self.recover_ms:
                self._state = "DRIVE"
                motion.steer_center()
                motion.drive_forward(self.pwm_forward)
            else:
                motion.set_steer(self._recover_steer)
                motion.drive_reverse(self.pwm_reverse)

    def reset(self):
        self._state            = "DRIVE"
        self._recover_start_ms = 0


# ── WallFollower ──────────────────────────────────────────────────────────────

class WallFollower(ExploreStrategy):
    """
    Right-hand wall follower.

    The truck tries to keep its right side at `target_dist` cm from the wall.
    A proportional controller converts the lateral error into a steering
    adjustment (in STEER_STEP increments so the model is respected).

    Behaviour
    ---------
    - Normal: adjust steering to maintain target distance on the right.
    - Front blocked: steer hard left until the front clears.
    - No wall on right (ur > search_dist): steer right to find the wall.

    Parameters
    ----------
    target_dist : cm — desired clearance on the right side
    stop_dist   : cm — front obstacle threshold
    search_dist : cm — if ur > this, no right wall detected; steer right
    kp          : proportional gain (steer steps per cm of error)
    """

    def __init__(self,
                 target_dist: float = 25.0,
                 stop_dist:   float = 20.0,
                 search_dist: float = 60.0,
                 kp:          float = 0.3):
        self.target_dist = target_dist
        self.stop_dist   = stop_dist
        self.search_dist = search_dist
        self.kp          = kp
        self._turning_left = False

    def step(self, truck, motion, sensors, dt):
        uf, ul, ur = sensors["uf"], sensors["ul"], sensors["ur"]

        # ── Front obstacle: turn left until clear ────────────────────────────
        if uf < self.stop_dist:
            self._turning_left = True
            motion.drive_forward(pwm=50)
            motion.set_steer(+MAX_STEER)
            return

        if self._turning_left and uf >= self.stop_dist + 10:
            self._turning_left = False

        # ── Wall following: proportional steering on right-side error ────────
        if ur > self.search_dist:
            # No wall on right — steer right to find it
            motion.set_steer(-MAX_STEER)
            motion.drive_forward(pwm=70)
        else:
            error  = self.target_dist - ur   # positive → too close, steer left
            steps  = error * self.kp
            angle  = round(steps / STEER_STEP) * STEER_STEP
            angle  = max(-MAX_STEER, min(+MAX_STEER, angle))
            motion.set_steer(angle)
            motion.drive_forward(pwm=100)

    def reset(self):
        self._turning_left = False


# ── Strategy registry ─────────────────────────────────────────────────────────

STRATEGIES: dict[str, type] = {
    "reactive":   ReactiveExplore,
    "embedded":   EmbeddedController,
    "wall_right": WallFollower,
}


def make_strategy(name: str, **kwargs) -> ExploreStrategy:
    """
    Instantiate a strategy by name.

    Parameters
    ----------
    name   : one of the keys in STRATEGIES
    kwargs : forwarded to the strategy constructor

    Example
    -------
        strategy = make_strategy("embedded", recover_ms=800)
    """
    if name not in STRATEGIES:
        raise ValueError(
            f"Unknown strategy {name!r}. Available: {list(STRATEGIES)}")
    return STRATEGIES[name](**kwargs)
