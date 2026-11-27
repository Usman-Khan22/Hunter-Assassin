"""
Hunter's Assassin - full facility map builder (50 x 35 tiles, 2400 x 1680 px).

Run:  python build_facility.py      (needs Pillow, uses generate_art.py for drawing)

The layout is defined below as rectangles that get carved out of solid wall,
so it is easy to move a room or corridor and rebuild.

Outputs:
  ../maps/facility.txt          50x35 grid the game can load (W wall, . room floor, , corridor floor)
  ../maps/objects.txt           separately placed objects: type tileX tileY rotation
  ../png/facility_base.png      2400x1680 base map (walls + floors only)
  ../mockups/facility_preview.png        2400x1680 base map + objects
  ../mockups/facility_preview_small.png  1200x840 overview of the preview
"""

import os
from collections import deque

from PIL import Image, ImageDraw, ImageFont

import generate_art as ga

HERE = os.path.dirname(os.path.abspath(__file__))
MAP_DIR = os.path.join(HERE, "..", "maps")
COLS, ROWS = 50, 35

# ---------------------------------------------------------------- layout
# Each entry: (x, y, w, h). Coordinates are in tiles, inclusive top-left.
ROOMS = {
    # top row
    "A_cells":       (1, 1, 9, 8),     # player start
    "B_storage":     (12, 1, 11, 8),
    "C_servers":     (25, 1, 10, 8),
    "D_security":    (38, 1, 11, 8),   # access card 1, terminal
    # middle row
    "E_lab":         (1, 13, 10, 9),
    "F_hall":        (15, 13, 17, 9),  # big central hall with pillars
    "G_control":     (36, 13, 13, 9),  # keypad terminal
    # bottom row
    "H_cargo":       (1, 26, 13, 8),
    "I_maintenance": (17, 26, 11, 8),  # access card 2
    "J_reactor":     (31, 26, 9, 8),
    "K_exit_lobby":  (43, 26, 6, 8),
}

# 2-tile-wide corridors
CORRIDORS = [
    (1, 10, 48, 2),    # north corridor
    (1, 23, 48, 2),    # south corridor
    (12, 10, 2, 15),   # west connector
    (33, 10, 2, 15),   # east connector
]

# 2-wide openings (doorways) through walls
OPENINGS = [
    # top rooms -> north corridor
    (4, 9, 2, 1), (17, 9, 2, 1), (29, 9, 2, 1), (42, 9, 2, 1),
    # B <-> C side door
    (23, 5, 2, 2),
    # middle rooms -> north corridor
    (5, 12, 2, 1), (22, 12, 2, 1), (44, 12, 2, 1),
    # middle rooms -> connectors
    (11, 16, 1, 2), (14, 18, 1, 2), (32, 15, 1, 2), (35, 18, 1, 2),
    # middle rooms -> south corridor
    (20, 22, 2, 1), (27, 22, 2, 1), (45, 22, 2, 1),
    # bottom rooms -> south corridor
    (6, 25, 2, 1), (21, 25, 2, 1), (35, 25, 2, 1), (45, 25, 2, 1),
    # J <-> K side door
    (40, 29, 3, 2),
    # exit gap in the outer wall
    (49, 29, 1, 2),
]

# 1-tile-wide stealth passages (no doors, good for sneaking past guards)
NARROW = [
    (10, 3, 2, 1),     # A <-> B
    (35, 4, 3, 1),     # C <-> D
    (14, 31, 3, 1),    # H <-> I
    (28, 28, 3, 1),    # I <-> J
    (3, 22, 1, 4),     # lab vent down to cargo bay (crosses south corridor)
]

# permanent pillars / partitions inside rooms (cover for stealth)
PILLARS = [
    (19, 15, 2, 2), (26, 15, 2, 2), (19, 19, 2, 1), (26, 19, 2, 1),  # central hall
    (6, 17, 1, 3),                                                    # lab partition
    (41, 15, 1, 4),                                                   # control room partition
    (34, 28, 3, 1),                                                   # reactor core wall
]


def build_grid():
    g = [["W"] * COLS for _ in range(ROWS)]

    def carve(rects, ch):
        for x, y, w, h in rects:
            for yy in range(y, y + h):
                for xx in range(x, x + w):
                    if 0 <= xx < COLS and 0 <= yy < ROWS:
                        g[yy][xx] = ch

    carve(ROOMS.values(), ".")
    carve(CORRIDORS, ",")
    carve(OPENINGS, ",")
    carve(NARROW, ",")
    carve(PILLARS, "W")
    return ["".join(r) for r in g]


