"""
Truck Sensor & SLAM Visualizer

Left panel : Rolling time-series for the 4 ultrasonic distance sensors (all trucks).
Right panel: Real-time SLAM occupancy grid + trajectory (first truck discovered).

SLAM pose estimation (IMU dead-reckoning):
  - Heading: complementary filter — gyro (yaw_rate) for short-term accuracy,
    magnetometer (mag_x/mag_y) for long-term drift correction.
  - Speed: cmd_pwm sets the baseline; acc_x corrects for acceleration/deceleration.
  - Occupancy grid updated via Bresenham ray tracing on each ultrasonic reading.
"""

import json
import math
import argparse
import threading
from collections import deque

import numpy as np
from scipy import ndimage as _ndi
import matplotlib
matplotlib.use("TkAgg")          # interactive backend — requires python3-tk
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import matplotlib.animation as animation

try:
    from skimage import measure as _sk_measure
    _HAS_SKIMAGE = True
except ImportError:
    _HAS_SKIMAGE = False

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

# SLAM IMU parameters
MAX_SPEED_CMS  = 25.0   # cm/s at 100 % PWM — used as dead-reckoning baseline
MAG_WEIGHT     =  0.05  # complementary filter: weight given to magnetometer heading
                         # (1 - MAG_WEIGHT) goes to gyro; higher → faster mag correction

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

# ── Angle helper ──────────────────────────────────────────────────────────────

def _blend_angles(a_deg, b_deg, weight_b):
    """Blend two angles using unit-vector averaging to handle wrap-around correctly."""
    a, b = math.radians(a_deg), math.radians(b_deg)
    x = (1.0 - weight_b) * math.cos(a) + weight_b * math.cos(b)
    y = (1.0 - weight_b) * math.sin(a) + weight_b * math.sin(b)
    return math.degrees(math.atan2(y, x)) % 360


# ── SLAM processor (single truck) ────────────────────────────────────────────

class SLAMProcessor:
    """
    Maintains an occupancy grid and trajectory for one truck.

    Pose estimation uses IMU dead-reckoning:
      - Heading: complementary filter combining yaw_rate (gyro) and
        mag_x/mag_y (magnetometer).  Gyro dominates short-term; mag corrects
        long-term drift.
      - Speed: cmd_pwm sets the baseline velocity; acc_x adjusts for
        acceleration and deceleration events.

    All grid/trajectory writes are protected by self.lock.
    """

    def __init__(self):
        self.grid       = OccupancyGrid(GRID_RESOLUTION, GRID_SIZE)
        self.trajectory = []   # (rel_x, rel_y) relative to first observed position
        self.heading    = 0.0
        self.start_x    = None
        self.start_y    = None
        self._x         = 0.0
        self._y         = 0.0
        self._prev_t_ms = None
        self.lock       = threading.Lock()

    def update(self, msg):
        t_ms = msg.get("t_ms", 0)

        # ── Heading: complementary filter (gyro + magnetometer) ──────────────
        mag_x    = float(msg.get("mag_x", 0.0))
        mag_y    = float(msg.get("mag_y", 0.0))
        yaw_rate = float(msg.get("yaw_rate", 0.0))   # deg/s

        if self._prev_t_ms is not None:
            dt_s = (t_ms - self._prev_t_ms) / 1000.0

            # Gyro prediction: integrate yaw rate
            heading_gyro = (self.heading + yaw_rate * dt_s) % 360

            if mag_x != 0.0 or mag_y != 0.0:
                # Magnetometer-derived heading (North = +Y → atan2(mag_x, mag_y))
                heading_mag = math.degrees(math.atan2(mag_x, mag_y)) % 360
                # Blend: gyro accurate short-term, mag corrects long-term drift
                self.heading = _blend_angles(heading_gyro, heading_mag, MAG_WEIGHT)
            else:
                self.heading = heading_gyro

            # ── Speed: cmd_pwm baseline + acc_x correction ───────────────────
            cmd_pwm = float(msg.get("cmd_pwm", 0))
            acc_x   = float(msg.get("acc_x", 0.0))   # m/s², forward axis

            # Baseline from motor command
            base_speed = (cmd_pwm / 100.0) * MAX_SPEED_CMS   # cm/s (signed)

            # Acceleration contribution for this tick (m/s² → cm/s)
            acc_contrib = acc_x * 100.0 * dt_s

            speed_cms = base_speed + acc_contrib

            # Update position
            angle    = math.radians(self.heading)
            self._x += speed_cms * dt_s * math.cos(angle)
            self._y += speed_cms * dt_s * math.sin(angle)

        else:
            # First message: initialise heading from magnetometer or compass field
            if mag_x != 0.0 or mag_y != 0.0:
                self.heading = math.degrees(math.atan2(mag_x, mag_y)) % 360
            else:
                self.heading = float(msg.get("compass") or msg.get("heading") or 0.0)

        self._prev_t_ms = t_ms

        # Fix origin on first message
        if self.start_x is None:
            self.start_x = self._x
            self.start_y = self._y

        rel_x = self._x - self.start_x
        rel_y = self._y - self.start_y

        sensors = {k: float(msg.get(k, SENSOR_MAX)) for k in SENSORS}

        with self.lock:
            self.grid.update(rel_x, rel_y, self.heading, sensors)
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

