"""
Truck Map Visualizer
Subscribes to all truck telemetry topics and visualizes their paths in real time.
Discovers trucks dynamically via the wildcard topic +/telemetry.
"""

import json
import math
import argparse
import threading
import numpy as np
import matplotlib
matplotlib.use("TkAgg")          # interactive backend — requires python3-tk
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import matplotlib.patches as patches
from collections import defaultdict

try:
    import paho.mqtt.client as mqtt
except ImportError:
    raise SystemExit("[ERROR] paho-mqtt not installed. Run: pip install paho-mqtt")

# ── Constants ─────────────────────────────────────────────────────────────────

MQTT_BROKER      = "192.168.2.2"
MQTT_PORT        = 1883
TOPIC_WILDCARD   = "+/telemetry"

SENSOR_MAX_RANGE = 150.0   # cm — ignore sensor projections beyond this
WALL_HIT_THRESH  = 140.0   # cm — only record as wall point if closer than this
BOUNDS_PERCENTILE = 3      # % to trim on each side when fitting boundary

# One colour per truck (cycles if more trucks than colours)
TRUCK_COLORS = ["tab:blue", "tab:orange", "tab:green", "tab:red", "tab:purple"]

# ── Per-truck state ───────────────────────────────────────────────────────────

class TruckState:
    def __init__(self, truck_id, color):
        self.truck_id  = truck_id
        self.color     = color
        self.x         = None
        self.y         = None
        self.heading   = 0.0
        self.prev_t_ms = None
        self.path_x    = []
        self.path_y    = []
        self.wall_x    = []
        self.wall_y    = []
        # Incremental axis bounds — updated per message, avoids O(n) scan each frame
        self.x_min = self.x_max = None
        self.y_min = self.y_max = None
        self.lock   = threading.Lock()

    def _expand_bounds(self, x, y):
        """Extend tracked axis bounds to include (x, y). Call inside lock."""
        if self.x_min is None:
            self.x_min = self.x_max = x
            self.y_min = self.y_max = y
        else:
            if x < self.x_min: self.x_min = x
            if x > self.x_max: self.x_max = x
            if y < self.y_min: self.y_min = y
            if y > self.y_max: self.y_max = y

    def update(self, msg: dict):
        t_ms    = msg.get("t_ms", 0)
        heading = msg.get("compass") or msg.get("heading") or 0.0
        self.heading = heading

        # Use ground-truth x/y if present (simulator), else dead-reckon from commands
        if "x" in msg and "y" in msg:
            self.x = msg["x"]
            self.y = msg["y"]
        else:
            if self.x is None:
                self.x, self.y = 0.0, 0.0
            if self.prev_t_ms is not None:
                dt_s      = (t_ms - self.prev_t_ms) / 1000.0
                cmd_pwm   = msg.get("cmd_pwm", 0)
                speed_cms = (cmd_pwm / 100.0) * 25.0
                angle     = math.radians(self.heading)
                self.x   += speed_cms * dt_s * math.cos(angle)
                self.y   += speed_cms * dt_s * math.sin(angle)

        self.prev_t_ms = t_ms

        with self.lock:
            self.path_x.append(self.x)
            self.path_y.append(self.y)
            self._expand_bounds(self.x, self.y)

            # Project sensor rays → wall hit points
            for offset_deg, key in [(0, "uf"), (180, "ub"), (270, "ul"), (90, "ur")]:
                dist = msg.get(key, SENSOR_MAX_RANGE)
                if dist < WALL_HIT_THRESH:
                    angle = math.radians(self.heading + offset_deg)
                    wx = self.x + dist * math.cos(angle)
                    wy = self.y + dist * math.sin(angle)
                    self.wall_x.append(wx)
                    self.wall_y.append(wy)
                    self._expand_bounds(wx, wy)


def fit_boundary(wall_x, wall_y):
    """Return (x, y, w, h) of estimated bounding rectangle, or None if too few points."""
    if len(wall_x) < 8:
        return None
    x0 = np.percentile(wall_x, BOUNDS_PERCENTILE)
    x1 = np.percentile(wall_x, 100 - BOUNDS_PERCENTILE)
    y0 = np.percentile(wall_y, BOUNDS_PERCENTILE)
    y1 = np.percentile(wall_y, 100 - BOUNDS_PERCENTILE)
    return x0, y0, x1 - x0, y1 - y0


# ── MQTT ──────────────────────────────────────────────────────────────────────

