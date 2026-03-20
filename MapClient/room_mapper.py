"""
room_mapper.py — Step-by-step room mapper for the simulated truck.

Step 1: Connect to MQTT broker, subscribe to the truck telemetry topic,
        and parse the sensor fields from each JSON message.
Step 2: Maintain a 10 m × 10 m occupancy grid (5 cm resolution) centred on
        the truck's starting position.
Step 3: Dead-reckon truck position from compass + gyroscope, update the
        occupancy grid from ultrasonic rays, and display the map live.
Step 4: Estimate the room boundary polygon from the occupied cells using
        binary dilation → contour tracing → RDP simplification.

Run
---
    python room_mapper.py
    python room_mapper.py --broker 192.168.2.2 --port 1883
    python room_mapper.py --resolution 10   # 10 cm cells
"""

import argparse
import json
import math
import threading

import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import matplotlib.patches as mpatches
import numpy as np
from scipy.ndimage import binary_dilation, binary_fill_holes
import paho.mqtt.client as mqtt

# ── MQTT configuration ────────────────────────────────────────────────────────

MQTT_BROKER = "192.168.2.2"
MQTT_PORT   = 1883
MQTT_TOPIC  = "TRUCK_SIM/telemetry"

FIELDS = [
    "truck_id", "seq", "t_ms",
    "ul", "ur", "uf", "ub",
    "gy_x", "gy_y", "gy_z",
    "compass",
    "mag_x", "mag_y", "mag_z",
    "acc_x", "acc_y", "acc_z",
]

# ── Grid defaults ─────────────────────────────────────────────────────────────

GRID_SIZE_CM  = 1000   # total side length in cm  (10 m)
GRID_RES_CM   = 5      # cell size in cm          (5 cm)
SENSOR_MAX_CM = 280.0  # distances above this are treated as no-wall detected

# ── Boundary estimation settings ──────────────────────────────────────────────

BOUNDARY_MIN_CELLS  = 20    # minimum occupied cells before attempting estimation
BOUNDARY_UPDATE_SEC = 2.0   # how often to re-estimate the boundary (seconds)
RDP_EPSILON_CM      = 15.0  # RDP simplification tolerance in cm


# ── OccupancyGrid ─────────────────────────────────────────────────────────────

class OccupancyGrid:
    """
    2-D log-odds occupancy grid centred on the truck's starting position.

    Coordinate convention
    ---------------------
    World frame  : origin = truck start, x = East (cm), y = North (cm)
    Grid indices : row = 0 at y_max (top), col = 0 at x_min (left)

    Cell values (log-odds)
    ----------------------
        0.0  → unknown
        > 0  → occupied (wall)
        < 0  → free
    """

    FREE     = -0.5
    OCCUPIED = +2.0
    CLAMP    =  6.0

    def __init__(self, size_cm: int = GRID_SIZE_CM, res_cm: int = GRID_RES_CM):
        self.res_cm  = res_cm
        self.size_cm = size_cm
        self.n_cells = size_cm // res_cm

        self.cells = np.zeros((self.n_cells, self.n_cells), dtype=np.float32)

        half = size_cm / 2.0
        self.x_min, self.y_min = -half, -half
        self.x_max, self.y_max =  half,  half

        print(
            f"[GRID] {self.n_cells}×{self.n_cells} cells  "
            f"({size_cm/100:.0f} m × {size_cm/100:.0f} m)  "
            f"resolution={res_cm} cm/cell"
        )

    # ── Coordinate helpers ────────────────────────────────────────────────────

    def world_to_cell(self, x_cm: float, y_cm: float) -> tuple[int, int]:
        if not (self.x_min <= x_cm < self.x_max and
                self.y_min <= y_cm < self.y_max):
            return -1, -1
        col = min(int((x_cm - self.x_min) / self.res_cm), self.n_cells - 1)
        row = min(int((self.y_max - y_cm) / self.res_cm), self.n_cells - 1)
        return row, col

    def cell_to_world(self, row: int, col: int) -> tuple[float, float]:
        x_cm = self.x_min + (col + 0.5) * self.res_cm
        y_cm = self.y_max - (row + 0.5) * self.res_cm
        return x_cm, y_cm

    def in_bounds(self, row: int, col: int) -> bool:
        return 0 <= row < self.n_cells and 0 <= col < self.n_cells

    # ── Ray update ────────────────────────────────────────────────────────────

    def update_ray(self, ox: float, oy: float,
                   heading_deg: float, offset_deg: float,
                   dist_cm: float) -> None:
        angle  = math.radians(heading_deg + offset_deg)
        dx, dy = math.cos(angle), math.sin(angle)

        step     = self.res_cm * 0.5
        n_steps  = int(dist_cm / step)
        wall_hit = dist_cm < SENSOR_MAX_CM

        prev_rc = (-1, -1)
        for i in range(n_steps):
            wx = ox + dx * i * step
            wy = oy + dy * i * step
            rc = self.world_to_cell(wx, wy)
            if rc == (-1, -1) or rc == prev_rc:
                continue
            r, c = rc
            self.cells[r, c] = max(-self.CLAMP, self.cells[r, c] + self.FREE)
            prev_rc = rc

        if wall_hit:
            wx = ox + dx * dist_cm
            wy = oy + dy * dist_cm
            r, c = self.world_to_cell(wx, wy)
            if self.in_bounds(r, c):
                self.cells[r, c] = min(self.CLAMP, self.cells[r, c] + self.OCCUPIED)

    # ── Image for display ─────────────────────────────────────────────────────

    def to_image(self) -> np.ndarray:
        img = np.full((self.n_cells, self.n_cells, 3), 50, dtype=np.uint8)
        img[self.cells < 0] = [220, 220, 220]   # free  → light grey
        img[self.cells > 0] = [255, 100,  30]   # wall  → orange
        return img

    # ── Occupied mask ─────────────────────────────────────────────────────────

    def occupied_mask(self) -> np.ndarray:
        """Boolean mask: True where cells are occupied (wall hits)."""
        return self.cells > 0


