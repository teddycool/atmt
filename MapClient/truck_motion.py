"""
truck_motion.py — Truck movement physics (Ackermann / bicycle steering model).

Physics summary
---------------
The truck is modelled as a rigid body with two axles (bicycle model).
Steering angle δ rotates the front wheels; when the truck moves it follows
a circular arc of radius R = wheelbase / tan(|δ|).

  - Forward  (cmd_pwm > 0): truck curves toward the steered direction.
  - Reverse  (cmd_pwm < 0): truck curves away from the steered direction
                             (front wheels trail, as on a real vehicle).
  - Stationary (cmd_pwm=0): heading does NOT change regardless of steering.

Steering angle
--------------
  Adjusted in steps of STEER_STEP degrees.
  Clamped to [-MAX_STEER, +MAX_STEER].
  Positive = left, negative = right  (standard vehicle convention).

IMU outputs updated each tick
------------------------------
  gyroscope.z  = yaw rate (deg/s)
  acceleration = world-frame Δvelocity projected onto truck local axes (m/s²)
                 + gravity on z-axis (+9.81 m/s² at rest)
"""

import math
import random
from truck import Truck

# ── Constants ─────────────────────────────────────────────────────────────────

STEER_STEP    =  5.0    # degrees — one steering adjustment increment
MAX_STEER     = 10.0    # degrees — hard lock in either direction
STEER_NOISE   =  0.5    # degrees — Gaussian noise std-dev on effective steering angle
WHEELBASE_CM  = 15.0    # cm      — distance between front and rear axles
MAX_SPEED_CMS = 100.0   # cm/s   — speed at 100 % PWM
G             =   9.81  # m/s²   — gravitational acceleration


# ── TruckMotion ───────────────────────────────────────────────────────────────

