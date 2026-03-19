"""
Truck Sensor & SLAM Visualizer

Left panel : Rolling time-series for the 4 ultrasonic distance sensors (all trucks).
Right panel: Real-time SLAM occupancy grid + trajectory (first truck discovered).

SLAM approach:
  - Occupancy grid with log-odds updates.
  - Bresenham ray tracing marks cells as free (along ray) or occupied (endpoint).
  - Pose comes from ground-truth x/y/heading in payload (simulator) or is
    dead-reckoned from compass + cmd_pwm for the real truck.
"""

import json
import math
import argparse
import threading
from collections import deque

import numpy as np
import matplotlib
matplotlib.use("TkAgg")          # interactive backend — requires python3-tk
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import matplotlib.animation as animation

try:
    import paho.mqtt.client as mqtt
except ImportError:
    raise SystemExit("[ERROR] paho-mqtt not installed. Run: pip install paho-mqtt")

# ── Constants ─────────────────────────────────────────────────────────────────

MQTT_BROKER    = "192.168.2.2"
MQTT_PORT      = 1883
TOPIC_WILDCARD = "+/telemetry"

SENSOR_MAX     = 250.0   # cm — sensor chart y-axis ceiling
WINDOW         = 60      # rolling readings per sensor per truck

STOP_DIST      = 12.0    # cm — red threshold line on sensor charts
SLOW_DIST      = 20.0    # cm — orange threshold line

SENSORS        = ["uf", "ub", "ul", "ur"]
SENSOR_LABELS  = {"uf": "Front (UF)", "ub": "Back (UB)",
                  "ul": "Left (UL)",  "ur": "Right (UR)"}

TRUCK_COLORS   = ["tab:blue", "tab:orange", "tab:green", "tab:purple", "tab:red"]

# SLAM grid parameters
GRID_RESOLUTION = 2.0    # cm per cell
GRID_SIZE       = 400    # cells — covers 800 × 800 cm around the start position
L_OCC           =  2.0   # log-odds added when a cell is hit by a sensor ray
L_FREE          = -0.3   # log-odds subtracted when a ray passes through a cell
L_MAX           =  10.0  # clamp ceiling
L_MIN           = -10.0  # clamp floor

# ── Bresenham ray iterator ────────────────────────────────────────────────────

def bresenham(x0, y0, x1, y1):
    """Yield every integer grid cell along the line from (x0,y0) to (x1,y1)."""
    dx, dy = abs(x1 - x0), abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx - dy
    while True:
        yield x0, y0
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 > -dy:
            err -= dy
            x0  += sx
        if e2 < dx:
            err += dx
            y0  += sy

# ── Occupancy grid ────────────────────────────────────────────────────────────

class OccupancyGrid:
    """
    2-D log-odds occupancy grid.
    origin cell = (GRID_SIZE//2, GRID_SIZE//2) → world (0, 0).
    Row index grows upward (origin='lower' in imshow).
    """

    def __init__(self, resolution, size):
        self.res      = resolution
        self.size     = size
        self.origin   = size // 2
        self.log_odds = np.zeros((size, size), dtype=np.float32)

    def _to_cell(self, x, y):
        col = int(round(x / self.res)) + self.origin
        row = int(round(y / self.res)) + self.origin
        return col, row

    def _in_bounds(self, col, row):
        return 0 <= col < self.size and 0 <= row < self.size

    def update(self, x, y, heading_deg, sensors):
        """
        Integrate one pose observation.
        sensors: dict with keys uf/ub/ul/ur, values in cm.
        """
        for offset_deg, key in [(0, "uf"), (180, "ub"), (270, "ul"), (90, "ur")]:
            dist  = sensors.get(key, SENSOR_MAX)
            angle = math.radians(heading_deg + offset_deg)
            ex    = x + dist * math.cos(angle)
            ey    = y + dist * math.sin(angle)

            c0, r0 = self._to_cell(x, y)
            c1, r1 = self._to_cell(ex, ey)

            # Max-range reading → no reliable endpoint; mark free along ray only
            hit = dist < SENSOR_MAX - 1.0

            cells = list(bresenham(c0, r0, c1, r1))

            # Free cells along the ray (skip the endpoint)
            for c, r in cells[:-1]:
                if self._in_bounds(c, r):
                    self.log_odds[r, c] = max(L_MIN, self.log_odds[r, c] + L_FREE)

            # Occupied endpoint
            if hit and self._in_bounds(c1, r1):
                self.log_odds[r1, c1] = min(L_MAX, self.log_odds[r1, c1] + L_OCC)

    def probability_map(self):
        """Convert log-odds to probability [0=free, 0.5=unknown, 1=occupied]."""
        return (1.0 / (1.0 + np.exp(-self.log_odds))).astype(np.float32)

    def extent_cm(self):
        """Matplotlib imshow extent: [left, right, bottom, top]."""
        half = self.origin * self.res
        return [-half, half, -half, half]

