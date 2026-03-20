#!/usr/bin/env python3
"""
Particle Localizer — Live MQTT Visualizer
==========================================
Subscribes to a Paho MQTT broker and renders a live top-down map
showing estimated room walls, particle position, sensor beams, trail,
compass, IMU bars, and raw telemetry.

Requirements:
    pip install paho-mqtt pygame

Usage:
    python particle_visualizer.py [--host HOST] [--port PORT] [--topic TOPIC]
    python particle_visualizer.py --host broker.hivemq.com --port 1883 --topic particle/sensor
"""

import argparse
import json
import math
import threading
import time
import sys
from collections import deque

try:
    import paho.mqtt.client as mqtt
except ImportError:
    sys.exit("Missing paho-mqtt.  Run:  pip install paho-mqtt")

try:
    import pygame
    import pygame.gfxdraw
except ImportError:
    sys.exit("Missing pygame.  Run:  pip install pygame")

# ── Palette ────────────────────────────────────────────────────────────────────
BG        = (8,  12,  15)
PANEL     = (13, 19,  23)
BORDER    = (26, 42,  53)
ACCENT    = (0,  229, 255)
ACCENT2   = (255, 107, 53)
ACCENT3   = (57,  255, 20)
WARN      = (255, 204,  0)
DIM       = (30,  48,  64)
TEXT      = (138, 180, 200)
TEXTBRIGHT= (204, 232, 244)
WHITE     = (255, 255, 255)
BLACK     = (0,   0,   0)

def alpha(color, a):
    """Return color as RGBA tuple."""
    return (*color, a)

# ── Layout constants ────────────────────────────────────────────────────────────
WIN_W, WIN_H = 1400, 820
LEFT_W  = 260
RIGHT_W = 300
MAP_W   = WIN_W - LEFT_W - RIGHT_W
MAP_H   = WIN_H - 48       # minus header

HEADER_H = 48


# ═══════════════════════════════════════════════════════════════════════════════
class State:
    """Thread-safe shared state between MQTT thread and render thread."""

    def __init__(self):
        self._lock = threading.Lock()
        self.latest: dict | None = None
        self.pkt_count = 0
        self.connected = False
        self.log_entries: deque = deque(maxlen=60)
        self.freq_samples: deque = deque(maxlen=20)
        self._last_pkt_time: float = 0.0

        # Position / room estimates
        self.pos_x: float | None = None
        self.pos_y: float | None = None
        self.heading: float = 0.0
        self.room_w: float | None = None
        self.room_h: float | None = None
        self.trail: deque = deque(maxlen=400)
        self.wall_hits: deque = deque(maxlen=200)   # (wx, wy, side)

    def push(self, data: dict):
        with self._lock:
            now = time.monotonic()
            if self._last_pkt_time:
                dt = now - self._last_pkt_time
                if dt > 0:
                    self.freq_samples.append(1.0 / dt)
            self._last_pkt_time = now
            self.latest = data
            self.pkt_count += 1
            self._update_position(data)

    def _update_position(self, d):
        ul = d.get("ul") or 0
        ur = d.get("ur") or 0
        uf = d.get("uf") or 0
        ub = d.get("ub") or 0
        compass = d.get("compass")
        if compass is not None:
            self.heading = compass

        # Room estimate
        w = ul + ur
        h = uf + ub
        if 30 < w < 2000:
            self.room_w = w if self.room_w is None else self.room_w * 0.92 + w * 0.08
        if 30 < h < 2000:
            self.room_h = h if self.room_h is None else self.room_h * 0.92 + h * 0.08

        # Position anchored to walls
        raw_x = ul
        raw_y = ub
        if self.pos_x is None:
            self.pos_x, self.pos_y = raw_x, raw_y
        else:
            self.pos_x = self.pos_x * 0.7 + raw_x * 0.3
            self.pos_y = self.pos_y * 0.7 + raw_y * 0.3

        self.trail.append((self.pos_x, self.pos_y))

        # Wall hit points (sensor frame → world frame)
        hr = (self.heading - 90) * math.pi / 180
        cos_h, sin_h = math.cos(hr), math.sin(hr)

        def rotate(lx, ly):
            return (
                self.pos_x + cos_h * lx - sin_h * ly,
                self.pos_y + sin_h * lx + cos_h * ly,
            )

        sensor_dirs = {
            "ul": rotate(-ul, 0),
            "ur": rotate( ur, 0),
            "uf": rotate(0,  uf),
            "ub": rotate(0, -ub),
        }
        for side, pt in sensor_dirs.items():
            self.wall_hits.append((pt[0], pt[1], side))

    def log(self, msg: str, kind: str = ""):
        ts = time.strftime("%H:%M:%S")
        with self._lock:
            self.log_entries.appendleft((ts, msg, kind))

    def snapshot(self):
        with self._lock:
            return dict(
                latest    = self.latest,
                pkt_count = self.pkt_count,
                connected = self.connected,
                log       = list(self.log_entries),
                freq      = list(self.freq_samples),
                pos_x     = self.pos_x,
                pos_y     = self.pos_y,
                heading   = self.heading,
                room_w    = self.room_w,
                room_h    = self.room_h,
                trail     = list(self.trail),
                wall_hits = list(self.wall_hits),
            )