# ── BoundaryEstimator ─────────────────────────────────────────────────────────

class BoundaryEstimator:
    """
    Estimates the room boundary polygon from the occupancy grid.

    Pipeline
    --------
    1. Threshold grid → binary occupied mask
    2. Dilate to connect nearby wall hits (fill small gaps)
    3. Fill interior holes so the interior is solid
    4. Trace the outer contour using a simple border-following algorithm
    5. Simplify the contour with Ramer–Douglas–Peucker (RDP)
    6. Convert cell indices back to world coordinates (cm)
    """

    def __init__(self, grid: OccupancyGrid,
                 rdp_epsilon_cm: float = RDP_EPSILON_CM):
        self._grid      = grid
        self._rdp_eps   = rdp_epsilon_cm / grid.res_cm   # convert to cells

    def estimate(self) -> np.ndarray | None:
        """
        Returns an (N, 2) array of world-frame (x, y) polygon vertices in cm,
        or None if there are not enough occupied cells yet.
        """
        mask = self._grid.occupied_mask()
        if mask.sum() < BOUNDARY_MIN_CELLS:
            return None

        # 1. Dilate to bridge gaps between sparse wall hits
        dilated = binary_dilation(mask, iterations=3)

        # 2. Fill enclosed free space so the shape is solid
        filled = binary_fill_holes(dilated)

        # 3. Trace outer contour
        contour = self._trace_contour(filled)
        if contour is None or len(contour) < 4:
            return None

        # 4. RDP simplification
        simplified = self._rdp(contour, self._rdp_eps)
        if len(simplified) < 3:
            return None

        # 5. Convert cell indices → world coordinates
        pts = np.array([
            self._grid.cell_to_world(int(r), int(c))
            for r, c in simplified
        ])
        return pts

    # ── Contour tracing (Moore neighbourhood border following) ────────────────

    @staticmethod
    def _trace_contour(mask: np.ndarray) -> list | None:
        """
        Trace the outer boundary of the largest connected foreground region
        using a simple clockwise Moore-neighbourhood walk.
        Returns a list of (row, col) tuples, or None if no boundary found.
        """
        rows, cols = np.where(mask)
        if len(rows) == 0:
            return None

        # Start from the topmost-leftmost occupied cell
        start = (int(rows[0]), int(cols[0]))

        contour = []
        # 8-connected neighbour directions (clockwise from top-left)
        dirs = [(-1,-1),(-1,0),(-1,1),(0,1),(1,1),(1,0),(1,-1),(0,-1)]

        pos  = start
        # Entry direction: we arrive from the left, so previous was from dir 7 (left)
        entry_dir = 6   # start looking from bottom-left

        for _ in range(mask.size):
            contour.append(pos)
            r, c = pos
            # Search clockwise from (entry_dir + 5) % 8
            found = False
            for k in range(8):
                d = (entry_dir + 5 + k) % 8
                dr, dc = dirs[d]
                nr, nc = r + dr, c + dc
                if (0 <= nr < mask.shape[0] and
                        0 <= nc < mask.shape[1] and
                        mask[nr, nc]):
                    entry_dir = (d + 4) % 8   # reverse direction
                    pos = (nr, nc)
                    found = True
                    break
            if not found:
                break
            if len(contour) > 2 and pos == start:
                break

        return contour if len(contour) >= 4 else None

    # ── Ramer–Douglas–Peucker ─────────────────────────────────────────────────

    @staticmethod
    def _rdp(points: list, epsilon: float) -> list:
        """Recursively simplify a polyline using RDP."""
        if len(points) < 3:
            return points

        pts = np.array(points, dtype=float)
        start, end = pts[0], pts[-1]

        # Perpendicular distances from each point to the start-end line
        d = end - start
        norm = np.linalg.norm(d)
        if norm < 1e-9:
            dists = np.linalg.norm(pts - start, axis=1)
        else:
            # Cross product / norm gives perpendicular distance
            dists = np.abs(
                (pts - start) @ np.array([-d[1], d[0]]) / norm
            )

        idx = int(np.argmax(dists))
        if dists[idx] > epsilon:
            left  = BoundaryEstimator._rdp(points[:idx+1], epsilon)
            right = BoundaryEstimator._rdp(points[idx:],   epsilon)
            return left[:-1] + right
        else:
            return [points[0], points[-1]]


