"""
Autonomous Truck Simulator
Simulates an ESP32 truck exploring a field and publishing telemetry via MQTT.
"""

import json
import math
import random
import time
import threading
import argparse

try:
    import paho.mqtt.client as mqtt
    MQTT_AVAILABLE = True
except ImportError:
    print("[WARN] paho-mqtt not installed. Run: pip install paho-mqtt")
    MQTT_AVAILABLE = False

# ── Field & simulation constants ──────────────────────────────────────────────

FIELD_WIDTH  = 120.0   # cm
FIELD_HEIGHT = 240.0   # cm

TRUCK_SPEED       = 5.0    # cm per tick
TURN_ANGLE        = 15.0   # degrees per tick when turning
SENSOR_MAX_RANGE  = 150.0  # cm  (ultrasound max reading)
SENSOR_NOISE_CM   = 0.5    # ±cm random noise on each sensor
OBSTACLE_MARGIN   = 20.0   # cm  — turn when any front sensor is closer than this
TICK_RATE_HZ      = 5      # simulation ticks per second (= MQTT publish rate)

# Static rectangular obstacles: list of (x, y, width, height) in cm
OBSTACLES = [
    (80, 60, 20, 40),
    (140, 120, 25, 25),
]

# ── MQTT settings ─────────────────────────────────────────────────────────────

MQTT_BROKER  = "192.168.2.2"
MQTT_PORT    = 1883
TRUCK_ID     = "ESP32_TRUCK_SIM"
MQTT_TOPIC   = f"{TRUCK_ID}/telemetry"


# ── Geometry helpers ──────────────────────────────────────────────────────────

def ray_aabb_distance(ox, oy, dx, dy, rx, ry, rw, rh):
    """Distance from ray origin (ox,oy) in direction (dx,dy) to AABB rect, or inf."""
    tmin, tmax = -math.inf, math.inf
    for lo, d, rlo, rhi in [(ox, dx, rx, rx + rw), (oy, dy, ry, ry + rh)]:
        if abs(d) < 1e-9:
            if lo < rlo or lo > rhi:
                return math.inf
        else:
            t1 = (rlo - lo) / d
            t2 = (rhi - lo) / d
            if t1 > t2:
                t1, t2 = t2, t1
            tmin = max(tmin, t1)
            tmax = min(tmax, t2)
    if tmax < 0 or tmin > tmax:
        return math.inf
    return tmin if tmin >= 0 else tmax


def sense(x, y, heading_deg, sensor_offset_deg):
    """Return distance (cm) in the given direction, accounting for field walls and obstacles."""
    angle = math.radians(heading_deg + sensor_offset_deg)
    dx, dy = math.cos(angle), math.sin(angle)

    # Distance to field boundary walls — check all four wall planes as slabs
    candidates = []
    if abs(dx) > 1e-9:
        t = (0 - x) / dx;          candidates.append(t)
        t = (FIELD_WIDTH - x) / dx; candidates.append(t)
    if abs(dy) > 1e-9:
        t = (0 - y) / dy;           candidates.append(t)
        t = (FIELD_HEIGHT - y) / dy; candidates.append(t)
    wall_dist = min((t for t in candidates if t > 1e-3), default=SENSOR_MAX_RANGE)

    # Distance to each obstacle
    obs_dist = min(
        (ray_aabb_distance(x, y, dx, dy, ox, oy, ow, oh)
         for ox, oy, ow, oh in OBSTACLES),
        default=math.inf,
    )

    raw = min(wall_dist, obs_dist, SENSOR_MAX_RANGE)
    noise = random.gauss(0, SENSOR_NOISE_CM)
    return round(max(0.0, min(SENSOR_MAX_RANGE, raw + noise)), 1)


# ── Explore strategies ────────────────────────────────────────────────────────

class ExploreStrategy:
    """Base class for explore strategies. Subclasses implement step()."""

    def step(self, truck, sensors):
        """Mutate truck position/heading for one tick given sensor readings."""
        raise NotImplementedError


class ReactiveExplore(ExploreStrategy):
    """Default strategy: drive forward, turn away from obstacles reactively."""

    def __init__(self):
        self.turning = 0          # -1 left, 0 straight, +1 right
        self.turn_ticks_left = 0

    def step(self, truck, sensors):
        if self.turn_ticks_left > 0:
            truck.heading = (truck.heading + self.turning * TURN_ANGLE) % 360
            self.turn_ticks_left -= 1
        elif sensors["uf"] < OBSTACLE_MARGIN:
            self.turning = 1 if sensors["ur"] >= sensors["ul"] else -1
            self.turn_ticks_left = random.randint(3, 8)
        else:
            angle = math.radians(truck.heading)
            nx = truck.x + TRUCK_SPEED * math.cos(angle)
            ny = truck.y + TRUCK_SPEED * math.sin(angle)
            truck.x = max(1.0, min(FIELD_WIDTH  - 1.0, nx))
            truck.y = max(1.0, min(FIELD_HEIGHT - 1.0, ny))


