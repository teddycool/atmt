"""
truck_remote.py — Autonomous remote control for the real truck.

Subscribes to the truck telemetry topic, parses sensor data, and publishes
control commands to steer the truck via exploration strategies.

Control interface (from remote.sh)
-----------------------------------
Topic  : {vehicle_id}/control
Payload: {"motor": <-100…100>, "direction": <-100…100>}
  motor     : 100 = full forward, -100 = full reverse, 0 = stop
  direction : -10 = right, 0 = straight, +10 = left (steps of 10)

Strategies
----------
  embedded   — DRIVE/RECOVER state machine (mirrors C++ firmware)
  reactive   — go straight, steer toward open side on obstacle
  wall_right — proportional controller keeping right wall at target distance

Run
---
    python truck_remote.py
    python truck_remote.py --strategy embedded
    python truck_remote.py --broker 192.168.2.2 --id 38504720f540
"""

import argparse
import json
from abc import ABC, abstractmethod

import paho.mqtt.client as mqtt

# ── Configuration ─────────────────────────────────────────────────────────────

MQTT_BROKER = "192.168.2.2"
VEHICLE_ID  = "38504720f540"

TOPIC_TELEMETRY = f"{VEHICLE_ID}/telemetry"
TOPIC_CONTROL   = f"{VEHICLE_ID}/control"

FIELDS = [
    "truck_id", "seq", "t_ms",
    "ul", "ur", "uf", "ub",
    "yaw_rate",
    "heading", "compass",
    "acc_x", "acc_y", "acc_z",
    "cmd_pwm", "cmd_steer",
]

# ── Motor / steering constants ────────────────────────────────────────────────

MOTOR_FWD     = 100    # full forward
MOTOR_REV     = -80    # reverse during recovery
DIR_MAX       = 100    # max direction value — full lock
DIR_STEP      = 10     # one steering increment


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


# ── RemoteControl — thin wrapper around send_command ─────────────────────────

class RemoteControl:
    """
    Mirrors the TruckMotion API from the simulator so strategies can be
    ported directly.  Accumulates motor/direction state and publishes via
    send_command() on flush().
    """

    def __init__(self):
        self._motor     = 0
        self._direction = 0

    # Motor — always full power (real truck requires 100 to move)
    def drive_forward(self, pwm: int = 100):
        self._motor = 100

    def drive_reverse(self, pwm: int = 100):
        self._motor = -100

    def stop(self):
        self._motor = 0

    # Steering
    def steer_center(self):
        self._direction = 0

    def steer_left(self):
        self._direction = min(DIR_MAX, self._direction + DIR_STEP)

    def steer_right(self):
        self._direction = max(-DIR_MAX, self._direction - DIR_STEP)

    def set_steer(self, value: int):
        """Set direction directly (clamped and snapped to DIR_STEP grid)."""
        snapped = round(value / DIR_STEP) * DIR_STEP
        self._direction = max(-DIR_MAX, min(DIR_MAX, snapped))

    # Publish
    def flush(self, client: mqtt.Client) -> None:
        """Publish the current motor/direction state to the truck."""
        payload = json.dumps({"motor": self._motor, "direction": self._direction})
        client.publish(TOPIC_CONTROL, payload)

    @property
    def motor(self) -> int:
        return self._motor

    @property
    def direction(self) -> int:
        return self._direction


# ── Exploration strategies ────────────────────────────────────────────────────

class ExploreStrategy(ABC):
    """Abstract base — subclasses implement step()."""

    @abstractmethod
    def step(self, sensors: dict, t_ms: int, rc: RemoteControl) -> None:
        """
        Decide control for one telemetry tick.

        Parameters
        ----------
        sensors : dict with keys uf, ub, ul, ur  (cm)
        t_ms    : simulation/truck time in milliseconds
        rc      : RemoteControl object to write motor/steering commands into
        """


class ReactiveExplore(ExploreStrategy):
    """
    Drive forward at full speed.
    When uf < STOP_DIST: stop, steer toward the side with more clearance.
    When uf clears CLEAR_DIST: straighten up and drive forward again.
    """

    STOP_DIST  = 20.0
    CLEAR_DIST = 30.0

    def __init__(self):
        self._avoiding = False

    def step(self, sensors, t_ms, rc):
        uf, ul, ur = sensors["uf"], sensors["ul"], sensors["ur"]

        if self._avoiding:
            if uf >= self.CLEAR_DIST:
                rc.steer_center()
                rc.drive_forward()
                self._avoiding = False
            else:
                rc.drive_reverse(pwm=60)
        else:
            if uf < self.STOP_DIST:
                self._avoiding = True
                rc.stop()
                if ur >= ul:
                    rc.set_steer(-DIR_MAX)   # steer right
                else:
                    rc.set_steer(+DIR_MAX)   # steer left
            else:
                rc.drive_forward()
                rc.steer_center()