# ── AutoCalibrator ────────────────────────────────────────────────────────────

class AutoCalibrator:
    """
    Automatically calibrates DeadReckoning parameters from incoming telemetry.

    Calibration pipeline (runs once after MIN_SAMPLES messages)
    -----------------------------------------------------------
    1. gy_threshold : Otsu's method on the |gy_z| histogram — finds the optimal
                      split between the two modes (DRIVE=low, RECOVER=high).
    2. forward_speed: In DRIVE samples where the compass is stable (straight
                      line) and uf is strictly decreasing (heading toward wall),
                      speed ≈ -Δuf/Δt.  Median of all valid measurements.
    3. recover_speed: Fixed at RECOVER_FRACTION × forward_speed (truck is
                      spinning mostly, not translating).

    Parameters
    ----------
    dr           : DeadReckoning instance to update after calibration
    min_samples  : number of messages to collect before calibrating
    """

    MIN_SAMPLES       = 40     # messages to collect before calibrating
    RECOVER_FRACTION  = 0.20   # recover_speed = this × forward_speed
    COMPASS_STABLE_DEG = 5.0   # max heading change between samples to count as straight

    def __init__(self, dr: "DeadReckoning", min_samples: int = MIN_SAMPLES):
        self._dr          = dr
        self._min_samples = min_samples
        self._samples: list[dict] = []
        self.done         = False

    def feed(self, data: dict) -> None:
        """Feed one telemetry message. Calibrates once enough data is collected."""
        if self.done:
            return
        self._samples.append(data)
        if len(self._samples) >= self._min_samples:
            self._calibrate()

    def _calibrate(self) -> None:
        gy_abs = np.array([abs(d["gy_z"]) for d in self._samples])

        # ── Step 1: Otsu threshold on |gy_z| ─────────────────────────────────
        gy_threshold = self._otsu(gy_abs)

        # ── Step 2: forward speed from uf decrease in DRIVE+straight segments ─
        speeds = []
        for i in range(1, len(self._samples)):
            prev, cur = self._samples[i - 1], self._samples[i]
            # Must both be DRIVE state
            if abs(prev["gy_z"]) > gy_threshold or abs(cur["gy_z"]) > gy_threshold:
                continue
            # Compass must be stable (heading straight)
            d_compass = abs(cur["compass"] - prev["compass"])
            if d_compass > 180:
                d_compass = 360 - d_compass
            if d_compass > self.COMPASS_STABLE_DEG:
                continue
            # uf must be strictly decreasing (approaching wall)
            dt = (cur["t_ms"] - prev["t_ms"]) / 1000.0
            if dt <= 0 or cur["uf"] >= prev["uf"]:
                continue
            v = (prev["uf"] - cur["uf"]) / dt
            if 5.0 < v < 200.0:   # sanity bounds (cm/s)
                speeds.append(v)

        if speeds:
            forward_speed = float(np.median(speeds))
        else:
            forward_speed = self._dr.forward_speed   # keep current default
            print("[CAL] Not enough straight-line samples for speed — keeping default")

        recover_speed = forward_speed * self.RECOVER_FRACTION

        # ── Apply to DeadReckoning ────────────────────────────────────────────
        self._dr.gy_threshold  = gy_threshold
        self._dr.forward_speed = forward_speed
        self._dr.recover_speed = recover_speed
        self.done = True

        print(
            f"[CAL] Calibration complete from {len(self._samples)} samples:\n"
            f"      gy_threshold  = {gy_threshold:.2f} deg/s\n"
            f"      forward_speed = {forward_speed:.1f} cm/s\n"
            f"      recover_speed = {recover_speed:.1f} cm/s"
        )

    @staticmethod
    def _otsu(values: np.ndarray) -> float:
        """
        1-D Otsu threshold: find the value that minimises within-class variance.
        Returns the threshold between the two modes.
        """
        hist, edges = np.histogram(values, bins=64)
        centres = (edges[:-1] + edges[1:]) / 2.0
        total   = hist.sum()
        if total == 0:
            return float(values.mean())

        best_var, best_t = -1.0, centres[0]
        w0, sum0 = 0.0, 0.0

        for i, (h, c) in enumerate(zip(hist, centres)):
            w0   += h
            sum0 += h * c
            w1    = total - w0
            if w0 == 0 or w1 == 0:
                continue
            mu0 = sum0 / w0
            mu1 = (hist @ centres - sum0) / w1
            var_between = (w0 / total) * (w1 / total) * (mu0 - mu1) ** 2
            if var_between > best_var:
                best_var, best_t = var_between, c

        return float(best_t)


