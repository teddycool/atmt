"""
truck.py — Truck state model for the truck simulator.

The Truck class holds the complete physical and sensor state of the truck
at any point in time.  It is a pure data container — simulation logic,
sensor computation, and control strategies live in separate modules.

Coordinate conventions
-----------------------
World frame  : x = East, y = North, z = up  (right-hand, cm)
Truck frame  : x = forward, y = left, z = up (right-hand)
Heading      : degrees, 0 = East, 90 = North, CCW positive
Distances    : cm
Velocity     : cm/s
Acceleration : m/s²  (matches MPU-6050 output units)
Gyroscope    : deg/s (matches MPU-6050 output units)
Magnetometer : µT    (matches HMC/QMC compass output units)
"""

from dataclasses import dataclass, field


@dataclass
class Position:
    """3-D position in the world frame (cm)."""
    x: float = 0.0   # East
    y: float = 0.0   # North
    z: float = 0.0   # Up (0 = floor level)


@dataclass
class Velocity:
    """3-D velocity in the world frame (cm/s)."""
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0


@dataclass
class Acceleration:
    """
    3-D specific force in the truck local frame (m/s²).
    On a flat stationary surface: acc_x≈0, acc_y≈0, acc_z≈+9.81 (gravity).
    """
    x: float = 0.0    # forward axis
    y: float = 0.0    # left axis
    z: float = 9.81   # up axis (gravity at rest)


@dataclass
class Gyroscope:
    """3-D angular velocity in the truck local frame (deg/s)."""
    x: float = 0.0   # roll rate
    y: float = 0.0   # pitch rate
    z: float = 0.0   # yaw rate  (positive = CCW when viewed from above)


@dataclass
class Magnetometer:
    """
    3-D magnetic field vector in the truck local frame (µT).
    Derived compass heading: atan2(mag_x, mag_y) → heading in radians.
    """
    x: float = 0.0   # forward component  (max when heading = North = 90°)
    y: float = 0.0   # left component     (max when heading = East  =  0°)
    z: float = 0.0   # vertical component (constant on flat ground)


@dataclass
class UltrasonicSensors:
    """Distance readings from the four ultrasonic sensors (cm)."""
    front: float = 250.0   # uf
    back:  float = 250.0   # ub
    left:  float = 250.0   # ul
    right: float = 250.0   # ur


@dataclass
class Truck:
    """
    Complete state of the truck at one simulation tick.

    Fields are grouped by sensor/subsystem so they map directly to the
    real ESP32 hardware (MPU-6050 IMU, ultrasonic sensors, motor driver).
    """

    # ── Pose ─────────────────────────────────────────────────────────────────
    position:  Position  = field(default_factory=Position)
    heading:   float     = 0.0    # degrees, 0=East, 90=North, CCW positive

    # ── Kinematics ────────────────────────────────────────────────────────────
    velocity:     Velocity     = field(default_factory=Velocity)
    acceleration: Acceleration = field(default_factory=Acceleration)

    # ── IMU ───────────────────────────────────────────────────────────────────
    gyroscope:    Gyroscope    = field(default_factory=Gyroscope)
    magnetometer: Magnetometer = field(default_factory=Magnetometer)
    compass:      float        = 0.0   # heading derived from magnetometer (degrees)

    # ── Distance sensors ─────────────────────────────────────────────────────
    ultrasonic: UltrasonicSensors = field(default_factory=UltrasonicSensors)

    # ── Control output ────────────────────────────────────────────────────────
    cmd_pwm:   int = 0           # motor PWM command  (-100 … +100)
    cmd_steer: str = "STRAIGHT"  # "STRAIGHT" | "LEFT" | "RIGHT"

    # ── Telemetry metadata ────────────────────────────────────────────────────
    truck_id: str = "SIM_TRUCK"
    seq:      int = 0     # message sequence counter
    t_ms:     int = 0     # simulation time (ms)

    # ── Convenience properties ────────────────────────────────────────────────

    @property
    def x(self): return self.position.x

    @x.setter
    def x(self, v): self.position.x = v

    @property
    def y(self): return self.position.y

    @y.setter
    def y(self, v): self.position.y = v

    @property
    def z(self): return self.position.z

    @z.setter
    def z(self, v): self.position.z = v

    @property
    def yaw_rate(self):
        """Yaw rate (deg/s) — shortcut to gyroscope.z."""
        return self.gyroscope.z

    def to_dict(self):
        """
        Serialize truck state to a flat dict matching the MQTT telemetry
        JSON schema defined in GUARDRAILS.md.
        """
        return {
            "truck_id":      self.truck_id,
            "seq":           self.seq,
            "t_ms":          self.t_ms,
            "mode":          "EXPLORE",
            "ul":            round(self.ultrasonic.left,  1),
            "ur":            round(self.ultrasonic.right, 1),
            "uf":            round(self.ultrasonic.front, 1),
            "ub":            round(self.ultrasonic.back,  1),
            "yaw_rate":      round(self.gyroscope.z,      2),
            "heading":       round(self.heading,           1),
            "compass":       round(self.compass,           1),
            "mag_x":         round(self.magnetometer.x,   2),
            "mag_y":         round(self.magnetometer.y,   2),
            "mag_z":         round(self.magnetometer.z,   2),
            "acc_x":         round(self.acceleration.x,   3),
            "acc_y":         round(self.acceleration.y,   3),
            "acc_z":         round(self.acceleration.z,   3),
            "width":         round(self.ultrasonic.left + self.ultrasonic.right, 1),
            "center_error":  round(self.ultrasonic.right - self.ultrasonic.left, 1),
            "front_blocked": self.ultrasonic.front < 20.0,
            "cmd_pwm":       self.cmd_pwm,
            "cmd_steer":     self.cmd_steer,
        }

    def __repr__(self):
        return (
            f"Truck(id={self.truck_id!r}  t={self.t_ms}ms  seq={self.seq}\n"
            f"  pos=({self.x:.1f}, {self.y:.1f}, {self.z:.1f}) cm  "
            f"heading={self.heading:.1f}°  compass={self.compass:.1f}°\n"
            f"  vel=({self.velocity.x:.1f}, {self.velocity.y:.1f}) cm/s\n"
            f"  acc=({self.acceleration.x:.3f}, {self.acceleration.y:.3f}, "
            f"{self.acceleration.z:.3f}) m/s²\n"
            f"  gyro=({self.gyroscope.x:.2f}, {self.gyroscope.y:.2f}, "
            f"{self.gyroscope.z:.2f}) deg/s\n"
            f"  mag=({self.magnetometer.x:.2f}, {self.magnetometer.y:.2f}, "
            f"{self.magnetometer.z:.2f}) µT\n"
            f"  us: uf={self.ultrasonic.front:.1f}  ub={self.ultrasonic.back:.1f}  "
            f"ul={self.ultrasonic.left:.1f}  ur={self.ultrasonic.right:.1f} cm\n"
            f"  cmd: pwm={self.cmd_pwm}  steer={self.cmd_steer!r})"
        )