# ── Boundary estimation ───────────────────────────────────────────────────────

def _rdp_simplify(pts, epsilon):
    """Ramer-Douglas-Peucker polyline simplification. pts: (N,2) array."""
    if len(pts) < 3:
        return pts
    start, end = pts[0], pts[-1]
    seg = end - start
    seg_len = np.linalg.norm(seg)
    if seg_len == 0:
        dists = np.linalg.norm(pts - start, axis=1)
    else:
        dists = np.abs(np.cross(seg, start - pts)) / seg_len
    idx = int(np.argmax(dists))
    if dists[idx] > epsilon:
        left  = _rdp_simplify(pts[:idx + 1], epsilon)
        right = _rdp_simplify(pts[idx:],     epsilon)
        return np.vstack([left[:-1], right])
    return np.array([start, end])


def estimate_boundary(prob_map, resolution, origin_cell):
    """
    Generic room boundary estimation — makes no assumption about room shape.

    Algorithm:
      1. Threshold high-confidence occupied cells (wall hits).
      2. Dilate to bridge gaps between sparse ultrasonic hits.
      3. Fill interior holes to get a solid room footprint.
      4. Trace the outer contour (marching squares via skimage, or gradient fallback).
      5. Convert cell indices → world coordinates (cm).
      6. Simplify with RDP to reduce noise.

    Returns (x, y) world-coordinate arrays forming the boundary polygon,
    or None if not enough data has been collected yet.
    """
    occupied = prob_map > 0.60
    if occupied.sum() < 30:
        return None

    # Bridge gaps between sparse wall readings (~15 cm dilation)
    dil_px  = max(2, int(15.0 / resolution))
    dilated = _ndi.binary_dilation(occupied, iterations=dil_px)

    # Fill the room interior so the contour traces the outer hull
    filled  = _ndi.binary_fill_holes(dilated)

    # Trace the boundary
    if _HAS_SKIMAGE:
        contours = _sk_measure.find_contours(filled.astype(float), 0.5)
        if not contours:
            return None
        # Largest contour = outer room boundary
        contour = max(contours, key=len)          # shape (N, 2): [row, col]
        rows, cols = contour[:, 0], contour[:, 1]
    else:
        # Fallback: edge pixels via gradient magnitude
        sx   = _ndi.sobel(filled.astype(float), axis=1)
        sy   = _ndi.sobel(filled.astype(float), axis=0)
        edge = np.hypot(sx, sy) > 0.5
        rows, cols = np.where(edge)
        if len(rows) < 10:
            return None

    # Convert (row, col) → world (x, y) in cm
    x = (cols - origin_cell) * resolution
    y = (rows - origin_cell) * resolution

    pts = np.column_stack([x, y])

    # Simplify: ~2 cm tolerance keeps meaningful corners, drops pixel-level noise
    pts = _rdp_simplify(pts, epsilon=2.0 * resolution)

    # Close the polygon
    if not np.allclose(pts[0], pts[-1]):
        pts = np.vstack([pts, pts[0]])

    return pts[:, 0], pts[:, 1]


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
    (boundary_line,) = ax_slam.plot([], [], "-", color="darkorange",
                                    linewidth=2.0, label="est. boundary", zorder=4)
    heading_arrow = [None]   # mutable ref so the closure can replace it each frame
    ax_slam.legend(fontsize=7, loc="upper right")

    # Per-truck sensor line artists: {truck_id: {sensor_key: Line2D}}
    sensor_lines  = {}
    frame_counter = [0]   # mutable counter for boundary recompute cadence

    def update(_frame):
        artists = []
        trucks  = dict(listener.trucks)
        frame_counter[0] += 1

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

                # Recompute boundary every 10 frames (~5 s at 500 ms interval)
                if frame_counter[0] % 10 == 0:
                    result = estimate_boundary(
                        prob, GRID_RESOLUTION, slam.grid.origin)
                    if result is not None:
                        boundary_line.set_data(result[0], result[1])
                artists.append(boundary_line)

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
