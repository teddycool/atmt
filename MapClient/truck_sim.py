"""
truck_sim.py — Main simulation runner.

Wires together:
  RoomShape        → room geometry
  Truck            → state container
  TruckMotion      → Ackermann bicycle-model physics
  ExploreStrategy  → autonomous control logic
  TruckVisualizer  → live matplotlib display

Run
---
    python truck_sim.py                          # defaults
    python truck_sim.py --room l_shape           # L-shaped room
    python truck_sim.py --room donut             # rectangular donut
    python truck_sim.py --strategy wall_right    # wall follower
    python truck_sim.py --speed 80 --hz 10       # 80 cm/s, 10 ticks/s
    python truck_sim.py --list-rooms             # show available rooms
"""

import argparse
import json
import math
import random
import threading
import time

import paho.mqtt.client as mqtt_client

from room_shape   import RoomShape
from truck        import Truck, UltrasonicSensors
from truck_motion import TruckMotion
from explore_strategy import make_strategy, STRATEGIES
from truck_visualizer import TruckVisualizer

# ── Simulation constants ──────────────────────────────────────────────────────

DEFAULT_HZ         = 10      # ticks per second
DEFAULT_SPEED      = 80      # % PWM for forward driving
MQTT_BROKER        = "192.168.2.2"
MQTT_PORT          = 1883
MQTT_TOPIC         = "TRUCK_SIM/telemetry"

SENSOR_MAX_CM      = 300.0   # cm — ultrasonic ceiling
SENSOR_NOISE_CM    = 1.5     # cm — Gaussian noise std-dev on sensor readings
HEADING_DRIFT_STEP = 0.15    # deg — per-tick random walk step of heading bias
HEADING_DRIFT_MAX  = 1.0     # deg/tick — max drift rate
MAG_HORIZONTAL     = 30.0    # µT — Earth's horizontal field (North = +Y world)
MAG_VERTICAL       = 40.0    # µT — Earth's vertical field (pointing down)
MAG_NOISE_UT       = 0.5     # µT — magnetometer noise std-dev
ACC_NOISE_MS2      = 0.02    # m/s² — accelerometer noise std-dev (each axis)
GYRO_NOISE_DEGS    = 0.5     # deg/s — gyroscope noise std-dev (all axes)
GYRO_DRIFT_STEP    = 0.03    # deg/s — per-tick random walk step for roll/pitch drift
GYRO_DRIFT_MAX     = 0.2     # deg/s — max roll/pitch drift magnitude

# ── Room presets ──────────────────────────────────────────────────────────────
# Sizes chosen so the area is roughly 4–10 m² (40 000–100 000 cm²).

ROOMS: dict[str, RoomShape] = {
    "rectangle":      RoomShape.rectangle(240, 120),           # 2.88 m²
    "square":         RoomShape.square(250),                   # 6.25 m²
    "l_shape":        RoomShape.l_shape(120, 120, 60, 60),    # ~1.08 m²
    "donut":          RoomShape.donut(300, 300, 120, 120),     # ~8.1 m²
    "circular_donut": RoomShape.circular_donut(150, 60),       # ~6.2 m²
}

# ── Sensing ───────────────────────────────────────────────────────────────────

def sense(truck: Truck, room: RoomShape,
          noise_cm: float = SENSOR_NOISE_CM,
          max_cm:   float = SENSOR_MAX_CM) -> dict:
    """
    Compute the four ultrasonic distances from the truck's current pose.

    Direction offsets (relative to heading):
      uf = 0°   (front)
      ub = 180° (back)
      ul = 270° (left)
      ur = 90°  (right)

    Returns a dict {"uf", "ub", "ul", "ur"} in cm.
    Also updates truck.ultrasonic in-place.
    """
    h = math.radians(truck.heading)
    out = {}
    for key, offset_deg, attr in [
        ("uf",   0,   "front"),
        ("ub", 180,   "back"),
        ("ul", 270,   "left"),
        ("ur",  90,   "right"),
    ]:
        angle  = h + math.radians(offset_deg)
        dx, dy = math.cos(angle), math.sin(angle)
        raw    = room.ray_distance(truck.x, truck.y, dx, dy)
        noisy  = max(0.0, min(max_cm, raw + random.gauss(0, noise_cm)))
        out[key] = round(noisy, 1)
        setattr(truck.ultrasonic, attr, out[key])
    return out


# ── IMU update ────────────────────────────────────────────────────────────────