# ── Truck state ───────────────────────────────────────────────────────────────

class Truck:
    def __init__(self, strategy=None):
        self.x = FIELD_WIDTH  / 2
        self.y = FIELD_HEIGHT / 2
        self.heading = random.uniform(0, 360)   # degrees, 0=East
        self.seq = 0
        self.t_ms = 0
        self.strategy = strategy or ReactiveExplore()

    def sensors(self):
        return {
            "ul": sense(self.x, self.y, self.heading, 270),  # left  = heading - 90°
            "ur": sense(self.x, self.y, self.heading,  90),  # right = heading + 90°
            "uf": sense(self.x, self.y, self.heading,   0),  # front
            "ub": sense(self.x, self.y, self.heading, 180),  # back
        }

    def step(self):
        """Advance one simulation tick."""
        s = self.sensors()
        self.strategy.step(self, s)
        self.seq  += 1
        self.t_ms += int(1000 / TICK_RATE_HZ)

    def payload(self):
        s = self.sensors()
        return {
            "truck_id":     TRUCK_ID,
            "seq":          self.seq,
            "t_ms":         self.t_ms,
            "mode":         "EXPLORE",
            "ul":           s["ul"],
            "ur":           s["ur"],
            "uf":           s["uf"],
            "ub":           s["ub"],
            "yaw_rate":     0,
            "heading":      0,
            "compass":      0,
            "mag_x":        0,
            "mag_y":        0,
            "mag_z":        0,
            "acc_x":        0,
            "acc_y":        0,
            "acc_z":        0,
            "width":        0,
            "center_error": 0,
            "front_blocked": False,
            "cmd_pwm":      0,
            "cmd_steer":    "STRAIGHT",
        }


# ── MQTT client ───────────────────────────────────────────────────────────────

class MQTTPublisher:
    def __init__(self, broker, port):
        self._connected = False
        if not MQTT_AVAILABLE:
            exit(1)
            return
        try:
            self._client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
        except AttributeError:
            self._client = mqtt.Client()
        self._client.on_connect    = self._on_connect
        self._client.on_disconnect = self._on_disconnect
        try:
            self._client.connect(broker, port, keepalive=60)
            self._client.loop_start()
        except Exception as e:
            print(f"[WARN] Could not connect to MQTT broker: {e}")

    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self._connected = True
            print(f"[MQTT] Connected to {MQTT_BROKER}:{MQTT_PORT}  topic={MQTT_TOPIC}")
        else:
            print(f"[MQTT] Connection failed (rc={rc})")

    def _on_disconnect(self, client, userdata, rc):
        self._connected = False
        print(f"[MQTT] Disconnected (rc={rc})")

    def publish(self, topic, payload_dict):
        msg = json.dumps(payload_dict)
        if self._connected:
            self._client.publish(topic, msg)
        # Always print to stdout
        print(f"[PUB] {topic}  {msg}")


# ── Main loop ─────────────────────────────────────────────────────────────────

def run(broker, port, dry_run):
    publisher = MQTTPublisher(broker, port) if not dry_run else None
    truck = Truck()
    interval = 1.0 / TICK_RATE_HZ

    print(f"[SIM] Field {FIELD_WIDTH}x{FIELD_HEIGHT} cm  |  "
          f"truck starts at ({truck.x:.0f},{truck.y:.0f})  heading={truck.heading:.0f}°")
    print(f"[SIM] Publishing every {interval*1000:.0f} ms  (Ctrl-C to stop)\n")

    try:
        while True:
            tick_start = time.monotonic()
            truck.step()
            payload = truck.payload()

            if publisher:
                publisher.publish(MQTT_TOPIC, payload)
            else:
                print(json.dumps(payload))

            elapsed = time.monotonic() - tick_start
            time.sleep(max(0.0, interval - elapsed))
    except KeyboardInterrupt:
        print("\n[SIM] Stopped.")


# ── CLI ───────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Autonomous truck field simulator")
    parser.add_argument("--broker",  default=MQTT_BROKER, help="MQTT broker host")
    parser.add_argument("--port",    default=MQTT_PORT, type=int, help="MQTT broker port")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print JSON to stdout instead of publishing to MQTT")
    args = parser.parse_args()
    run(args.broker, args.port, args.dry_run)
