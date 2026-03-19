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
import importlib

try:
    import paho.mqtt.client as mqtt
    MQTT_AVAILABLE = True
except ImportError:
    print("[WARN] paho-mqtt not installed. Run: pip install paho-mqtt")
    MQTT_AVAILABLE = False

try:
    import matplotlib.pyplot as plt
    import matplotlib.animation as animation
    import matplotlib.patches as mpatches
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    print("[WARN] matplotlib not installed. Run: pip install matplotlib")
    MATPLOTLIB_AVAILABLE = False

# ── Field & simulation constants ──────────────────────────────────────────────

FIELD_WIDTH  = 120.0   # cm
FIELD_HEIGHT = 240.0   # cm

TRUCK_SPEED       = 1    # cm per tick  (= 10 cm/s at TICK_RATE_HZ=5)
TURN_ANGLE        = 15.0   # degrees per tick when turning
SENSOR_MAX_RANGE  = 750.0  # cm  (ultrasound max reading)
SENSOR_NOISE_CM   = 5    # ±cm random noise on each sensor
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


class EmbeddedExplore(ExploreStrategy):
    """Mirrors the ESP32 embedded control loop (z_main_control_loop.cpp).

    State machine: EXPLORE → RECOVER → EXPLORE
    Sensors are filtered with an exponential moving average.
    Steering uses corridor centering with a deadband and hysteresis.
    Recovery backs up then turns away from the closer wall.
    """

    # ── Config (mirrors ControlConfig) ────────────────────────────────────────
    FRONT_STOP_DIST    = 12.0   # cm  — emergency stop
    FRONT_SLOW_DIST    = 20.0   # cm  — reduce speed
    SIDE_MIN_DIST      =  6.0   # cm  — side collision avoidance
    CENTER_DEADBAND    =  3.0   # cm  — corridor centering tolerance
    STEER_HOLD         =  3     # consecutive ticks before steer change takes effect
    US_ALPHA           =  0.35  # EMA smoothing factor for ultrasonics
    STUCK_FRONT_MS     =  700   # ms front must be blocked before recovery
    RECOVER_REVERSE_MS =  500   # ms spent reversing
    RECOVER_TURN_MS    =  600   # ms spent turning after reverse

    def __init__(self):
        self.state = "EXPLORE"
        self._filt = {"ul": None, "ur": None, "uf": None, "ub": None}
        self._pending_steer = "STRAIGHT"
        self._steer_count = 0
        self._committed_steer = "STRAIGHT"
        self._front_blocked_since_ms = None
        self._recover_start_ms = None

    # ── Internal helpers ───────────────────────────────────────────────────────

    def _filter(self, sensors):
        for k in self._filt:
            prev = self._filt[k]
            self._filt[k] = sensors[k] if prev is None else (
                self.US_ALPHA * sensors[k] + (1 - self.US_ALPHA) * prev
            )
        return dict(self._filt)

    def _hysteresis(self, steer):
        """Return committed steer only after STEER_HOLD consecutive identical commands."""
        if steer == self._pending_steer:
            self._steer_count += 1
        else:
            self._pending_steer = steer
            self._steer_count = 1
        if self._steer_count >= self.STEER_HOLD:
            self._committed_steer = steer
        return self._committed_steer

    def _apply_motion(self, truck, speed_frac, steer):
        """Move truck one tick. speed_frac ∈ [-1, 1]; steer ∈ LEFT/STRAIGHT/RIGHT."""
        if steer == "LEFT":
            truck.heading = (truck.heading - TURN_ANGLE) % 360
        elif steer == "RIGHT":
            truck.heading = (truck.heading + TURN_ANGLE) % 360
        dist = TRUCK_SPEED * speed_frac
        angle = math.radians(truck.heading)
        nx = truck.x + dist * math.cos(angle)
        ny = truck.y + dist * math.sin(angle)
        truck.x = max(1.0, min(FIELD_WIDTH  - 1.0, nx))
        truck.y = max(1.0, min(FIELD_HEIGHT - 1.0, ny))

    # ── Strategy entry point ───────────────────────────────────────────────────

    def step(self, truck, sensors):
        f = self._filter(sensors)
        t_ms = truck.t_ms
        front_blocked = f["uf"] < self.FRONT_STOP_DIST

        if self.state == "EXPLORE":
            # Stuck detection — transition to RECOVER after STUCK_FRONT_MS
            if front_blocked:
                if self._front_blocked_since_ms is None:
                    self._front_blocked_since_ms = t_ms
                elif (t_ms - self._front_blocked_since_ms) >= self.STUCK_FRONT_MS:
                    self.state = "RECOVER"
                    self._recover_start_ms = t_ms
                    self._front_blocked_since_ms = None
                    return
            else:
                self._front_blocked_since_ms = None

            # Speed — priority: front distance
            if front_blocked:
                speed_frac = 0.0
            elif f["uf"] < self.FRONT_SLOW_DIST:
                speed_frac = 80 / 90
            else:
                speed_frac = 1.0

            # Steer — priority: front blocked > side avoidance > corridor centering
            if front_blocked:
                steer = "RIGHT" if f["ul"] < f["ur"] else "LEFT"
            elif f["ul"] < self.SIDE_MIN_DIST:
                steer = "RIGHT"
                speed_frac = min(speed_frac, 80 / 90)
            elif f["ur"] < self.SIDE_MIN_DIST:
                steer = "LEFT"
                speed_frac = min(speed_frac, 80 / 90)
            else:
                center_error = f["ur"] - f["ul"]
                if center_error > self.CENTER_DEADBAND:
                    steer = "RIGHT"
                elif center_error < -self.CENTER_DEADBAND:
                    steer = "LEFT"
                else:
                    steer = "STRAIGHT"

            steer = self._hysteresis(steer)
            self._apply_motion(truck, speed_frac, steer)

        elif self.state == "RECOVER":
            elapsed = t_ms - self._recover_start_ms
            if elapsed < self.RECOVER_REVERSE_MS:
                self._apply_motion(truck, -80 / 90, "STRAIGHT")
            elif elapsed < self.RECOVER_REVERSE_MS + self.RECOVER_TURN_MS:
                steer = "RIGHT" if f["ul"] < f["ur"] else "LEFT"
                self._apply_motion(truck, 80 / 90, steer)
            else:
                self.state = "EXPLORE"
                self._apply_motion(truck, 1.0, "STRAIGHT")