class MQTTListener:
    def __init__(self, broker, port):
        self.trucks: dict[str, TruckState] = {}
        self._color_idx = 0
        self._lock = threading.Lock()

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
                print(f"[VIZ] New truck discovered: {truck_id} ({color})")

        self.trucks[truck_id].update(payload)


# ── Visualization ─────────────────────────────────────────────────────────────

def run_visualizer(listener: MQTTListener):
    fig, ax = plt.subplots(figsize=(8, 8))
    ax.set_title("Truck Map Visualizer")
    ax.set_xlabel("x (cm)")
    ax.set_ylabel("y (cm)")
    ax.set_aspect("equal")
    ax.grid(True, linestyle="--", alpha=0.4)

    # Artists stored per truck_id
    path_lines  = {}
    wall_scatts = {}
    heading_arrows = {}
    bound_rects = {}
    bound_texts = {}

    def init():
        return []

    def update(_frame):
        artists = []
        trucks = dict(listener.trucks)  # snapshot

        for truck_id, state in trucks.items():
            with state.lock:
                px = list(state.path_x)
                py = list(state.path_y)
                wx = list(state.wall_x)
                wy = list(state.wall_y)
                heading  = state.heading
                color    = state.color
                cur_x    = state.x
                cur_y    = state.y

            if cur_x is None:
                continue

            # ── Path line
            if truck_id not in path_lines:
                (line,) = ax.plot([], [], "--", color=color, linewidth=1.2,
                                  label=truck_id, alpha=0.7)
                path_lines[truck_id] = line
            path_lines[truck_id].set_data(px, py)
            artists.append(path_lines[truck_id])

            # ── Wall hit scatter
            if truck_id not in wall_scatts:
                wall_scatts[truck_id] = ax.scatter(
                    [], [], s=4, color=color, alpha=0.3, marker=".")
            if wx:
                wall_scatts[truck_id].set_offsets(np.c_[wx, wy])
            artists.append(wall_scatts[truck_id])

            # ── Heading arrow
            arrow_len = 10.0
            angle = math.radians(heading)
            dx, dy = arrow_len * math.cos(angle), arrow_len * math.sin(angle)
            if truck_id in heading_arrows:
                heading_arrows[truck_id].remove()
            heading_arrows[truck_id] = ax.annotate(
                "", xy=(cur_x + dx, cur_y + dy), xytext=(cur_x, cur_y),
                arrowprops=dict(arrowstyle="->", color=color, lw=2),
            )
            artists.append(heading_arrows[truck_id])

            # ── Estimated boundary rectangle
            bounds = fit_boundary(wx, wy)
            if bounds:
                bx, by, bw, bh = bounds
                if truck_id in bound_rects:
                    bound_rects[truck_id].remove()
                rect = patches.Rectangle(
                    (bx, by), bw, bh,
                    linewidth=2, edgecolor="red", facecolor="none",
                    linestyle="-", alpha=0.9,
                )
                ax.add_patch(rect)
                bound_rects[truck_id] = rect
                artists.append(rect)

                label = f"{bw:.0f}×{bh:.0f} cm"
                if truck_id in bound_texts:
                    bound_texts[truck_id].remove()
                bound_texts[truck_id] = ax.text(
                    bx + bw / 2, by + bh + 4, label,
                    ha="center", color=color, fontsize=8,
                )
                artists.append(bound_texts[truck_id])

        # Auto-scale axes to data
        all_x = [x for s in trucks.values() for x in s.path_x + s.wall_x]
        all_y = [y for s in trucks.values() for y in s.path_y + s.wall_y]
        if all_x and all_y:
            margin = 15
            ax.set_xlim(min(all_x) - margin, max(all_x) + margin)
            ax.set_ylim(min(all_y) - margin, max(all_y) + margin)

        if trucks:
            ax.legend(loc="upper right", fontsize=8)

        return artists

    ani = animation.FuncAnimation(
        fig, update, init_func=init, interval=200, blit=False,
        cache_frame_data=False,
    )

    plt.tight_layout()
    plt.show()


# ── CLI ───────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Real-time truck map visualizer")
    parser.add_argument("--broker", default=MQTT_BROKER, help="MQTT broker host")
    parser.add_argument("--port",   default=MQTT_PORT, type=int, help="MQTT broker port")
    args = parser.parse_args()

    listener = MQTTListener(args.broker, args.port)
    run_visualizer(listener)