# ── DeadReckoning ─────────────────────────────────────────────────────────────

class DeadReckoning:
    """
    Estimate truck position from compass + gyroscope using a complementary filter.

    Heading
    -------
    Raw compass has Gaussian noise that causes direction to jitter each tick.
    The gyroscope (gy_z) is accurate short-term but drifts long-term.
    A complementary filter fuses both:
        heading = alpha * (heading_prev + gy_z * dt) + (1-alpha) * compass
    alpha close to 1 → trust gyro for fast changes; compass corrects slow drift.

    Speed model
    -----------
    acc_x ≈ 0 at constant speed — unreliable for integration.
    State is inferred from |gy_z|:
      DRIVE   → |gy_z| ≤ gy_threshold  → forward at forward_speed
      RECOVER → |gy_z| > gy_threshold  → truck is mostly spinning while
                reversing; net linear displacement is small → recover_speed
    """

    ALPHA = 0.95   # complementary filter weight for gyroscope (0–1)

    def __init__(self, forward_speed: float = 70.0,
                 recover_speed: float = 15.0,
                 gy_threshold:  float = 15.0):
        self.forward_speed = forward_speed
        self.recover_speed = recover_speed
        self.gy_threshold  = gy_threshold
        self.x        = 0.0
        self.y        = 0.0
        self._heading = None   # filtered heading (degrees)
        self._last_t_ms = None

    def update(self, data: dict) -> tuple[float, float]:
        t_ms    = data["t_ms"]
        compass = data["compass"]
        gy_z    = data["gy_z"]

        if self._last_t_ms is None:
            self._last_t_ms = t_ms
            self._heading   = compass
            return self.x, self.y

        dt = (t_ms - self._last_t_ms) / 1000.0
        self._last_t_ms = t_ms
        if dt <= 0:
            return self.x, self.y

        # Complementary filter — handles 0°/360° wrap-around
        heading_gyro = (self._heading + gy_z * dt) % 360
        diff = compass - heading_gyro
        if diff >  180: diff -= 360
        if diff < -180: diff += 360
        self._heading = (heading_gyro + (1 - self.ALPHA) * diff) % 360

        heading_rad = math.radians(self._heading)
        speed = (-self.recover_speed if abs(gy_z) > self.gy_threshold
                 else self.forward_speed)

        self.x += speed * math.cos(heading_rad) * dt
        self.y += speed * math.sin(heading_rad) * dt
        return self.x, self.y