# ---------------------------------------------------------------- objects
# type, tileX, tileY, rotation (degrees). Doors: 0 = in a horizontal wall, 90 = in a vertical wall.
OBJECTS = [
    ("assassin", 4, 4, 90),
    # security doors on key routes (two tiles each for 2-wide openings)
    ("security_door", 42, 9, 0), ("security_door", 43, 9, 0),       # security office
    ("security_door", 45, 25, 0), ("security_door", 46, 25, 0),     # exit lobby from corridor
    ("security_door", 40, 29, 90), ("security_door", 40, 30, 90),   # exit lobby from reactor
    ("security_door", 35, 18, 90), ("security_door", 35, 19, 90),   # control room
    ("exit_door", 49, 29, 90), ("exit_door", 49, 30, 90),
    # terminals
    ("security_terminal", 46, 2, 0),
    ("security_terminal", 46, 19, 0),
    ("security_terminal", 28, 2, 0),
    # collectibles
    ("access_card", 47, 7, 0),
    ("access_card", 25, 32, 0),
    # crates (obstacles / cover)
    ("cargo_crate", 13, 2, 0), ("cargo_crate", 14, 2, 0), ("cargo_crate", 13, 3, 0),
    ("cargo_crate", 20, 6, 0), ("cargo_crate", 21, 6, 0), ("cargo_crate", 21, 7, 0),
    ("cargo_crate", 3, 28, 0), ("cargo_crate", 4, 28, 0), ("cargo_crate", 3, 29, 0),
    ("cargo_crate", 9, 27, 0), ("cargo_crate", 10, 27, 0),
    ("cargo_crate", 8, 31, 0), ("cargo_crate", 9, 31, 0), ("cargo_crate", 9, 32, 0),
    ("cargo_crate", 18, 27, 0), ("cargo_crate", 26, 29, 0),
    ("cargo_crate", 8, 15, 0), ("cargo_crate", 16, 20, 0), ("cargo_crate", 30, 14, 0),
    # guards (patrol start positions)
    ("guard", 24, 10, 180),
    ("guard", 8, 23, 0),
    ("guard", 33, 17, 90),
    ("guard", 22, 17, 0),
    ("guard", 43, 5, 180),
    ("guard", 37, 30, 270),
    ("guard", 5, 30, 0),
]


def check_connected(m):
    """BFS from the player start: every floor tile must be reachable."""
    start = (4, 4)
    seen = {start}
    q = deque([start])
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < COLS and 0 <= ny < ROWS and m[ny][nx] != "W" and (nx, ny) not in seen:
                seen.add((nx, ny))
                q.append((nx, ny))
    floor = sum(1 for row in m for ch in row if ch != "W")
    return len(seen), floor


def render_preview(base, m, sprites):
    img = base.copy()
    T = ga.TILE
    for name, tx, ty, rot in OBJECTS:
        spr = sprites[name]
        if name in ("assassin", "guard"):
            # sprites face right at 0; positive rotation here = clockwise on screen
            spr = spr.rotate(-rot, expand=True, resample=Image.BICUBIC)
        elif rot:
            spr = spr.rotate(rot, expand=True, resample=Image.BICUBIC)
        x = tx * T + (T - spr.width) // 2
        y = ty * T + (T - spr.height) // 2
        img.alpha_composite(spr, (x, y))
    # room labels (preview only, not part of the game art)
    d = ImageDraw.Draw(img)
    try:
        font = ImageFont.truetype("bahnschrift.ttf", 22)
    except OSError:
        font = ImageFont.load_default(22)
    for name, (x, y, w, h) in ROOMS.items():
        label = name.split("_", 1)[1].replace("_", " ").upper()
        cx, cy = (x + w / 2) * T, (y + h) * T - 16
        d.text((cx, cy), label, font=font, fill=(150, 160, 185), anchor="mm")
    return img


def main():
    os.makedirs(MAP_DIR, exist_ok=True)
    m = build_grid()
    reached, floor = check_connected(m)
    print(f"connectivity: {reached}/{floor} floor tiles reachable from start")
    assert reached == floor, "some floor tiles are unreachable"

    with open(os.path.join(MAP_DIR, "facility.txt"), "w") as f:
        f.write("\n".join(m) + "\n")
    with open(os.path.join(MAP_DIR, "objects.txt"), "w") as f:
        f.write("# type tileX tileY rotation\n")
        for o in OBJECTS:
            f.write(" ".join(str(v) for v in o) + "\n")
        for name, tx, ty, _ in OBJECTS:
            assert m[ty][tx] != "W", f"{name} at ({tx},{ty}) is inside a wall"

    # Render the big map at lower supersampling to keep memory reasonable.
    ga.SS = 3
    base = ga.draw_base_map(m)
    ga.SS = 8
    assert base.size == (2400, 1680), base.size
    base.convert("RGB").save(os.path.join(ga.PNG_DIR, "facility_base.png"))

    sprites = {n: Image.open(os.path.join(ga.PNG_DIR, f"{n}.png")).convert("RGBA") for n in
               ["assassin", "guard", "cargo_crate", "security_terminal", "security_door",
                "exit_door", "access_card"]}
    prev = render_preview(base, m, sprites)
    prev.convert("RGB").save(os.path.join(ga.MOCK_DIR, "facility_preview.png"))
    prev.resize((1200, 840), Image.LANCZOS).convert("RGB").save(
        os.path.join(ga.MOCK_DIR, "facility_preview_small.png"))
    print("\n".join(m))


if __name__ == "__main__":
    main()
