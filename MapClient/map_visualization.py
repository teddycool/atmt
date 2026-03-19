"""
Truck Sensor Visualizer
Subscribes to all truck telemetry topics and shows real-time rolling charts
for the four ultrasonic distance sensors: UF (front), UB (back), UL (left), UR (right).
One line per truck, one subplot per sensor.
"""

import json
import argparse
import threading
from collections import deque

import matplotlib
matplotlib.use("TkAgg")          # interactive backend — requires python3-tk
import matplotlib.pyplot as plt
import matplotlib.animation as animation

try:
    import paho.mqtt.client as mqtt
except ImportError:
    raise SystemExit("[ERROR] paho-mqtt not installed. Run: pip install paho-mqtt")

# ── Constants ─────────────────────────────────────────────────────────────────

MQTT_BROKER    = "192.168.2.2"
MQTT_PORT      = 1883
TOPIC_WILDCARD = "+/telemetry"

SENSOR_MAX     = 250.0   # cm — y-axis ceiling
WINDOW         = 60      # number of readings to keep per sensor per truck

# Threshold reference lines drawn on every subplot
STOP_DIST      = 12.0    # cm — red line (embedded front-stop threshold)
SLOW_DIST      = 20.0    # cm — orange line (embedded slow-down threshold)

SENSORS        = ["uf", "ub", "ul", "ur"]
SENSOR_LABELS  = {"uf": "Front (UF)", "ub": "Back (UB)",
                  "ul": "Left (UL)",  "ur": "Right (UR)"}

TRUCK_COLORS   = ["tab:blue", "tab:orange", "tab:green", "tab:purple", "tab:red"]

# ── Per-truck state ───────────────────────────────────────────────────────────

class TruckState:
    def __init__(self, truck_id, color):
        self.truck_id = truck_id
        self.color    = color
        # One deque per sensor
        self.history  = {s: deque(maxlen=WINDOW) for s in SENSORS}
        self.lock     = threading.Lock()

    def update(self, msg: dict):
        with self.lock:
            for s in SENSORS:
                if s in msg:
                    self.history[s].append(float(msg[s]))

# ── MQTT listener ─────────────────────────────────────────────────────────────

class MQTTListener:
    def __init__(self, broker, port):
        self.trucks     = {}          # truck_id → TruckState
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
                print(f"[VIZ] New truck discovered: {truck_id} ({color})")

        self.trucks[truck_id].update(payload)

# ── Visualization ─────────────────────────────────────────────────────────────

def run_visualizer(listener: MQTTListener):
    fig, axes = plt.subplots(2, 2, figsize=(10, 6))
    fig.suptitle("Ultrasonic Sensor Distances", fontsize=13)

    # Map sensor key → subplot axis
    ax_map = {
        "uf": axes[0][0],
        "ub": axes[0][1],
        "ul": axes[1][0],
        "ur": axes[1][1],
    }

    # Draw static threshold lines and labels on every subplot
    for sensor, ax in ax_map.items():
        ax.set_title(SENSOR_LABELS[sensor])
        ax.set_ylim(0, SENSOR_MAX)
        ax.set_xlim(0, WINDOW)
        ax.set_ylabel("Distance (cm)")
        ax.set_xlabel("Readings")
        ax.axhline(STOP_DIST, color="red",    linestyle="--", linewidth=1,
                   label=f"Stop {STOP_DIST:.0f} cm")
        ax.axhline(SLOW_DIST, color="orange", linestyle="--", linewidth=1,
                   label=f"Slow {SLOW_DIST:.0f} cm")
        ax.legend(fontsize=7, loc="upper right")
        ax.grid(True, alpha=0.3)

    # Lines stored as {truck_id: {sensor: Line2D}}
    lines = {}

    def update(_frame):
        trucks = dict(listener.trucks)  # snapshot

        for truck_id, state in trucks.items():
            with state.lock:
                history = {s: list(state.history[s]) for s in SENSORS}

            if truck_id not in lines:
                lines[truck_id] = {}

            for sensor, ax in ax_map.items():
                data = history[sensor]
                if not data:
                    continue

                if sensor not in lines[truck_id]:
                    (line,) = ax.plot([], [], color=state.color,
                                      linewidth=1.5, label=truck_id)
                    ax.legend(fontsize=7, loc="upper right")
                    lines[truck_id][sensor] = line

                lines[truck_id][sensor].set_data(range(len(data)), data)

        return [l for truck in lines.values() for l in truck.values()]

    ani = animation.FuncAnimation(
        fig, update, interval=200, blit=False, cache_frame_data=False,
    )

    plt.tight_layout()
    plt.show()

# ── CLI ───────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Real-time ultrasonic sensor visualizer")
    parser.add_argument("--broker", default=MQTT_BROKER, help="MQTT broker host")
    parser.add_argument("--port",   default=MQTT_PORT, type=int, help="MQTT broker port")
    args = parser.parse_args()

    listener = MQTTListener(args.broker, args.port)
    run_visualizer(listener)