class TruckMotion:
    """
    Manages the truck's steering angle and motor command and advances
    the truck's physical state by one simulation tick.

    Usage
    -----
        motion = TruckMotion()
        motion.drive_forward(pwm=80)
        motion.steer_left()
        motion.update(truck, dt=0.2)   # call once per tick
    """

    def __init__(self, wheelbase: float = WHEELBASE_CM,
                 max_speed: float = MAX_SPEED_CMS):
        self.wheelbase     = wheelbase
        self.max_speed     = max_speed
        self._steer_deg    = 0.0   # current steering angle (deg)
        self._cmd_pwm      = 0     # current motor command  (-100 … +100)

    # ── Steering control ──────────────────────────────────────────────────────

    def steer_left(self):
        """Rotate front wheels one step further to the left."""
        self._steer_deg = min(MAX_STEER, self._steer_deg + STEER_STEP)

    def steer_right(self):
        """Rotate front wheels one step further to the right."""
        self._steer_deg = max(-MAX_STEER, self._steer_deg - STEER_STEP)

    def steer_center(self):
        """Return front wheels to the straight-ahead position."""
        self._steer_deg = 0.0

    def set_steer(self, angle_deg: float):
        """Set steering angle directly (clamped to ±MAX_STEER, snapped to grid)."""
        snapped = round(angle_deg / STEER_STEP) * STEER_STEP
        self._steer_deg = max(-MAX_STEER, min(MAX_STEER, snapped))

    # ── Motor control ─────────────────────────────────────────────────────────

    def drive_forward(self, pwm: int = 100):
        """Drive forward. pwm in [1, 100]."""
        self._cmd_pwm = max(1, min(100, int(pwm)))

    def drive_reverse(self, pwm: int = 100):
        """Drive in reverse. pwm in [1, 100] (sign applied internally)."""
        self._cmd_pwm = -max(1, min(100, int(pwm)))

    def stop(self):
        """Cut motor power."""
        self._cmd_pwm = 0

    def set_pwm(self, pwm: int):
        """Set motor command directly (-100 … +100)."""
        self._cmd_pwm = max(-100, min(100, int(pwm)))

    # ── Properties ────────────────────────────────────────────────────────────

    @property
    def steering_angle(self) -> float:
        """Current steering angle in degrees (+= left, -= right)."""
        return self._steer_deg

    @property
    def cmd_pwm(self) -> int:
        return self._cmd_pwm

    @property
    def cmd_steer(self) -> str:
        """Discretised steer string matching the MQTT telemetry schema."""
        if self._steer_deg > 0:
            return "LEFT"
        if self._steer_deg < 0:
            return "RIGHT"
        return "STRAIGHT"

    @property
    def speed(self) -> float:
        """Current speed in cm/s (positive = forward, negative = reverse)."""
        return (self._cmd_pwm / 100.0) * self.max_speed

    # ── Physics step ──────────────────────────────────────────────────────────

    def update(self, truck: Truck, dt: float) -> None:
        """
        Advance the truck's physical state by dt seconds.

        Updates on the truck object
        ---------------------------
          position  (x, y)
          heading
          velocity  (world frame, cm/s)
          acceleration (truck local frame, m/s²)
          gyroscope.z  (yaw rate, deg/s)
          cmd_pwm / cmd_steer

        Parameters
        ----------
        truck : Truck   — state object to update in-place
        dt    : float   — time step in seconds
        """
        v     = self.speed                        # cm/s (signed)
        noisy_steer = self._steer_deg + random.gauss(0, STEER_NOISE)
        delta = math.radians(noisy_steer)        # steering angle in radians (with noise)

        # Snapshot previous velocity for acceleration calculation
        prev_vx = truck.velocity.x
        prev_vy = truck.velocity.y

        # ── Heading and position update ───────────────────────────────────────
        if abs(v) < 1e-3:
            # Stationary — no movement, no yaw
            dh_deg = 0.0
            new_vx = new_vy = 0.0

        elif abs(delta) < math.radians(0.1):
            # Straight line (steering angle effectively zero)
            h_rad  = math.radians(truck.heading)
            new_vx = v * math.cos(h_rad)
            new_vy = v * math.sin(h_rad)
            truck.x += new_vx * dt
            truck.y += new_vy * dt
            dh_deg  = 0.0

        else:
            # Circular arc — bicycle model
            # R = wheelbase / tan(|δ|)
            # ω = v / R  (positive CCW)
            # Sign: forward+left → CCW (+ω); reverse+left → CW (−ω) — handled by v sign
            R      = self.wheelbase / math.tan(abs(delta))
            omega  = (v / R) * math.copysign(1.0, delta)   # rad/s

            dh_rad = omega * dt
            dh_deg = math.degrees(dh_rad)

            # Integrate position using the midpoint heading for accuracy
            mid_h  = math.radians(truck.heading) + dh_rad / 2.0
            new_vx = v * math.cos(mid_h)
            new_vy = v * math.sin(mid_h)
            truck.x       += new_vx * dt
            truck.y       += new_vy * dt
            truck.heading  = (truck.heading + dh_deg) % 360

        # ── Velocity ─────────────────────────────────────────────────────────
        truck.velocity.x = new_vx
        truck.velocity.y = new_vy
        truck.velocity.z = 0.0

        # ── Gyroscope — yaw rate (deg/s) ─────────────────────────────────────
        truck.gyroscope.z = dh_deg / dt if dt > 0 else 0.0

        # ── Acceleration (world-frame Δv → truck local frame) ────────────────
        if dt > 0:
            ax_world = (new_vx - prev_vx) / dt / 100.0   # cm/s² → m/s²
            ay_world = (new_vy - prev_vy) / dt / 100.0

            h = math.radians(truck.heading)
            cos_h, sin_h = math.cos(h), math.sin(h)

            # Project onto truck local axes (x=forward, y=left)
            truck.acceleration.x =  ax_world * cos_h + ay_world * sin_h
            truck.acceleration.y = -ax_world * sin_h + ay_world * cos_h
        truck.acceleration.z = G    # gravity always present on z-axis

        # ── Control record ────────────────────────────────────────────────────
        truck.cmd_pwm   = self._cmd_pwm
        truck.cmd_steer = self.cmd_steer

    def __repr__(self):
        return (
            f"TruckMotion(steer={self._steer_deg:+.0f}°  "
            f"pwm={self._cmd_pwm:+d}  speed={self.speed:+.1f} cm/s)"
        )