def update_imu(truck: Truck) -> None:
    """
    Update magnetometer and compass fields from the truck's current heading.
    mag_x = MAG_HORIZONTAL * sin(heading)   [forward component]
    mag_y = MAG_HORIZONTAL * cos(heading)   [left component]
    mag_z = MAG_VERTICAL                    [vertical, constant]
    """
    h_rad = math.radians(truck.heading)
    truck.magnetometer.x = MAG_HORIZONTAL * math.sin(h_rad) + random.gauss(0, MAG_NOISE_UT)
    truck.magnetometer.y = MAG_HORIZONTAL * math.cos(h_rad) + random.gauss(0, MAG_NOISE_UT)
    truck.magnetometer.z = MAG_VERTICAL                      + random.gauss(0, MAG_NOISE_UT)
    truck.compass = math.degrees(
        math.atan2(truck.magnetometer.x, truck.magnetometer.y)) % 360

    truck.acceleration.x += random.gauss(0, ACC_NOISE_MS2)
    truck.acceleration.y += random.gauss(0, ACC_NOISE_MS2)
    truck.acceleration.z += random.gauss(0, ACC_NOISE_MS2)
    truck.gyroscope.x    += random.gauss(0, GYRO_NOISE_DEGS)
    truck.gyroscope.y    += random.gauss(0, GYRO_NOISE_DEGS)
    truck.gyroscope.z    += random.gauss(0, GYRO_NOISE_DEGS)


# ── MQTT ─────────────────────────────────────────────────────────────────────

def mqtt_connect(broker: str, port: int):
    """
    Create and connect a paho MQTT client.
    Returns the client on success, None if the broker is unreachable.
    """
    client = mqtt_client.Client(client_id="truck_sim", clean_session=True)
    try:
        client.connect(broker, port, keepalive=60)
        client.loop_start()
        print(f"[MQTT] Connected to {broker}:{port}  topic={MQTT_TOPIC}")
        return client
    except Exception as e:
        print(f"[WARN] Could not connect to MQTT broker: {e}")
        print("[WARN] Running without MQTT publishing.")
        return None


def _build_payload(truck: Truck) -> str:
    """Serialise truck sensor state to the telemetry JSON string."""
    us = truck.ultrasonic
    payload = {
        "truck_id":      truck.truck_id,
        "seq":           truck.seq,
        "t_ms":          truck.t_ms,
        "mode":          "EXPLORE",
        "ul":            round(us.left,               1),
        "ur":            round(us.right,              1),
        "uf":            round(us.front,              1),
        "ub":            round(us.back,               1),
        "yaw_rate":      round(truck.gyroscope.z,     2),
        "gy_x":          round(truck.gyroscope.x,     2),
        "gy_y":          round(truck.gyroscope.y,     2),
        "gy_z":          round(truck.gyroscope.z,     2),
        "heading":       round(truck.heading,          1),
        "compass":       round(truck.compass,          1),
        "mag_x":         round(truck.magnetometer.x,  2),
        "mag_y":         round(truck.magnetometer.y,  2),
        "mag_z":         round(truck.magnetometer.z,  2),
        "acc_x":         round(truck.acceleration.x,  3),
        "acc_y":         round(truck.acceleration.y,  3),
        "acc_z":         round(truck.acceleration.z,  3),
        "width":         round(us.left + us.right,    1),
        "center_error":  round(us.right - us.left,    1),
        "front_blocked": us.front < 20.0,
        "cmd_pwm":       truck.cmd_pwm,
        "cmd_steer":     truck.cmd_steer,
    }
    return json.dumps(payload)


# ── Simulation loop ───────────────────────────────────────────────────────────

