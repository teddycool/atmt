"""
truck_visualizer.py — Live matplotlib visualizer for the truck simulator.

Shows
-----
  • Room boundary (outer wall in red, inner holes/islands in dark-red)
  • Truck path    (thin dashed grey trail)
  • Current pose  (filled dot + heading arrow)
  • Sensor rays   (coloured lines: front=orange, back=grey, left=cyan, right=green)
  • Info panel    (speed, steering angle, sensor distances, tick time)

Thread safety
-------------
The simulation runs in a background thread and calls update() each tick.
The matplotlib FuncAnimation reads a snapshot protected by a Lock so that
no partial state is ever displayed.

Usage
-----
    viz = TruckVisualizer(room)

    def sim_loop():
        while True:
            motion.update(truck, dt)
            sensors = sense(truck, room)
            strategy.step(truck, motion, sensors, dt)
            viz.update(truck, sensors)
            time.sleep(dt)

    t = threading.Thread(target=sim_loop, daemon=True)
    t.start()
    viz.show()          # blocks until the window is closed
"""

import math
import threading

import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib.animation as animation
from matplotlib.patches import FancyArrowPatch

from room_shape import RoomShape
from truck import Truck


# ── Colour palette ────────────────────────────────────────────────────────────

_COLOUR = {
    "wall_outer": "#e53935",    # red
    "wall_hole":  "#b71c1c",    # dark red (inner islands)
    "path":       "#90a4ae",    # grey trail
    "truck":      "#1565c0",    # blue dot
    "arrow":      "#0d47a1",    # dark blue heading arrow
    "ray_front":  "#ff6f00",    # orange
    "ray_back":   "#78909c",    # grey
    "ray_left":   "#00acc1",    # cyan
    "ray_right":  "#43a047",    # green
    "text_bg":    "#ffffffcc",  # semi-transparent white
}

_RAY_ALPHA = 0.55
_PATH_MAX  = 4000   # maximum trail points kept in memory


# ── TruckVisualizer ───────────────────────────────────────────────────────────