# ═══════════════════════════════════════════════════════════════════════════════
class MQTTWorker(threading.Thread):
    def __init__(self, state: State, host, port, topic, username=None, password=None):
        super().__init__(daemon=True)
        self.state    = state
        self.host     = host
        self.port     = port
        self.topic    = topic
        self.username = username
        self.password = password
        self._client  = None

    def run(self):
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self._client = client

        if self.username:
            client.username_pw_set(self.username, self.password)

        client.on_connect    = self._on_connect
        client.on_message    = self._on_message
        client.on_disconnect = self._on_disconnect

        self.state.log(f"Connecting to {self.host}:{self.port}…", "info")
        try:
            client.connect(self.host, self.port, keepalive=60)
            client.loop_forever()
        except Exception as e:
            self.state.log(f"Connection failed: {e}", "err")

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code == 0:
            self.state.connected = True
            self.state.log(f"Connected. Subscribing to '{self.topic}'", "info")
            client.subscribe(self.topic)
        else:
            self.state.log(f"Connect refused: code {reason_code}", "err")

    def _on_message(self, client, userdata, msg):
        try:
            data = json.loads(msg.payload.decode())
            self.state.push(data)
        except Exception as e:
            self.state.log(f"Bad payload: {e}", "warn")

    def _on_disconnect(self, client, userdata, flags, reason_code, properties):
        self.state.connected = False
        self.state.log("Disconnected", "warn")

    def stop(self):
        if self._client:
            self._client.disconnect()