# ── Per-truck sensor history ──────────────────────────────────────────────────

class TruckState:
    def __init__(self, truck_id, color):
        self.truck_id = truck_id
        self.color    = color
        self.history  = {s: deque(maxlen=WINDOW) for s in SENSORS}
        self.lock     = threading.Lock()

    def update(self, msg):
        with self.lock:
            for s in SENSORS:
                if s in msg:
                    self.history[s].append(float(msg[s]))

# ── SLAM processor (single truck) ────────────────────────────────────────────

class SLAMProcessor:
    """
    Maintains an occupancy grid and trajectory for one truck.
    Pose source priority: x/y/heading fields in payload → dead-reckoning.
    All grid/trajectory writes are protected by self.lock.
    """

    def __init__(self):
        self.grid       = OccupancyGrid(GRID_RESOLUTION, GRID_SIZE)
        self.trajectory = []        # (rel_x, rel_y) relative to first observed position
        self.heading    = 0.0
        self.start_x    = None
        self.start_y    = None
        self._x         = 0.0
        self._y         = 0.0
        self._prev_t_ms = None
        self.lock       = threading.Lock()

    def update(self, msg):
        t_ms    = msg.get("t_ms", 0)
        heading = float(msg.get("compass") or msg.get("heading") or 0.0)
        self.heading = heading

        # ── Pose
        if "x" in msg and "y" in msg:
            self._x = float(msg["x"])
            self._y = float(msg["y"])
        else:
            if self._prev_t_ms is not None:
                dt_s      = (t_ms - self._prev_t_ms) / 1000.0
                cmd_pwm   = float(msg.get("cmd_pwm", 0))
                speed_cms = (abs(cmd_pwm) / 100.0) * 25.0
                if cmd_pwm < 0:
                    speed_cms = -speed_cms
                angle   = math.radians(heading)
                self._x += speed_cms * dt_s * math.cos(angle)
                self._y += speed_cms * dt_s * math.sin(angle)
        self._prev_t_ms = t_ms

        # Fix origin on first message
        if self.start_x is None:
            self.start_x = self._x
            self.start_y = self._y

        rel_x = self._x - self.start_x
        rel_y = self._y - self.start_y

        sensors = {k: float(msg.get(k, SENSOR_MAX)) for k in SENSORS}

        with self.lock:
            self.grid.update(rel_x, rel_y, heading, sensors)
            self.trajectory.append((rel_x, rel_y))

# ── MQTT listener ─────────────────────────────────────────────────────────────

class MQTTListener:
    def __init__(self, broker, port):
        self.trucks     = {}     # truck_id → TruckState
        self.slam       = None   # SLAMProcessor — first truck only
        self.slam_id    = None
        self._color_idx = 0
        self._lock      = threading.Lock()

        try:
            self._client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
        except AttributeError:
            self._client = mqtt.Client()

        self._client.on_connect    = self._on_connect
        self._client.on_message    = self._on_message
        self._client.on_disconnect = self._on_disconnect

        try:
            self._client.connect(broker, port, keepalive=60)
            self._client.loop_start()
        except Exception as e:
            raise SystemExit(f"[ERROR] Cannot connect to broker at {broker}:{port} — {e}")

    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            client.subscribe(TOPIC_WILDCARD)
            print(f"[MQTT] Connected — subscribed to '{TOPIC_WILDCARD}'")
        else:
            print(f"[MQTT] Connection failed (rc={rc})")

    def _on_disconnect(self, client, userdata, rc):
        print(f"[MQTT] Disconnected (rc={rc})")

    def _on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode())
        except (json.JSONDecodeError, UnicodeDecodeError):
            return
        print(f"[MSG] {msg.topic}  {json.dumps(payload)}")

        truck_id = payload.get("truck_id") or msg.topic.split("/")[0]

        with self._lock:
            if truck_id not in self.trucks:
                color = TRUCK_COLORS[self._color_idx % len(TRUCK_COLORS)]
                self._color_idx += 1
                self.trucks[truck_id] = TruckState(truck_id, color)
                print(f"[VIZ] New truck: {truck_id} ({color})")

                if self.slam is None:
                    self.slam    = SLAMProcessor()
                    self.slam_id = truck_id
                    print(f"[SLAM] Tracking: {truck_id}")

        self.trucks[truck_id].update(payload)
        if truck_id == self.slam_id:
            self.slam.update(payload)

# ── Visualization ─────────────────────────────────────────────────────────────

