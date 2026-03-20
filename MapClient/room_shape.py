"""
room_shape.py — Geometric room definitions for the truck simulator.

A RoomShape has:
  outer  : the enclosing boundary polygon (truck must stay inside)
  holes  : list of inner polygons the truck must stay outside (islands/walls)

Factory methods
---------------
  RoomShape.rectangle(w, h)
  RoomShape.square(size)
  RoomShape.l_shape(main_w, main_h, wing_w, wing_h)
  RoomShape.donut(outer_w, outer_h, inner_w, inner_h)
  RoomShape.circular_donut(outer_r, inner_r, n_pts=64)

All shapes are positioned with their bottom-left corner near the origin.
"""

import math
import random


# ── Low-level polygon geometry ────────────────────────────────────────────────

def _polygon_circle(cx, cy, r, n):
    """Return an n-vertex polygon that approximates a circle."""
    return [
        (cx + r * math.cos(2 * math.pi * i / n),
         cy + r * math.sin(2 * math.pi * i / n))
        for i in range(n)
    ]


def _ray_segment(ox, oy, dx, dy, x1, y1, x2, y2):
    """
    Distance t along ray (ox,oy)+(dx,dy)*t to segment (x1,y1)-(x2,y2).
    Returns math.inf if no valid intersection.
    """
    ex, ey = x2 - x1, y2 - y1
    denom  = dx * ey - dy * ex
    if abs(denom) < 1e-9:
        return math.inf
    t = ((x1 - ox) * ey - (y1 - oy) * ex) / denom
    u = ((x1 - ox) * dy - (y1 - oy) * dx) / denom
    return t if t > 1e-3 and 0.0 <= u <= 1.0 else math.inf


def _point_in_polygon(x, y, poly):
    """Ray-casting point-in-polygon test."""
    inside, j = False, len(poly) - 1
    for i, (xi, yi) in enumerate(poly):
        xj, yj = poly[j]
        if ((yi > y) != (yj > y)) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
            inside = not inside
        j = i
    return inside


def _poly_ray_dist(ox, oy, dx, dy, poly):
    """Minimum ray distance to any edge of a single polygon."""
    n = len(poly)
    return min(
        (_ray_segment(ox, oy, dx, dy, *poly[i], *poly[(i + 1) % n])
         for i in range(n)),
        default=math.inf,
    )


def _min_edge_dist(x, y, poly):
    """Minimum perpendicular distance from (x,y) to any edge of a polygon."""
    min_d, n = math.inf, len(poly)
    for i in range(n):
        x1, y1 = poly[i]
        x2, y2 = poly[(i + 1) % n]
        ex, ey = x2 - x1, y2 - y1
        seg2   = ex * ex + ey * ey
        t      = (max(0.0, min(1.0, ((x - x1) * ex + (y - y1) * ey) / seg2))
                  if seg2 > 1e-9 else 0.0)
        min_d  = min(min_d, math.hypot(x - x1 - t * ex, y - y1 - t * ey))
    return min_d


# ── RoomShape ─────────────────────────────────────────────────────────────────