# ── Truck state ───────────────────────────────────────────────────────────────

START_MARGIN = 20.0   # cm from border — truck won't start closer than this


class Truck:
    def __init__(self, strategy=None):
        self.x = random.uniform(START_MARGIN, FIELD_WIDTH  - START_MARGIN)
        self.y = random.uniform(START_MARGIN, FIELD_HEIGHT - START_MARGIN)
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


# ── Real-time visualizer ──────────────────────────────────────────────────────

class SimVisualizer:
    """Matplotlib window showing room boundary, obstacles, and live truck path.

    Thread-safe: the sim thread calls update_pos(); matplotlib's FuncAnimation
    reads it on the main thread.
    """

    REFRESH_MS = 150   # animation poll interval

    def __init__(self, field_w, field_h, obstacles):
        self._xs = []
        self._ys = []
        self._lock = threading.Lock()

        fig_h = max(5, field_h / 30)
        fig_w = max(3, field_w / 30)
        self._fig, self._ax = plt.subplots(figsize=(fig_w, fig_h))
        ax = self._ax

        ax.set_xlim(-10, field_w + 10)
        ax.set_ylim(-10, field_h + 10)
        ax.set_aspect("equal")
        ax.set_xlabel("x (cm)")
        ax.set_ylabel("y (cm)")
        ax.set_title("Truck Simulator — Live Path")

        # Room boundary in red
        ax.add_patch(mpatches.Rectangle(
            (0, 0), field_w, field_h,
            linewidth=2, edgecolor="red", facecolor="none",
        ))

        # Obstacles in red (lighter fill so the path shows through)
        for ox, oy, ow, oh in obstacles:
            ax.add_patch(mpatches.Rectangle(
                (ox, oy), ow, oh,
                linewidth=1.5, edgecolor="red", facecolor="#ffcccc",
            ))

        # Path (dashed line) and current-position dot
        self._line, = ax.plot([], [], color="steelblue", linestyle="--",
                              linewidth=1, alpha=0.7, label="path")
        self._dot,  = ax.plot([], [], "o", color="steelblue", markersize=6)

        ax.legend(loc="upper right", fontsize=8)

        self._anim = animation.FuncAnimation(
            self._fig, self._draw_frame, interval=self.REFRESH_MS, blit=True,
        )

    def update_pos(self, x, y):
        """Called by the simulation thread after each tick."""
        with self._lock:
            self._xs.append(x)
            self._ys.append(y)

    def _draw_frame(self, _frame):
        with self._lock:
            xs = list(self._xs)
            ys = list(self._ys)
        self._line.set_data(xs, ys)
        if xs:
            self._dot.set_data([xs[-1]], [ys[-1]])
        return self._line, self._dot

    def show(self):
        """Block the calling thread (must be main thread) until window is closed."""
        plt.tight_layout()
        plt.show()