def run_visualizer(listener: MQTTListener):
    fig = plt.figure(figsize=(16, 8))
    fig.suptitle("Truck Sensor & SLAM Visualizer", fontsize=13)
    gs  = gridspec.GridSpec(2, 3, figure=fig, wspace=0.4, hspace=0.45)

    # ── Sensor subplots (left 2×2)
    ax_sensors = {
        "uf": fig.add_subplot(gs[0, 0]),
        "ub": fig.add_subplot(gs[0, 1]),
        "ul": fig.add_subplot(gs[1, 0]),
        "ur": fig.add_subplot(gs[1, 1]),
    }
    for key, ax in ax_sensors.items():
        ax.set_title(SENSOR_LABELS[key], fontsize=9)
        ax.set_ylim(0, SENSOR_MAX)
        ax.set_xlim(0, WINDOW)
        ax.set_ylabel("cm", fontsize=8)
        ax.set_xlabel("readings", fontsize=8)
        ax.tick_params(labelsize=7)
        ax.axhline(STOP_DIST, color="red",    linestyle="--", linewidth=1,
                   label=f"stop {STOP_DIST:.0f} cm")
        ax.axhline(SLOW_DIST, color="orange", linestyle="--", linewidth=1,
                   label=f"slow {SLOW_DIST:.0f} cm")
        ax.legend(fontsize=6, loc="upper right")
        ax.grid(True, alpha=0.3)

    # ── SLAM subplot (right column, spans both rows)
    ax_slam = fig.add_subplot(gs[:, 2])
    ax_slam.set_title("SLAM Map + Trajectory", fontsize=9)
    ax_slam.set_xlabel("x (cm)", fontsize=8)
    ax_slam.set_ylabel("y (cm)", fontsize=8)
    ax_slam.tick_params(labelsize=7)
    ax_slam.set_aspect("equal")

    half = GRID_SIZE * GRID_RESOLUTION / 2
    slam_extent = [-half, half, -half, half]

    slam_img = ax_slam.imshow(
        np.full((GRID_SIZE, GRID_SIZE), 0.5, dtype=np.float32),
        origin="lower", cmap="gray_r", vmin=0.0, vmax=1.0,
        extent=slam_extent, interpolation="nearest",
    )
    (traj_line,) = ax_slam.plot([], [], "--", color="tab:blue",
                                linewidth=1.2, label="trajectory", zorder=3)
    (pos_dot,)   = ax_slam.plot([], [], "o", color="tab:red",
                                markersize=7, zorder=5, label="position")
    heading_arrow = [None]   # mutable ref so the closure can replace it each frame
    ax_slam.legend(fontsize=7, loc="upper right")

    # Per-truck sensor line artists: {truck_id: {sensor_key: Line2D}}
    sensor_lines = {}

    def update(_frame):
        artists = []
        trucks  = dict(listener.trucks)

        # ── Sensor charts
        for truck_id, state in trucks.items():
            with state.lock:
                history = {s: list(state.history[s]) for s in SENSORS}

            if truck_id not in sensor_lines:
                sensor_lines[truck_id] = {}

            for key, ax in ax_sensors.items():
                data = history[key]
                if not data:
                    continue
                if key not in sensor_lines[truck_id]:
                    (line,) = ax.plot([], [], color=state.color,
                                      linewidth=1.5, label=truck_id)
                    ax.legend(fontsize=6, loc="upper right")
                    sensor_lines[truck_id][key] = line
                sensor_lines[truck_id][key].set_data(range(len(data)), data)
                artists.append(sensor_lines[truck_id][key])

        # ── SLAM map
        slam = listener.slam
        if slam is not None:
            with slam.lock:
                prob    = slam.grid.probability_map()
                traj    = list(slam.trajectory)
                heading = slam.heading

            slam_img.set_data(prob)
            artists.append(slam_img)

            if traj:
                tx, ty = zip(*traj)
                traj_line.set_data(tx, ty)
                pos_dot.set_data([tx[-1]], [ty[-1]])

                # Auto-zoom to visited area
                margin = 30
                ax_slam.set_xlim(min(tx) - margin, max(tx) + margin)
                ax_slam.set_ylim(min(ty) - margin, max(ty) + margin)

                # Heading arrow — remove previous, draw new
                if heading_arrow[0] is not None:
                    heading_arrow[0].remove()
                arrow_len = 12.0
                angle     = math.radians(heading)
                heading_arrow[0] = ax_slam.annotate(
                    "",
                    xy=(tx[-1] + arrow_len * math.cos(angle),
                        ty[-1] + arrow_len * math.sin(angle)),
                    xytext=(tx[-1], ty[-1]),
                    arrowprops=dict(arrowstyle="->", color="tab:red", lw=2),
                    zorder=6,
                )
                artists += [traj_line, pos_dot, heading_arrow[0]]

        return artists

    ani = animation.FuncAnimation(
        fig, update, interval=500, blit=False, cache_frame_data=False,
    )

    plt.tight_layout()
    plt.show()

# ── CLI ───────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Real-time ultrasonic sensor charts + SLAM map")
    parser.add_argument("--broker", default=MQTT_BROKER, help="MQTT broker host")
    parser.add_argument("--port",   default=MQTT_PORT,   type=int,
                        help="MQTT broker port")
    args = parser.parse_args()

    listener = MQTTListener(args.broker, args.port)
    run_visualizer(listener)