# ═══════════════════════════════════════════════════════════════════════════════
class Renderer:
    def __init__(self, state: State):
        pygame.init()
        pygame.display.set_caption("PARTICLE LOCALIZER")
        self.screen = pygame.display.set_mode((WIN_W, WIN_H), pygame.RESIZABLE)
        self.clock  = pygame.time.Clock()
        self.state  = state

        # Fonts
        self.font_mono_sm = pygame.font.SysFont("Courier New", 10)
        self.font_mono_md = pygame.font.SysFont("Courier New", 12)
        self.font_mono_lg = pygame.font.SysFont("Courier New", 14)
        self.font_bold_sm = pygame.font.SysFont("Courier New", 11, bold=True)
        self.font_title   = pygame.font.SysFont("Courier New", 15, bold=True)

        # Map surface (composited separately)
        self.map_surf = pygame.Surface((MAP_W, MAP_H))

    # ── Main loop ──────────────────────────────────────────────────────────────
    def run(self):
        running = True
        while running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        running = False
                elif event.type == pygame.VIDEORESIZE:
                    pass  # pygame handles this automatically in newer versions

            snap = self.state.snapshot()
            self.screen.fill(BG)
            self._draw_header(snap)
            self._draw_left_panel(snap)
            self._draw_map(snap)
            self._draw_right_panel(snap)
            pygame.display.flip()
            self.clock.tick(30)

        pygame.quit()

    # ── Header ─────────────────────────────────────────────────────────────────
    def _draw_header(self, snap):
        pygame.draw.rect(self.screen, PANEL, (0, 0, WIN_W, HEADER_H))
        pygame.draw.line(self.screen, BORDER, (0, HEADER_H-1), (WIN_W, HEADER_H-1), 1)

        x = 16
        # Title
        self._text("PARTICLE LOCALIZER", x, 16, self.font_title, ACCENT)
        x += 200

        # Connection dot + label
        dot_color = ACCENT3 if snap["connected"] else DIM
        pygame.draw.circle(self.screen, dot_color, (x + 5, 24), 5)
        label = "CONNECTED" if snap["connected"] else "DISCONNECTED"
        self._text(label, x + 14, 18, self.font_mono_sm, TEXT)
        x += 130

        d = snap["latest"]
        if d:
            self._header_stat("SEQ",     str(d.get("seq", "—")),    x);     x += 100
            freq = snap["freq"]
            avg_freq = f"{sum(freq)/len(freq):.1f}" if freq else "—"
            self._header_stat("FREQ",    avg_freq + " Hz",           x);     x += 130
            self._header_stat("HEADING", f"{d.get('compass', 0):.1f}°", x); x += 140

        # Right side
        pkt_txt = f"PACKETS  {snap['pkt_count']}"
        self._text(pkt_txt, WIN_W - 150, 18, self.font_mono_sm, WARN)

    def _header_stat(self, label, val, x):
        self._text(label, x, 12, self.font_mono_sm, TEXT)
        self._text(val,   x, 26, self.font_mono_sm, ACCENT)

    # ── Left panel ─────────────────────────────────────────────────────────────
    def _draw_left_panel(self, snap):
        x0, y0 = 0, HEADER_H
        pygame.draw.rect(self.screen, PANEL, (x0, y0, LEFT_W, WIN_H - HEADER_H))
        pygame.draw.line(self.screen, BORDER, (LEFT_W, y0), (LEFT_W, WIN_H), 1)

        y = y0 + 12

        # Section: Ultrasonic
        y = self._section_header("ULTRASONIC (cm)", x0+8, y)
        y = self._draw_ultrasonic_display(snap, x0+8, y, LEFT_W-16, 170)
        y += 8

        # Section: Compass
        y = self._section_header("COMPASS", x0+8, y)
        y = self._draw_compass(snap, x0+8, y, LEFT_W-16, 120)
        y += 8

        # Section: Event log
        y = self._section_header("EVENT LOG", x0+8, y)
        self._draw_log(snap, x0+8, y, LEFT_W-16, WIN_H - y - 10)

    def _draw_ultrasonic_display(self, snap, x, y, w, h):
        d = snap["latest"]
        surf = pygame.Surface((w, h), pygame.SRCALPHA)
        cx, cy = w//2, h//2 + 10
        max_r  = min(cx, cy) - 20
        max_dist = 500.0

        # Grid rings
        for frac in [0.25, 0.5, 0.75, 1.0]:
            r = int(max_r * frac)
            pygame.draw.circle(surf, (*ACCENT, 20), (cx, cy), r, 1)
        # Crosshair
        pygame.draw.line(surf, (*ACCENT, 25), (cx - max_r, cy), (cx + max_r, cy), 1)
        pygame.draw.line(surf, (*ACCENT, 25), (cx, cy - max_r), (cx, cy + max_r), 1)

        sensors = [
            ("uf", -math.pi/2, ACCENT,  "F"),
            ("ub",  math.pi/2, ACCENT,  "B"),
            ("ul",  math.pi,   ACCENT2, "L"),
            ("ur",  0,         ACCENT2, "R"),
        ]

        if d:
            for key, angle, color, label in sensors:
                v = d.get(key)
                if v is None:
                    continue
                r = min(v / max_dist, 1.0) * max_r
                ex = int(cx + math.cos(angle) * r)
                ey = int(cy + math.sin(angle) * r)

                # Cone fill
                cone_surf = pygame.Surface((w, h), pygame.SRCALPHA)
                spread = 0.18
                pts = [(cx, cy)]
                steps = 12
                for i in range(steps + 1):
                    a = angle - spread + (2 * spread * i / steps)
                    pts.append((cx + math.cos(a)*r, cy + math.sin(a)*r))
                if len(pts) > 2:
                    pygame.draw.polygon(cone_surf, (*color, 30), pts)
                surf.blit(cone_surf, (0, 0))

                # Beam
                pygame.draw.line(surf, (*color, 180), (cx, cy), (ex, ey), 2)
                # Endpoint
                pygame.draw.circle(surf, color, (ex, ey), 4)

                # Label
                lx = int(cx + math.cos(angle) * (max_r + 14))
                ly = int(cy + math.sin(angle) * (max_r + 14))
                txt = f"{label}:{v:.0f}"
                lbl = self.font_mono_sm.render(txt, True, color)
                surf.blit(lbl, (lx - lbl.get_width()//2, ly - lbl.get_height()//2))

        # Center particle
        pygame.draw.circle(surf, ACCENT3, (cx, cy), 5)
        pygame.draw.circle(surf, (*ACCENT3, 80), (cx, cy), 9, 1)

        self.screen.blit(surf, (x, y))
        return y + h

    def _draw_compass(self, snap, x, y, w, h):
        d = snap["latest"]
        surf = pygame.Surface((w, h), pygame.SRCALPHA)
        cx, cy = w//2, h//2
        r = min(cx, cy) - 8

        # Ring
        pygame.draw.circle(surf, (*BORDER, 255), (cx, cy), r, 1)

        # Tick marks
        for i in range(36):
            a = (i / 36) * math.pi * 2 - math.pi / 2
            inner = r - (10 if i % 9 == 0 else 5)
            x1 = cx + math.cos(a) * inner
            y1 = cy + math.sin(a) * inner
            x2 = cx + math.cos(a) * r
            y2 = cy + math.sin(a) * r
            color = (*ACCENT, 120) if i % 9 == 0 else (*BORDER, 200)
            pygame.draw.line(surf, color, (int(x1), int(y1)), (int(x2), int(y2)), 1 + (i%9==0))

        # Cardinal labels
        for i, label in enumerate(["N","E","S","W"]):
            a = (i / 4) * math.pi * 2 - math.pi / 2
            lx = int(cx + math.cos(a) * (r - 18))
            ly = int(cy + math.sin(a) * (r - 18))
            color = ACCENT2 if label == "N" else DIM
            lbl = self.font_bold_sm.render(label, True, color)
            surf.blit(lbl, (lx - lbl.get_width()//2, ly - lbl.get_height()//2))

        # Heading needle
        deg = d.get("compass", snap["heading"]) if d else snap["heading"]
        a = (deg - 90) * math.pi / 180
        nx = int(cx + math.cos(a) * (r - 20))
        ny = int(cy + math.sin(a) * (r - 20))
        tx = int(cx - math.cos(a) * 14)
        ty = int(cy - math.sin(a) * 14)
        pygame.draw.line(surf, ACCENT2, (cx, cy), (nx, ny), 2)
        pygame.draw.line(surf, (*DIM, 180), (cx, cy), (tx, ty), 2)
        pygame.draw.circle(surf, ACCENT2, (cx, cy), 4)

        # Degree text
        deg_lbl = self.font_mono_sm.render(f"{deg:.1f}°", True, ACCENT)
        surf.blit(deg_lbl, (cx - deg_lbl.get_width()//2, cy + r - 16))

        self.screen.blit(surf, (x, y))
        return y + h

    def _draw_log(self, snap, x, y, w, h):
        colors = {"info": ACCENT3, "warn": WARN, "err": ACCENT2, "": TEXT}
        for i, (ts, msg, kind) in enumerate(snap["log"][:int(h//14)]):
            color = colors.get(kind, TEXT)
            line = f"[{ts}] {msg}"
            lbl  = self.font_mono_sm.render(line[:48], True, color)
            self.screen.blit(lbl, (x, y + i*14))

    # ── Map ────────────────────────────────────────────────────────────────────
    def _draw_map(self, snap):
        x0 = LEFT_W
        y0 = HEADER_H
        map_w = WIN_W - LEFT_W - RIGHT_W
        map_h = WIN_H - HEADER_H

        surf = pygame.Surface((map_w, map_h), pygame.SRCALPHA)
        surf.fill(BG)

        rW = snap["room_w"]
        rH = snap["room_h"]
        margin = 60

        if rW and rH and rW > 0 and rH > 0:
            scale = min((map_w - margin*2) / rW, (map_h - margin*2) / rH)
            ox = (map_w - rW * scale) / 2
            oy = (map_h - rH * scale) / 2
        else:
            scale = 1.0
            ox, oy = margin, margin
            rW = rH = None

        def to_screen(wx, wy):
            return (int(ox + wx * scale), int(oy + wy * scale))

        # Grid
        grid_cm = 50
        grid_px = grid_cm * scale
        if grid_px > 8 and rW:
            gx = ox
            while gx < ox + rW * scale:
                pygame.draw.line(surf, (*ACCENT, 12), (int(gx), int(oy)), (int(gx), int(oy + rH * scale)), 1)
                gx += grid_px
            gy = oy
            while gy < oy + rH * scale:
                pygame.draw.line(surf, (*ACCENT, 12), (int(ox), int(gy)), (int(ox + rW * scale), int(gy)), 1)
                gy += grid_px

        # Room walls
        if rW and rH:
            rx, ry = int(ox), int(oy)
            rw2, rh2 = int(rW * scale), int(rH * scale)

            # Glow behind wall
            glow_surf = pygame.Surface((map_w, map_h), pygame.SRCALPHA)
            pygame.draw.rect(glow_surf, (*ACCENT, 18), (rx-4, ry-4, rw2+8, rh2+8), 8)
            surf.blit(glow_surf, (0, 0))

            pygame.draw.rect(surf, (*ACCENT, 180), (rx, ry, rw2, rh2), 2)

            # Corner brackets
            for cx2, cy2 in [(rx,ry),(rx+rw2,ry),(rx,ry+rh2),(rx+rw2,ry+rh2)]:
                sx = -1 if cx2 > rx else 1
                sy = -1 if cy2 > ry else 1
                pygame.draw.line(surf, ACCENT, (cx2, cy2), (cx2 + sx*12, cy2), 2)
                pygame.draw.line(surf, ACCENT, (cx2, cy2), (cx2, cy2 + sy*12), 2)

            # Dimension labels
            w_lbl = self.font_mono_sm.render(f"{rW:.0f} cm", True, (*ACCENT, 160))
            surf.blit(w_lbl, (rx + rw2//2 - w_lbl.get_width()//2, ry - 18))
            h_lbl = self.font_mono_sm.render(f"{rH:.0f} cm", True, (*ACCENT, 160))
            rot   = pygame.transform.rotate(h_lbl, 90)
            surf.blit(rot, (rx - 20, ry + rh2//2 - rot.get_height()//2))

        # Wall hit points
        for wx, wy, side in snap["wall_hits"]:
            sx, sy = to_screen(wx, wy)
            if 0 <= sx < map_w and 0 <= sy < map_h:
                pygame.draw.circle(surf, (*ACCENT, 60), (sx, sy), 3)

        # Trail
        trail = snap["trail"]
        if len(trail) > 1:
            pts = [to_screen(x, y) for x, y in trail]
            for i in range(1, len(pts)):
                alpha_v = int(40 * i / len(pts))
                pygame.draw.line(surf, (*ACCENT3, alpha_v + 10), pts[i-1], pts[i], 2)
            # Trail dots
            for i, pt in enumerate(pts[::6]):
                a = int(80 * i * 6 / max(len(pts),1))
                pygame.draw.circle(surf, (*ACCENT3, a), pt, 2)

        # Sensor beams from particle
        d = snap["latest"]
        px, py = snap["pos_x"], snap["pos_y"]
        if d and px is not None:
            sx0, sy0 = to_screen(px, py)
            hr = (snap["heading"] - 90) * math.pi / 180
            cos_h, sin_h = math.cos(hr), math.sin(hr)

            sensor_dirs_map = {
                "uf": ( 0,   1),
                "ub": ( 0,  -1),
                "ul": (-1,   0),
                "ur": ( 1,   0),
            }
            for key, (lx, ly) in sensor_dirs_map.items():
                v = d.get(key)
                if v is None: continue
                wx2 = cos_h * lx - sin_h * ly
                wy2 = sin_h * lx + cos_h * ly
                ex = int(sx0 + wx2 * v * scale)
                ey = int(sy0 + wy2 * v * scale)
                color = ACCENT2 if key in ("ul","ur") else ACCENT

                # Dashed beam
                seg_len, gap_len = 5, 4
                total = max(1, int(math.hypot(ex-sx0, ey-sy0)))
                steps = total // (seg_len + gap_len)
                for s in range(steps):
                    t0 = s * (seg_len + gap_len) / total
                    t1 = (s * (seg_len + gap_len) + seg_len) / total
                    p1 = (int(sx0 + (ex-sx0)*t0), int(sy0 + (ey-sy0)*t0))
                    p2 = (int(sx0 + (ex-sx0)*t1), int(sy0 + (ey-sy0)*t1))
                    pygame.draw.line(surf, (*color, 100), p1, p2, 1)

                pygame.draw.circle(surf, (*color, 160), (ex, ey), 4)

            # Heading arrow
            ax = int(sx0 + cos_h * 22)
            ay = int(sy0 + sin_h * 22)
            pygame.draw.line(surf, ACCENT2, (sx0, sy0), (ax, ay), 2)
            # Arrow head
            perp = (-sin_h * 5, cos_h * 5)
            pygame.draw.polygon(surf, ACCENT2, [
                (ax, ay),
                (ax - int(cos_h*8 + perp[0]), ay - int(sin_h*8 + perp[1])),
                (ax - int(cos_h*8 - perp[0]), ay - int(sin_h*8 - perp[1])),
            ])

            # Glow rings
            for r_glow, a_glow in [(20,30),(14,50),(8,80)]:
                pygame.draw.circle(surf, (*ACCENT3, a_glow), (sx0, sy0), r_glow, 1)

            # Particle dot
            pygame.draw.circle(surf, ACCENT3, (sx0, sy0), 6)

        # No-data overlay
        if not d:
            ov = pygame.Surface((map_w, map_h), pygame.SRCALPHA)
            ov.fill((0, 0, 0, 120))
            surf.blit(ov, (0,0))
            msg1 = self.font_title.render("AWAITING SENSOR DATA", True, DIM)
            msg2 = self.font_mono_sm.render("Connect to MQTT broker to begin", True, BORDER)
            surf.blit(msg1, (map_w//2 - msg1.get_width()//2, map_h//2 - 16))
            surf.blit(msg2, (map_w//2 - msg2.get_width()//2, map_h//2 + 10))

        self.screen.blit(surf, (x0, y0))

    # ── Right panel ────────────────────────────────────────────────────────────
    def _draw_right_panel(self, snap):
        x0 = WIN_W - RIGHT_W
        y0 = HEADER_H
        pygame.draw.rect(self.screen, PANEL, (x0, y0, RIGHT_W, WIN_H - HEADER_H))
        pygame.draw.line(self.screen, BORDER, (x0, y0), (x0, WIN_H), 1)

        d    = snap["latest"]
        y    = y0 + 12
        pad  = 12

        # ── Position ──
        y = self._section_header("ESTIMATED POSITION", x0+pad, y)
        rows = [
            ("X",       f"{snap['pos_x']:.0f} cm"      if snap['pos_x'] is not None else "—"),
            ("Y",       f"{snap['pos_y']:.0f} cm"      if snap['pos_y'] is not None else "—"),
            ("HEADING", f"{snap['heading']:.1f}°"),
        ]
        for label, val in rows:
            self._text(label, x0+pad,         y, self.font_mono_sm, TEXT)
            self._text(val,   x0+RIGHT_W-pad, y, self.font_mono_sm, ACCENT, right=True)
            y += 16
        y += 8

        # ── Room ──
        y = self._section_header("ROOM ESTIMATE", x0+pad, y)
        rW, rH = snap["room_w"], snap["room_h"]
        room_rows = [
            ("WIDTH",  f"{rW:.0f} cm"                  if rW else "—"),
            ("HEIGHT", f"{rH:.0f} cm"                  if rH else "—"),
            ("AREA",   f"{rW*rH/10000:.2f} m²"         if rW and rH else "—"),
        ]
        for label, val in room_rows:
            self._text(label, x0+pad,         y, self.font_mono_sm, TEXT)
            self._text(val,   x0+RIGHT_W-pad, y, self.font_mono_sm, ACCENT3, right=True)
            y += 16
        y += 8

        # ── Range sensors ──
        y = self._section_header("RANGE SENSORS", x0+pad, y)
        sensors = [("FWD","uf"),("BCK","ub"),("LFT","ul"),("RGT","ur")]
        for i in range(0, 4, 2):
            for j in range(2):
                lbl, key = sensors[i+j]
                val = f"{d.get(key):.0f}" if d and d.get(key) is not None else "—"
                bx = x0 + pad + j * (RIGHT_W - pad*2)//2
                self._text(lbl, bx, y,    self.font_mono_sm, TEXT)
                self._text(val, bx, y+14, self.font_mono_md, ACCENT)
            y += 36
        y += 4

        # ── IMU bars ──
        y = self._section_header("IMU", x0+pad, y)
        imu_fields = [
            ("ACC X", "acc_x", 20),
            ("ACC Y", "acc_y", 20),
            ("ACC Z", "acc_z", 20),
            ("GY Z",  "gy_z",  500),
            ("YAW",   "yaw_rate", 180),
        ]
        bar_w = RIGHT_W - pad*2
        for label, key, max_v in imu_fields:
            v = d.get(key, 0) if d else 0
            pct = min(abs(v) / max_v, 1.0)
            self._text(f"{label}", x0+pad, y, self.font_mono_sm, TEXT)
            self._text(f"{v:.2f}", x0+RIGHT_W-pad, y, self.font_mono_sm, ACCENT, right=True)
            y += 13
            # bar track
            pygame.draw.rect(self.screen, BG,     (x0+pad, y, bar_w, 5))
            pygame.draw.rect(self.screen, BORDER, (x0+pad, y, bar_w, 5), 1)
            fill_w = int(pct * (bar_w//2))
            color  = ACCENT if v >= 0 else ACCENT2
            bx_fill = x0 + pad + bar_w//2 if v >= 0 else x0 + pad + bar_w//2 - fill_w
            pygame.draw.rect(self.screen, color, (bx_fill, y, fill_w, 5))
            y += 9
        y += 6

        # ── Raw JSON ──
        y = self._section_header("RAW PACKET", x0+pad, y)
        if d:
            for key, val in list(d.items())[:int((WIN_H - y - 10) // 13)]:
                line = f"{key}: {val}"
                lbl  = self.font_mono_sm.render(line[:36], True, DIM)
                self.screen.blit(lbl, (x0+pad, y))
                y += 13

    # ── Utilities ──────────────────────────────────────────────────────────────
    def _text(self, text, x, y, font, color, right=False):
        surf = font.render(str(text), True, color)
        if right:
            self.screen.blit(surf, (x - surf.get_width(), y))
        else:
            self.screen.blit(surf, (x, y))

    def _section_header(self, label, x, y):
        pygame.draw.line(self.screen, BORDER, (x, y+6), (x + LEFT_W - 16 if x < LEFT_W else RIGHT_W-16), y+6, 1)
        lbl = self.font_mono_sm.render(f"── {label} ──", True, ACCENT)
        self.screen.blit(lbl, (x, y))
        return y + 18


# ═══════════════════════════════════════════════════════════════════════════════
def demo_thread(state: State):
    """Simulates a particle in a 400×300 cm room for testing without a broker."""
    state.log("DEMO MODE — simulating particle in 400×300 cm room", "info")
    t = 0.0
    angle = 0.0
    while True:
        t     += 0.05
        angle  = (angle + 1.2) % 360
        x      = 200 + 80 * math.sin(t * 0.7)
        y      = 150 + 60 * math.cos(t * 0.5)
        room_w, room_h = 400, 300
        compass = (angle + math.sin(t) * 15) % 360

        data = {
            "seq":      int(t * 20),
            "t_ms":     int(time.time() * 1000),
            "ul":       round(x + (math.random() if False else 0), 1),
            "ur":       round(room_w - x, 1),
            "uf":       round(y, 1),
            "ub":       round(room_h - y, 1),
            "yaw_rate": round(math.sin(t) * 15, 2),
            "gy_x":     round(math.sin(t * 1.3) * 2, 2),
            "gy_y":     round(math.cos(t * 0.9) * 2, 2),
            "gy_z":     round(math.sin(t) * 5, 2),
            "compass":  round(compass, 1),
            "mag_x":    round(math.cos(math.radians(compass)) * 30, 2),
            "mag_y":    round(math.sin(math.radians(compass)) * 30, 2),
            "mag_z":    round(-45.0, 2),
            "acc_x":    round(math.cos(t) * 2, 2),
            "acc_y":    round(math.sin(t) * 2, 2),
            "acc_z":    round(9.81, 2),
        }
        state.push(data)
        time.sleep(0.1)


# ═══════════════════════════════════════════════════════════════════════════════
def main():
    parser = argparse.ArgumentParser(description="Particle Localizer — MQTT Visualizer")
    parser.add_argument("--host",     default="broker.hivemq.com", help="MQTT broker host")
    parser.add_argument("--port",     type=int, default=1883,      help="MQTT broker port")
    parser.add_argument("--topic",    default="particle/sensor",   help="MQTT topic to subscribe")
    parser.add_argument("--user",     default=None,                help="MQTT username")
    parser.add_argument("--password", default=None,                help="MQTT password")
    parser.add_argument("--demo",     action="store_true",         help="Run in demo mode (no broker needed)")
    args = parser.parse_args()

    state = State()

    if args.demo:
        t = threading.Thread(target=demo_thread, args=(state,), daemon=True)
        t.start()
        state.connected = True
    else:
        worker = MQTTWorker(
            state    = state,
            host     = args.host,
            port     = args.port,
            topic    = args.topic,
            username = args.user,
            password = args.password,
        )
        worker.start()

    renderer = Renderer(state)
    renderer.run()


if __name__ == "__main__":
    main()