class RoomShape:
    """
    A 2-D driveable area defined by an outer polygon and optional inner holes.

    The driveable area is everything that is:
      • inside  the outer polygon, AND
      • outside every hole polygon.

    This representation supports any shape:
      - simple convex/concave polygons (rectangle, L-shape, …)
      - shapes with voids (donut, room with pillar, …)
    """

    def __init__(self, outer, holes=None):
        """
        Parameters
        ----------
        outer : list of (x, y)  — outer boundary vertices (CCW order)
        holes : list of lists   — each inner list is a hole polygon (CW order)
        """
        self.outer = list(outer)
        self.holes = [list(h) for h in (holes or [])]

    # ── Factory methods ───────────────────────────────────────────────────────

    @classmethod
    def rectangle(cls, width, height):
        """Axis-aligned rectangle with bottom-left corner at the origin."""
        return cls([(0, 0), (width, 0), (width, height), (0, height)])

    @classmethod
    def square(cls, size):
        """Square room."""
        return cls.rectangle(size, size)

    @classmethod
    def l_shape(cls, main_w=120, main_h=160, wing_w=60, wing_h=80):
        """
        L-shaped room: a wide main body with a narrower wing rising from
        the left side.

        (0, main_h+wing_h) ── (wing_w, main_h+wing_h)
               │                        │
               │          wing          │
        (0, main_h)     (wing_w, main_h) ── (main_w, main_h)
               │                                   │
               │            main body              │
        (0,  0) ─────────────────────── (main_w, 0)
        """
        top = main_h + wing_h
        return cls([
            (0,      0),
            (main_w, 0),
            (main_w, main_h),
            (wing_w, main_h),
            (wing_w, top),
            (0,      top),
        ])

    @classmethod
    def donut(cls, outer_w=240, outer_h=240, inner_w=100, inner_h=100):
        """
        Rectangular donut: a rectangular room with a centred rectangular
        island that the truck must drive around.

        Outer boundary (CCW):
          (0,0) → (outer_w,0) → (outer_w,outer_h) → (0,outer_h)

        Inner hole (CW so it reads as a void):
          centred rectangle of size inner_w × inner_h.
        """
        outer = [(0, 0), (outer_w, 0), (outer_w, outer_h), (0, outer_h)]
        ox    = (outer_w - inner_w) / 2
        oy    = (outer_h - inner_h) / 2
        # CW winding for the hole
        hole  = [
            (ox,            oy),
            (ox,            oy + inner_h),
            (ox + inner_w,  oy + inner_h),
            (ox + inner_w,  oy),
        ]
        return cls(outer, holes=[hole])

    @classmethod
    def circular_donut(cls, outer_r=120, inner_r=50, n_pts=64):
        """
        Circular donut: a circular outer wall with a circular inner island.
        Both circles are centred at (outer_r, outer_r) so the shape sits
        near the origin.  n_pts controls the polygon resolution.
        """
        cx, cy = outer_r, outer_r
        outer  = _polygon_circle(cx, cy, outer_r, n_pts)
        # Reversed winding → hole
        hole   = list(reversed(_polygon_circle(cx, cy, inner_r, n_pts)))
        return cls(outer, holes=[hole])

    # ── Geometry queries ──────────────────────────────────────────────────────

    def contains(self, x, y):
        """
        Return True if (x, y) is in the driveable area:
        inside the outer boundary and outside every hole.
        """
        if not _point_in_polygon(x, y, self.outer):
            return False
        return all(not _point_in_polygon(x, y, h) for h in self.holes)

    def ray_distance(self, ox, oy, dx, dy):
        """
        Distance from ray (ox,oy)+(dx,dy)*t to the nearest wall.
        Checks outer boundary edges and all hole edges.
        """
        d = _poly_ray_dist(ox, oy, dx, dy, self.outer)
        for hole in self.holes:
            d = min(d, _poly_ray_dist(ox, oy, dx, dy, hole))
        return d

    def clamp_position(self, ox, oy, nx, ny, margin=1.0):
        """
        Move from (ox, oy) toward (nx, ny), stopping `margin` cm before
        the first wall hit.  Returns the safe (x, y) to move to.
        """
        dx, dy = nx - ox, ny - oy
        dist   = math.hypot(dx, dy)
        if dist < 1e-9:
            return nx, ny
        udx, udy = dx / dist, dy / dist
        wall_t   = self.ray_distance(ox, oy, udx, udy)
        if wall_t < dist:
            safe = max(0.0, wall_t - margin)
            return ox + udx * safe, oy + udy * safe
        return nx, ny

    def min_wall_dist(self, x, y):
        """Minimum distance from (x, y) to any wall (outer or holes)."""
        d = _min_edge_dist(x, y, self.outer)
        for hole in self.holes:
            d = min(d, _min_edge_dist(x, y, hole))
        return d

    def random_point(self, margin=20.0, attempts=2000):
        """
        Return a random driveable point at least `margin` cm from every wall.
        Falls back to the centroid of the outer polygon if sampling fails.
        """
        xs = [p[0] for p in self.outer]
        ys = [p[1] for p in self.outer]
        x_lo, x_hi = min(xs) + margin, max(xs) - margin
        y_lo, y_hi = min(ys) + margin, max(ys) - margin
        for _ in range(attempts):
            x = random.uniform(x_lo, x_hi)
            y = random.uniform(y_lo, y_hi)
            if self.contains(x, y) and self.min_wall_dist(x, y) >= margin:
                return x, y
        return sum(xs) / len(xs), sum(ys) / len(ys)   # centroid fallback

    # ── Metadata ──────────────────────────────────────────────────────────────

    @property
    def bounds(self):
        """(x_min, y_min, x_max, y_max) of the outer bounding box."""
        xs = [p[0] for p in self.outer]
        ys = [p[1] for p in self.outer]
        return min(xs), min(ys), max(xs), max(ys)

    @property
    def all_polygons(self):
        """Outer polygon followed by all holes — useful for drawing."""
        return [self.outer] + self.holes