# ── Main loop ─────────────────────────────────────────────────────────────────

STRATEGIES = {
    "reactive": ReactiveExplore,
    "embedded": EmbeddedExplore,
}


def run(broker, port, dry_run, strategy_name, visualize=False, no_obstacles=False):
    global OBSTACLES
    if no_obstacles:
        OBSTACLES = []

    publisher = MQTTPublisher(broker, port) if not dry_run else None
    truck = Truck(strategy=STRATEGIES[strategy_name]())
    interval = 1.0 / TICK_RATE_HZ

    print(f"[SIM] Field {FIELD_WIDTH}x{FIELD_HEIGHT} cm  |  "
          f"truck starts at ({truck.x:.0f},{truck.y:.0f})  heading={truck.heading:.0f}°  "
          f"strategy={strategy_name}")
    print(f"[SIM] Publishing every {interval*1000:.0f} ms  (Ctrl-C to stop)\n")

    viz = SimVisualizer(FIELD_WIDTH, FIELD_HEIGHT, OBSTACLES) if visualize else None

    def sim_loop():
        last_pos_print = time.monotonic()
        try:
            while True:
                tick_start = time.monotonic()
                truck.step()
                payload = truck.payload()

                if viz:
                    viz.update_pos(truck.x, truck.y)

                if publisher:
                    publisher.publish(MQTT_TOPIC, payload)
                else:
                    print(json.dumps(payload))

                if tick_start - last_pos_print >= 1.0:
                    print(f"[POS] t={truck.t_ms/1000:.1f}s  x={truck.x:.1f} cm  "
                          f"y={truck.y:.1f} cm  heading={truck.heading:.1f}°")
                    last_pos_print = tick_start

                elapsed = time.monotonic() - tick_start
                time.sleep(max(0.0, interval - elapsed))
        except KeyboardInterrupt:
            print("\n[SIM] Stopped.")

    if viz:
        # Matplotlib must own the main thread; simulation runs as a daemon thread.
        t = threading.Thread(target=sim_loop, daemon=True)
        t.start()
        viz.show()   # blocks until window is closed
    else:
        sim_loop()


# ── CLI ───────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Autonomous truck field simulator")
    parser.add_argument("--broker",  default=MQTT_BROKER, help="MQTT broker host")
    parser.add_argument("--port",    default=MQTT_PORT, type=int, help="MQTT broker port")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print JSON to stdout instead of publishing to MQTT")
    parser.add_argument("--strategy", default="embedded", choices=STRATEGIES,
                        help="Explore strategy: embedded (default) or reactive")
    parser.add_argument("--visualize", action="store_true",
                        help="Open a real-time matplotlib window showing the truck path")
    parser.add_argument("--no-obstacles", action="store_true",
                        help="Remove all obstacles from the field")
    args = parser.parse_args()

    if args.visualize and not MATPLOTLIB_AVAILABLE:
        print("[ERROR] --visualize requires matplotlib. Run: pip install matplotlib")
        raise SystemExit(1)

    run(args.broker, args.port, args.dry_run, args.strategy, args.visualize,
        args.no_obstacles)