class TruckVisualizer:
    """
    Live top-down visualizer for a truck exploring a room.

    Parameters
    ----------
    room        : RoomShape — defines the boundaries to draw
    refresh_ms  : int       — animation poll interval in milliseconds
    title       : str       — window title
    max_path    : int       — maximum number of path points to keep
    """

    def __init__(self, room: RoomShape,
                 refresh_ms: int = 100,
                 title:      str = "Truck Simulator",
                 max_path:   int = _PATH_MAX):

        self._room       = room
        self._refresh_ms = refresh_ms
        self._max_path   = max_path

        # Shared state (written by sim thread, read by animation callback)
        self._lock    = threading.Lock()
        self._snap    = None   # latest (truck_copy, sensors_copy) snapshot

        # Path history (list of (x, y)) — protected by lock
        self._path_xs: list[float] = []
        self._path_ys: list[float] = []

        self._fig, self._ax = self._build_figure(title)
        self._artists       = self._build_artists()
        self._anim          = None   # created in show()

    # ── Figure construction ───────────────────────────────────────────────────

    def _build_figure(self, title):
        x_min, y_min, x_max, y_max = self._room.bounds
        w_cm = x_max - x_min
        h_cm = y_max - y_min

        # Scale figure so 1 cm ≈ proportional pixels, max 10 × 10 inches
        scale  = min(10.0 / max(w_cm, 1), 10.0 / max(h_cm, 1)) * 60
        fig_w  = max(6.0, w_cm * scale / 72)
        fig_h  = max(5.0, h_cm * scale / 72)

        fig, ax = plt.subplots(figsize=(fig_w, fig_h))
        fig.patch.set_facecolor("#fafafa")
        ax.set_facecolor("#f5f5f5")
        ax.set_aspect("equal")
        ax.set_title(title, fontsize=11, pad=8)
        ax.set_xlabel("x  (cm)", fontsize=9)
        ax.set_ylabel("y  (cm)", fontsize=9)
        ax.tick_params(labelsize=8)

        margin = max(w_cm, h_cm) * 0.08
        ax.set_xlim(x_min - margin, x_max + margin)
        ax.set_ylim(y_min - margin, y_max + margin)
        ax.grid(True, color="#dddddd", linewidth=0.5, zorder=0)

        # Room boundary patches
        for idx, poly in enumerate(self._room.all_polygons):
            color = _COLOUR["wall_outer"] if idx == 0 else _COLOUR["wall_hole"]
            patch = mpatches.Polygon(
                poly, closed=True,
                linewidth=2.5, edgecolor=color, facecolor="none", zorder=2,
            )
            ax.add_patch(patch)

        return fig, ax

    def _build_artists(self):
        ax = self._ax

        # Path trail
        (path_line,) = ax.plot(
            [], [], color=_COLOUR["path"],
            linewidth=1.0, linestyle="--", alpha=0.6, zorder=3,
        )

        # Sensor rays: front, back, left, right
        sensor_lines = {
            "front": ax.plot([], [], color=_COLOUR["ray_front"],
                             linewidth=1.5, alpha=_RAY_ALPHA, zorder=4)[0],
            "back":  ax.plot([], [], color=_COLOUR["ray_back"],
                             linewidth=1.2, alpha=_RAY_ALPHA, zorder=4)[0],
            "left":  ax.plot([], [], color=_COLOUR["ray_left"],
                             linewidth=1.5, alpha=_RAY_ALPHA, zorder=4)[0],
            "right": ax.plot([], [], color=_COLOUR["ray_right"],
                             linewidth=1.5, alpha=_RAY_ALPHA, zorder=4)[0],
        }

        # Truck dot
        (truck_dot,) = ax.plot(
            [], [], "o",
            color=_COLOUR["truck"], markersize=8, zorder=6,
        )

        # Heading arrow (re-created each frame)
        heading_arrow = [None]

        # Info text box
        info_text = ax.text(
            0.01, 0.99, "",
            transform=ax.transAxes,
            fontsize=7.5, verticalalignment="top", fontfamily="monospace",
            bbox=dict(boxstyle="round,pad=0.4", facecolor=_COLOUR["text_bg"],
                      edgecolor="#cccccc", linewidth=0.8),
            zorder=7,
        )

        return dict(
            path_line     = path_line,
            sensor_lines  = sensor_lines,
            truck_dot     = truck_dot,
            heading_arrow = heading_arrow,
            info_text     = info_text,
        )

    # ── Public API ────────────────────────────────────────────────────────────

    def update(self, truck: Truck, sensors: dict) -> None:
        """
        Called by the simulation thread every tick.
        Stores a lightweight snapshot for the next animation frame.
        sensors: dict with keys uf, ub, ul, ur  (cm).
        """
        with self._lock:
            self._snap = (
                truck.x, truck.y, truck.heading,
                truck.cmd_pwm, truck.cmd_steer,
                truck.t_ms, truck.seq,
                float(sensors.get("uf", 0)),
                float(sensors.get("ub", 0)),
                float(sensors.get("ul", 0)),
                float(sensors.get("ur", 0)),
            )
            self._path_xs.append(truck.x)
            self._path_ys.append(truck.y)
            if len(self._path_xs) > self._max_path:
                self._path_xs = self._path_xs[-self._max_path:]
                self._path_ys = self._path_ys[-self._max_path:]

    def show(self) -> None:
        """Start the animation and block until the window is closed."""
        self._anim = animation.FuncAnimation(
            self._fig,
            self._draw_frame,
            interval=self._refresh_ms,
            blit=False,
            cache_frame_data=False,
        )
        plt.tight_layout()
        plt.show()

    # ── Animation callback ────────────────────────────────────────────────────

    def _ray_endpoint(self, x, y, heading_deg, offset_deg, dist_cm):
        """Compute the endpoint of a sensor ray."""
        angle = math.radians(heading_deg + offset_deg)
        return x + dist_cm * math.cos(angle), y + dist_cm * math.sin(angle)

    def _draw_frame(self, _frame):
        with self._lock:
            snap   = self._snap
            path_x = list(self._path_xs)
            path_y = list(self._path_ys)

        if snap is None:
            return

        (x, y, heading, cmd_pwm, cmd_steer,
         t_ms, seq, uf, ub, ul, ur) = snap

        a = self._artists

        # ── Path trail ───────────────────────────────────────────────────────
        a["path_line"].set_data(path_x, path_y)

        # ── Sensor rays (front=0°, back=180°, left=270°, right=90°) ─────────
        for key, offset, dist in [
            ("front",   0,   uf),
            ("back",  180,   ub),
            ("left",  270,   ul),
            ("right",  90,   ur),
        ]:
            ex, ey = self._ray_endpoint(x, y, heading, offset, dist)
            a["sensor_lines"][key].set_data([x, ex], [y, ey])

        # ── Truck dot ────────────────────────────────────────────────────────
        a["truck_dot"].set_data([x], [y])

        # ── Heading arrow ────────────────────────────────────────────────────
        if a["heading_arrow"][0] is not None:
            a["heading_arrow"][0].remove()

        arrow_len = max(4.0, (abs(cmd_pwm) / 100.0) * 18.0)
        angle     = math.radians(heading)
        a["heading_arrow"][0] = self._ax.annotate(
            "",
            xy=(x + arrow_len * math.cos(angle),
                y + arrow_len * math.sin(angle)),
            xytext=(x, y),
            arrowprops=dict(
                arrowstyle="-|>",
                color=_COLOUR["arrow"],
                lw=2.0,
                mutation_scale=14,
            ),
            zorder=6,
        )

        # ── Info text ────────────────────────────────────────────────────────
        speed_cms = (abs(cmd_pwm) / 100.0) * 100.0   # rough cm/s estimate
        a["info_text"].set_text(
            f" t = {t_ms / 1000:.1f} s   seq = {seq}\n"
            f" pos  ({x:6.1f}, {y:6.1f}) cm\n"
            f" hdg  {heading:6.1f}°\n"
            f" spd  {speed_cms:6.1f} cm/s   pwm {cmd_pwm:+d}\n"
            f" steer {cmd_steer}\n"
            f" uf {uf:5.1f}  ub {ub:5.1f}\n"
            f" ul {ul:5.1f}  ur {ur:5.1f} cm "
        )