class EmbeddedController(ExploreStrategy):
    """
    DRIVE / RECOVER state machine — mirrors the C++ firmware logic.

    DRIVE   : full speed forward, steering centred.
              Transitions to RECOVER when uf < front_stop_dist.
    RECOVER : reverse with max steering toward the open side for
              recover_ms milliseconds, then back to DRIVE.
    """

    def __init__(self,
                 front_stop_dist: float = 20.0,
                 pwm_forward:     int   = 100,
                 pwm_reverse:     int   = 100,
                 recover_ms:      int   = 4000):
        self.front_stop_dist = front_stop_dist
        self.pwm_forward     = pwm_forward
        self.pwm_reverse     = pwm_reverse
        self.recover_ms      = recover_ms

        self._state            = "DRIVE"
        self._recover_start_ms = 0
        self._recover_dir      = +DIR_MAX

    def step(self, sensors, t_ms, rc):
        uf, ul, ur = sensors["uf"], sensors["ul"], sensors["ur"]

        if self._state == "DRIVE":
            if uf < self.front_stop_dist:
                self._recover_dir      = +DIR_MAX if ul < ur else -DIR_MAX
                self._recover_start_ms = t_ms
                self._state            = "RECOVER"
                rc.stop()
            else:
                rc.drive_forward(self.pwm_forward)
                rc.steer_center()

        else:  # RECOVER
            if t_ms - self._recover_start_ms >= self.recover_ms:
                self._state = "DRIVE"
                rc.steer_center()
                rc.drive_forward(self.pwm_forward)
            else:
                rc.set_steer(self._recover_dir)
                rc.drive_reverse(self.pwm_reverse)


class WallFollower(ExploreStrategy):
    """
    Right-hand wall follower.

    Maintains target_dist cm from the right wall using a proportional
    steering controller.  Turns left when the front is blocked; steers
    right to find the wall when ur > search_dist.
    """

    def __init__(self,
                 target_dist: float = 25.0,
                 stop_dist:   float = 20.0,
                 search_dist: float = 60.0,
                 kp:          float = 0.3):
        self.target_dist = target_dist
        self.stop_dist   = stop_dist
        self.search_dist = search_dist
        self.kp          = kp
        self._turning_left = False

    def step(self, sensors, t_ms, rc):
        uf, ul, ur = sensors["uf"], sensors["ul"], sensors["ur"]

        if uf < self.stop_dist:
            self._turning_left = True
            rc.drive_forward(pwm=50)
            rc.set_steer(+DIR_MAX)
            return

        if self._turning_left and uf >= self.stop_dist + 10:
            self._turning_left = False

        if ur > self.search_dist:
            rc.set_steer(-DIR_MAX)   # steer right to find wall
            rc.drive_forward()
        else:
            error = self.target_dist - ur   # positive → too close → steer left
            if error > 5:
                rc.set_steer(+DIR_MAX)
            elif error < -5:
                rc.set_steer(-DIR_MAX)
            else:
                rc.steer_center()
            rc.drive_forward()


# ── Strategy registry ─────────────────────────────────────────────────────────

STRATEGIES = {
    "embedded":   EmbeddedController,
    "reactive":   ReactiveExplore,
    "wall_right": WallFollower,
}


# ── MQTT callbacks ────────────────────────────────────────────────────────────

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[MQTT] Connected to {userdata['broker']}")
        client.subscribe(TOPIC_TELEMETRY)
        print(f"[MQTT] Subscribed to  {TOPIC_TELEMETRY}")
        print(f"[MQTT] Publishing to  {TOPIC_CONTROL}")
    else:
        print(f"[ERROR] Connection failed  rc={rc}")


def on_message(client, userdata, msg):
    strategy = userdata["strategy"]
    rc       = userdata["rc"]

    data = parse_telemetry(msg.payload.decode())
    if data is None:
        return

    sensors = {k: data[k] for k in ("uf", "ub", "ul", "ur")}
    strategy.step(sensors, data["t_ms"], rc)
    rc.flush(client)

    print(
        f"seq={data['seq']:5d}  t={data['t_ms']/1000:.1f}s  "
        f"uf={sensors['uf']:5.1f}  ul={sensors['ul']:5.1f}  "
        f"ur={sensors['ur']:5.1f}  ub={sensors['ub']:5.1f} cm  "
        f"motor={rc.motor:+4d}  dir={rc.direction:+4d}"
    )


# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Autonomous truck remote controller",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--broker",   default=MQTT_BROKER,    help="MQTT broker host")
    parser.add_argument("--port",     default=1883, type=int, help="MQTT broker port")
    parser.add_argument("--id",       default=VEHICLE_ID,     help="Vehicle ID")
    parser.add_argument("--strategy", default="embedded",
                        choices=list(STRATEGIES),
                        help="Exploration strategy")
    args = parser.parse_args()

    global TOPIC_TELEMETRY, TOPIC_CONTROL
    TOPIC_TELEMETRY = f"{args.id}/telemetry"
    TOPIC_CONTROL   = f"{args.id}/control"

    strategy = STRATEGIES[args.strategy]()
    rc       = RemoteControl()

    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION1,
        client_id="truck_remote",
        clean_session=True,
        userdata={"broker": args.broker, "strategy": strategy, "rc": rc},
    )
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        client.connect(args.broker, args.port, keepalive=60)
    except Exception as e:
        raise SystemExit(f"[ERROR] Cannot connect to {args.broker}:{args.port} — {e}")

    print(f"[INFO] Connecting to {args.broker}:{args.port}  vehicle={args.id}")
    print(f"[INFO] Strategy: {args.strategy}")
    client.loop_forever()


if __name__ == "__main__":
    main()