# ── MapVisualizer ─────────────────────────────────────────────────────────────

class MapVisualizer:
    """
    Live matplotlib display: occupancy grid + truck path + boundary polygon.
    """

    REFRESH_MS = 500

    def __init__(self, grid: OccupancyGrid, estimator: BoundaryEstimator):
        self._grid      = grid
        self._estimator = estimator
        self._lock      = threading.Lock()

        self._truck_x: float      = 0.0
        self._truck_y: float      = 0.0
        self._path_x:  list       = [0.0]
        self._path_y:  list       = [0.0]
        self._boundary: np.ndarray | None = None
        self._last_boundary_t: float      = 0.0

        half = grid.size_cm / 2.0
        self._fig, self._ax = plt.subplots(figsize=(8, 8))
        self._fig.patch.set_facecolor("#1a1a2e")
        self._ax.set_facecolor("#1a1a2e")
        self._ax.set_title("Room Mapper — occupancy grid + boundary",
                            color="white", fontsize=11)
        self._ax.set_xlabel("x  (cm)", color="white", fontsize=9)
        self._ax.set_ylabel("y  (cm)", color="white", fontsize=9)
        self._ax.tick_params(colors="white", labelsize=8)
        for spine in self._ax.spines.values():
            spine.set_edgecolor("#444")

        extent = [-half, half, -half, half]
        self._im = self._ax.imshow(
            grid.to_image(), origin="upper",
            extent=extent, interpolation="nearest",
        )
        self._ax.set_xlim(-half, half)
        self._ax.set_ylim(-half, half)
        self._ax.set_aspect("equal")

        (self._path_line,) = self._ax.plot(
            [], [], color="#00e5ff", linewidth=1.0, alpha=0.7, zorder=3,
        )
        (self._truck_dot,) = self._ax.plot(
            [], [], "o", color="#ff1744", markersize=7, zorder=4,
        )
        self._boundary_patch = None
        self._anim = None

    def push(self, x: float, y: float, t: float) -> None:
        """Called by MQTT thread each tick."""
        boundary = None
        if t - self._last_boundary_t >= BOUNDARY_UPDATE_SEC:
            boundary = self._estimator.estimate()
            self._last_boundary_t = t

        with self._lock:
            self._truck_x = x
            self._truck_y = y
            self._path_x.append(x)
            self._path_y.append(y)
            if boundary is not None:
                self._boundary = boundary

    def _draw_frame(self, _frame):
        with self._lock:
            tx, ty   = self._truck_x, self._truck_y
            px       = list(self._path_x)
            py       = list(self._path_y)
            boundary = self._boundary

        self._im.set_data(self._grid.to_image())
        self._path_line.set_data(px, py)
        self._truck_dot.set_data([tx], [ty])

        # Redraw boundary polygon
        if self._boundary_patch is not None:
            self._boundary_patch.remove()
            self._boundary_patch = None

        if boundary is not None and len(boundary) >= 3:
            closed = np.vstack([boundary, boundary[0]])
            self._boundary_patch = mpatches.Polygon(
                boundary, closed=True,
                linewidth=2.0, edgecolor="#00e676", facecolor="none",
                zorder=5,
            )
            self._ax.add_patch(self._boundary_patch)

    def show(self) -> None:
        self._anim = animation.FuncAnimation(
            self._fig, self._draw_frame,
            interval=self.REFRESH_MS, blit=False, cache_frame_data=False,
        )
        plt.tight_layout()
        plt.show()


# ── Parsing ───────────────────────────────────────────────────────────────────