def sim_loop(truck: Truck, room: RoomShape, motion: TruckMotion,
             strategy, viz: TruckVisualizer,
             tick_hz: int, speed_pwm: int,
             mqtt=None) -> None:
    """
    Main simulation tick loop — runs in a background daemon thread.

    Each tick:
      1. Sense    — compute ultrasonic distances from current pose + room
      2. Strategy — decide speed / steering from sensor readings
      3. Physics  — advance position/heading/IMU via Ackermann model
      4. Clamp    — keep truck inside room boundaries
      5. Drift    — add small realistic heading noise on straight drives
      6. IMU      — update magnetometer / compass
      7. Metadata — increment seq and t_ms
      8. Visualize — push snapshot to the visualizer
      9. Publish  — send telemetry JSON to MQTT broker
    """
    dt              = 1.0 / tick_hz
    drift           = 0.0     # slowly-varying heading bias (deg/tick)
    gyro_drift_x    = 0.0     # slowly-varying roll-rate bias (deg/s)
    gyro_drift_y    = 0.0     # slowly-varying pitch-rate bias (deg/s)
    interval_ms     = int(1000 / tick_hz)
    mqtt_every      = max(1, tick_hz // 2)   # publish every 500 ms → 2 Hz

    # Set the initial forward speed on the motion controller
    motion.set_pwm(speed_pwm)

    while True:
        tick_start = time.monotonic()

        # 1. Sense
        sensors = sense(truck, room)

        # 2. Strategy
        strategy.step(truck, motion, sensors, dt)

        # 3. Physics — save position before update for clamping
        prev_x, prev_y = truck.x, truck.y
        motion.update(truck, dt)

        # 4. Clamp — if new position is outside room, restore safe position
        if not room.contains(truck.x, truck.y):
            truck.x, truck.y = room.clamp_position(prev_x, prev_y,
                                                    truck.x, truck.y)

        # 5. Heading drift + gyro roll/pitch drift — straight-line imperfection
        if truck.cmd_steer == "STRAIGHT":
            drift += random.gauss(0, HEADING_DRIFT_STEP)
            drift  = max(-HEADING_DRIFT_MAX, min(HEADING_DRIFT_MAX, drift))
            truck.heading = (truck.heading + drift) % 360

            gyro_drift_x += random.gauss(0, GYRO_DRIFT_STEP)
            gyro_drift_x  = max(-GYRO_DRIFT_MAX, min(GYRO_DRIFT_MAX, gyro_drift_x))
            gyro_drift_y += random.gauss(0, GYRO_DRIFT_STEP)
            gyro_drift_y  = max(-GYRO_DRIFT_MAX, min(GYRO_DRIFT_MAX, gyro_drift_y))
        else:
            drift        *= 0.8   # decay biases when steering intentionally
            gyro_drift_x *= 0.8
            gyro_drift_y *= 0.8

        truck.gyroscope.x += gyro_drift_x
        truck.gyroscope.y += gyro_drift_y

        # 6. IMU
        update_imu(truck)

        # 7. Metadata
        truck.seq  += 1
        truck.t_ms += interval_ms

        # 8. Visualize
        viz.update(truck, sensors)

        # 9. Publish telemetry at 2 Hz
        if mqtt is not None and truck.seq % mqtt_every == 0:
            mqtt.publish(MQTT_TOPIC, _build_payload(truck))

        # Print position every second
        if truck.seq % tick_hz == 0:
            print(f"[SIM] t={truck.t_ms/1000:.1f}s  "
                  f"pos=({truck.x:.1f}, {truck.y:.1f}) cm  "
                  f"hdg={truck.heading:.1f}°  "
                  f"uf={sensors['uf']:.1f}  ul={sensors['ul']:.1f}  "
                  f"ur={sensors['ur']:.1f} cm")

        # Sleep for remainder of tick
        elapsed = time.monotonic() - tick_start
        time.sleep(max(0.0, dt - elapsed))


# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Autonomous truck room simulator",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--room",     default="l_shape",
                        choices=list(ROOMS),
                        help="Room geometry preset")
    parser.add_argument("--strategy", default="embedded",
                        choices=list(STRATEGIES),
                        help="Exploration strategy")
    parser.add_argument("--speed",    default=DEFAULT_SPEED, type=int,
                        help="Forward PWM %% (0–100)")
    parser.add_argument("--hz",       default=DEFAULT_HZ,    type=int,
                        help="Simulation tick rate (ticks/second)")
    parser.add_argument("--list-rooms", action="store_true",
                        help="Print available room presets and exit")
    parser.add_argument("--broker",   default=MQTT_BROKER,
                        help="MQTT broker host")
    parser.add_argument("--port",     default=MQTT_PORT, type=int,
                        help="MQTT broker port")
    parser.add_argument("--no-mqtt",  action="store_true",
                        help="Disable MQTT publishing")
    args = parser.parse_args()

    if args.list_rooms:
        for name, room in ROOMS.items():
            b = room.bounds
            w, h = b[2] - b[0], b[3] - b[1]
            print(f"  {name:<20} {w:.0f} × {h:.0f} cm")
        return

    room     = ROOMS[args.room]
    strategy = make_strategy(args.strategy)
    motion   = TruckMotion()
    viz      = TruckVisualizer(room, refresh_ms=max(50, 1000 // args.hz))
    mqtt     = None if args.no_mqtt else mqtt_connect(args.broker, args.port)

    # Initialise truck at a random valid position inside the room
    truck       = Truck(truck_id="TRUCK_SIM")
    truck.x, truck.y = room.random_point(margin=20.0)
    truck.heading    = random.uniform(0, 360)

    print(f"[SIM] room={args.room}  strategy={args.strategy}  "
          f"speed={args.speed}%  hz={args.hz}")
    print(f"[SIM] start pos=({truck.x:.0f}, {truck.y:.0f}) cm  "
          f"heading={truck.heading:.0f}°")
    print(f"[SIM] bounds: {room.bounds}")
    print()

    # Run simulation in a background daemon thread
    t = threading.Thread(
        target=sim_loop,
        args=(truck, room, motion, strategy, viz, args.hz, args.speed, mqtt),
        daemon=True,
    )
    t.start()

    # Visualizer owns the main thread (required by matplotlib)
    viz.show()


if __name__ == "__main__":
    main()