def parse_telemetry(raw: str) -> dict | None:
    try:
        payload = json.loads(raw)
    except json.JSONDecodeError as e:
        print(f"[WARN] JSON parse error: {e}")
        return None

    data = {}
    for field in FIELDS:
        if field not in payload:
            print(f"[WARN] Missing field '{field}' — skipping message")
            return None
        data[field] = payload[field]

    return data


# ── MQTT callbacks ────────────────────────────────────────────────────────────

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[MQTT] Connected to {userdata['broker']}:{userdata['port']}")
        client.subscribe(MQTT_TOPIC)
        print(f"[MQTT] Subscribed to {MQTT_TOPIC}")
    else:
        print(f"[ERROR] Connection failed  rc={rc}")


def on_message(client, userdata, msg):
    grid = userdata["grid"]
    dr   = userdata["dead_reckoning"]
    cal  = userdata["calibrator"]
    viz  = userdata["viz"]

    data = parse_telemetry(msg.payload.decode())
    if data is None:
        return

    # Feed calibrator — it updates dr parameters once enough data is collected
    cal.feed(data)

    # Skip grid/position updates until calibration is done
    if not cal.done:
        remaining = cal._min_samples - len(cal._samples)
        print(f"[CAL] Collecting samples … {len(cal._samples)}/{cal._min_samples}")
        return

    x, y    = dr.update(data)
    heading = data["compass"]
    t_sec   = data["t_ms"] / 1000.0

    for offset, dist in [
        (  0, data["uf"]),
        (180, data["ub"]),
        (270, data["ul"]),
        ( 90, data["ur"]),
    ]:
        grid.update_ray(x, y, heading, offset, dist)

    viz.push(x, y, t_sec)

    print(
        f"seq={data['seq']:5d}  t={t_sec:.1f}s  "
        f"pos=({x:6.1f}, {y:6.1f}) cm  "
        f"hdg={heading:6.1f}°  gy_z={data['gy_z']:+6.2f} deg/s  "
        f"uf={data['uf']:5.1f}  ul={data['ul']:5.1f}  "
        f"ur={data['ur']:5.1f}  ub={data['ub']:5.1f} cm"
    )


# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Room mapper — Step 4: boundary estimation",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--broker",     default=MQTT_BROKER,       help="MQTT broker host")
    parser.add_argument("--port",       default=MQTT_PORT,    type=int,   help="MQTT broker port")
    parser.add_argument("--size",       default=GRID_SIZE_CM, type=int,   help="Grid side length (cm)")
    parser.add_argument("--resolution", default=GRID_RES_CM,  type=int,   help="Cell size (cm)")
    parser.add_argument("--speed",          default=70.0,  type=float, help="Nominal forward speed (cm/s)")
    parser.add_argument("--recover-speed",  default=15.0,  type=float, help="Net speed during recovery/turning (cm/s)")
    parser.add_argument("--gy-thresh",      default=15.0,  type=float, help="Yaw-rate threshold for recovery detection (deg/s)")
    parser.add_argument("--rdp",        default=RDP_EPSILON_CM, type=float, help="RDP simplification tolerance (cm)")
    args = parser.parse_args()

    grid      = OccupancyGrid(size_cm=args.size, res_cm=args.resolution)
    dr        = DeadReckoning(forward_speed=args.speed,
                              recover_speed=args.recover_speed,
                              gy_threshold=args.gy_thresh)
    cal       = AutoCalibrator(dr)
    estimator = BoundaryEstimator(grid, rdp_epsilon_cm=args.rdp)
    viz       = MapVisualizer(grid, estimator)

    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION1,
        client_id="room_mapper",
        clean_session=True,
        userdata={
            "broker": args.broker, "port": args.port,
            "grid": grid, "dead_reckoning": dr, "calibrator": cal, "viz": viz,
        },
    )
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        client.connect(args.broker, args.port, keepalive=60)
    except Exception as e:
        raise SystemExit(f"[ERROR] Cannot connect to {args.broker}:{args.port} — {e}")

    print(f"[INFO] Connecting to {args.broker}:{args.port} ...")
    client.loop_start()
    viz.show()
    client.loop_stop()


if __name__ == "__main__":
    main()
