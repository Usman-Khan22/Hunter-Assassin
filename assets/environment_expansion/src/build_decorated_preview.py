"""
Hunter's Assassin - proposed room decoration, validation and previews.

Run:  python generate_environment.py   (first, makes the PNGs)
      python build_decorated_preview.py

Reads (never writes) the approved files:
  art_style_sample/maps/facility.txt, art_style_sample/maps/objects.txt,
  art_style_sample/png/*.png
Writes only inside environment_expansion/:
  maps/decoration_objects.txt          type tileX tileY rotation   (the proposal)
  maps/decoration_footprints.txt       every solid prop with its occupied tiles
  maps/decoration_collision_preview.txt  facility grid with proposed solid tiles marked P
  maps/validation_report.txt           results of the BFS / placement checks
  mockups/asset_sheet.png
  mockups/decorated_facility_preview.png (+ _small)
  mockups/gameplay_lab.png, gameplay_reactor.png, gameplay_security.png

Placement convention (same for every asset, see ASSET_SPECIFICATIONS.md):
  * footprint = (w, h) tiles at rotation 0; swapped for 90 / 270
  * (tileX, tileY) = TOP-LEFT tile of the footprint AFTER rotation
  * sprite centre = footprint centre + R(rotation) * (0, mount_dy)
    (mount_dy = 0 for floor props; negative for wall-mounted props)
  * rotation is clockwise on screen (raylib DrawTexturePro convention)
"""

import math
import os
import sys
from collections import deque

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))      # art_style_sample/
sys.path.insert(0, os.path.join(ROOT, "src"))
import generate_art as ga  # noqa: E402

T = 48
COLS, ROWS = 50, 35
ENV_PNG = os.path.join(HERE, "..", "png")
OLD_PNG = os.path.join(ROOT, "png")
OUT_MAPS = os.path.join(HERE, "..", "maps")
OUT_MOCK = os.path.join(HERE, "..", "mockups")

# ------------------------------------------------------------------ asset specs
# name: footprint tiles (w,h) at rot 0, solid?, mount ("floor" | "decal" | "wall"), wall inset px
SPECS = {
    "server_rack":      ((1, 1), True,  "floor", 0),
    "research_desk":    ((2, 1), True,  "floor", 0),
    "specimen_tank":    ((1, 1), True,  "floor", 0),
    "power_generator":  ((2, 2), True,  "floor", 0),
    "locker":           ((1, 1), True,  "floor", 0),
    "cell_bed":         ((2, 1), True,  "floor", 0),
    "tool_station":     ((2, 1), True,  "floor", 0),
    "control_desk":     ((2, 1), True,  "floor", 0),
    "cargo_container":  ((2, 1), True,  "floor", 0),
    "storage_rack":     ((2, 1), True,  "floor", 0),
    "floor_vent":       ((1, 1), False, "decal", 0),
    "hazard_marking":   ((1, 1), False, "decal", 0),
    "floor_arrow":      ((1, 1), False, "decal", 0),
    "floor_arrow_exit": ((1, 1), False, "decal", 0),
    "wall_console":     ((1, 1), False, "wall", 8),
    "wall_light":       ((1, 1), False, "wall", 6),
    "wall_pipe":        ((2, 1), False, "wall", 6),
}

# ------------------------------------------------------------------ the proposal
# (type, tileX, tileY, rotation)
DECOR = [
    # ---- A Cells: beds, lockers, lights
    ("cell_bed", 1, 1, 0), ("cell_bed", 6, 1, 0), ("cell_bed", 1, 4, 90),
    ("locker", 1, 8, 0), ("locker", 2, 8, 0), ("locker", 8, 8, 0), ("locker", 9, 8, 0),
    ("wall_light", 4, 1, 0), ("wall_light", 9, 6, 90),
    # ---- B Storage: storage racks, lockers (existing crates stay)
    ("storage_rack", 17, 1, 0), ("storage_rack", 19, 1, 0),
    ("storage_rack", 16, 4, 90), ("storage_rack", 19, 4, 90),
    ("locker", 21, 1, 0), ("locker", 22, 1, 0), ("locker", 22, 2, 0), ("locker", 12, 8, 0), ("locker", 13, 8, 0),
    ("wall_light", 14, 1, 0),
    # ---- C Servers: rack rows, vents, wall consoles
    ("server_rack", 31, 1, 0), ("server_rack", 32, 1, 0), ("server_rack", 33, 1, 0), ("server_rack", 34, 1, 0),
    ("server_rack", 27, 5, 0), ("server_rack", 28, 5, 0),
    ("server_rack", 31, 5, 0), ("server_rack", 32, 5, 0), ("server_rack", 33, 5, 0),
    ("floor_vent", 29, 4, 0), ("floor_vent", 30, 7, 0),
    ("wall_console", 26, 8, 180), ("wall_console", 33, 8, 180),
    # ---- D Security: monitoring desk + wall screens, lockers
    ("control_desk", 41, 2, 0), ("wall_console", 41, 1, 0), ("wall_console", 42, 1, 0),
    ("control_desk", 39, 6, 0), ("wall_console", 44, 1, 0),
    ("locker", 38, 1, 0), ("locker", 38, 2, 0), ("locker", 48, 1, 0),
    ("wall_light", 48, 4, 90),
    # ---- E Lab: research desks, specimen tanks, wall console
    ("specimen_tank", 1, 13, 0), ("specimen_tank", 2, 13, 0), ("specimen_tank", 3, 13, 0),
    ("specimen_tank", 1, 21, 0),
    ("research_desk", 9, 13, 0), ("research_desk", 1, 16, 90),
    ("research_desk", 3, 19, 0), ("research_desk", 8, 20, 0),
    ("wall_console", 10, 14, 90), ("wall_console", 4, 21, 180),
    # ---- F Central Hall: floor arrows, lights, a pipe, light cover at the edges
    ("floor_arrow", 16, 17, 0), ("floor_arrow", 29, 17, 0), ("floor_arrow", 21, 20, 90),
    ("wall_light", 17, 13, 0), ("wall_light", 29, 13, 0),
    ("wall_light", 17, 21, 180), ("wall_light", 24, 21, 180),
    ("wall_pipe", 24, 13, 0),
    ("locker", 15, 13, 0), ("locker", 16, 13, 0), ("server_rack", 31, 21, 0),
    # ---- G Control: control desks, wall consoles, monitoring racks
    ("control_desk", 37, 14, 0), ("wall_console", 37, 13, 0), ("wall_console", 38, 13, 0),
    ("control_desk", 48, 13, 90), ("control_desk", 43, 16, 0),
    ("server_rack", 36, 21, 0), ("server_rack", 37, 21, 0),
    ("wall_console", 48, 18, 90), ("wall_light", 40, 21, 180),
    # ---- H Cargo: containers, hazard paint (existing crates stay)
    ("cargo_container", 11, 29, 90), ("cargo_container", 12, 33, 0),
    ("cargo_container", 12, 26, 0), ("cargo_container", 1, 33, 0), ("cargo_container", 5, 33, 0),
    ("hazard_marking", 1, 32, 0), ("hazard_marking", 2, 32, 0),
    ("hazard_marking", 5, 32, 0), ("hazard_marking", 6, 32, 0),
    ("wall_light", 10, 26, 0),
    # ---- I Maintenance: generators, tool stations, vents
    ("power_generator", 23, 26, 0), ("power_generator", 20, 29, 0),
    ("tool_station", 19, 33, 0), ("tool_station", 26, 33, 0),
    ("hazard_marking", 23, 28, 0), ("hazard_marking", 24, 28, 0),
    ("floor_vent", 18, 30, 0), ("floor_vent", 25, 30, 0),
    ("wall_pipe", 18, 26, 0),
    # ---- J Reactor: generators, hazard ring round the core, pipe
    ("power_generator", 31, 32, 0), ("power_generator", 38, 32, 0), ("power_generator", 38, 26, 0),
    ("hazard_marking", 34, 27, 0), ("hazard_marking", 35, 27, 0), ("hazard_marking", 36, 27, 0),
    ("hazard_marking", 34, 29, 0), ("hazard_marking", 35, 29, 0), ("hazard_marking", 36, 29, 0),
    ("wall_pipe", 31, 26, 0), ("floor_vent", 33, 31, 0),
    # ---- K Exit Lobby: exit arrows, consoles, lights (kept open)
    ("floor_arrow_exit", 45, 29, 0), ("floor_arrow_exit", 47, 29, 0),
    ("floor_arrow_exit", 45, 30, 0), ("floor_arrow_exit", 47, 30, 0),
    ("wall_light", 48, 28, 90), ("wall_light", 48, 31, 90),
    ("wall_console", 44, 33, 180), ("wall_console", 47, 26, 0),
    # ---- corridors: decoration only (no collision)
    ("wall_light", 9, 10, 0), ("wall_light", 25, 10, 0), ("wall_light", 37, 10, 0),
    ("wall_light", 10, 24, 180), ("wall_light", 28, 24, 180), ("wall_light", 40, 24, 180),
    ("wall_pipe", 14, 11, 180), ("wall_pipe", 38, 23, 0),
    ("floor_vent", 26, 11, 0), ("floor_vent", 8, 24, 0), ("floor_vent", 12, 14, 0),
    ("hazard_marking", 45, 24, 0), ("hazard_marking", 46, 24, 0),
    ("floor_arrow", 41, 24, 0),
]

SOLID_EXISTING = {"cargo_crate", "security_terminal"}   # treated as blocking in the checks


# ------------------------------------------------------------------ geometry helpers
def rot_vec(dx, dy, rot):
    a = math.radians(rot)
    return (round(dx * math.cos(a) - dy * math.sin(a), 6), round(dx * math.sin(a) + dy * math.cos(a), 6))


def footprint(name, tx, ty, rot):
    (w, h), *_ = SPECS[name]
    if rot in (90, 270):
        w, h = h, w
    return [(tx + i, ty + j) for j in range(h) for i in range(w)], (w, h)


def sprite_center(name, tx, ty, rot, size):
    (fw, fh) = footprint(name, tx, ty, rot)[1]
    cx, cy = (tx + fw / 2) * T, (ty + fh / 2) * T
    _, _, mount, inset = SPECS[name]
    if mount == "wall":
        dy = -T / 2 - inset + size[1] / 2
        ox, oy = rot_vec(0, dy, rot)
        cx, cy = cx + ox, cy + oy
    return cx, cy


def mount_offset(name, size):
    _, _, mount, inset = SPECS[name]
    return 0 if mount != "wall" else -T / 2 - inset + size[1] / 2


# ------------------------------------------------------------------ loading
def load_map():
    with open(os.path.join(ROOT, "maps", "facility.txt")) as f:
        m = [ln.rstrip("\n") for ln in f if ln.strip()]
    assert len(m) == ROWS and all(len(r) == COLS for r in m)
    return m


def load_objects():
    objs = []
    with open(os.path.join(ROOT, "maps", "objects.txt")) as f:
        for ln in f:
            ln = ln.strip()
            if not ln or ln.startswith("#"):
                continue
            t, x, y, r = ln.split()
            objs.append((t, int(x), int(y), int(r)))
    return objs


# ------------------------------------------------------------------ validation
def validate(m, objs):
    rep, errors = [], []
    wall = lambda x, y: not (0 <= x < COLS and 0 <= y < ROWS) or m[y][x] == "W"
    N4 = ((1, 0), (-1, 0), (0, 1), (0, -1))

    existing_block = {(x, y) for t, x, y, _ in objs if t in SOLID_EXISTING}
    obj_tiles = {(x, y) for _, x, y, _ in objs}
    spawn = next((x, y) for t, x, y, _ in objs if t == "assassin")

    # tiles that solid props must keep clear
    keep = set()
    for y in range(ROWS):                      # every room tile touching a doorway / corridor / narrow passage
        for x in range(COLS):
            if m[y][x] == ",":
                for dx, dy in N4:
                    if not wall(x + dx, y + dy) and m[y + dy][x + dx] == ".":
                        keep.add((x + dx, y + dy))
    for t, x, y, _ in objs:
        if t in ("security_terminal", "access_card", "guard", "assassin", "security_door", "exit_door"):
            keep.add((x, y))
            keep.update((x + dx, y + dy) for dx, dy in N4)

    solid, decal_tiles, footprints = {}, [], []
    for name, tx, ty, rot in DECOR:
        tiles, _ = footprint(name, tx, ty, rot)
        _, is_solid, mount, _ = SPECS[name]
        tag = f"{name} {tx} {ty} {rot}"
        if rot not in (0, 90, 180, 270):
            errors.append(f"{tag}: rotation must be 0/90/180/270")
        for (x, y) in tiles:
            if wall(x, y):
                errors.append(f"{tag}: tile ({x},{y}) is a wall")
        if mount == "wall":
            bx, by = rot_vec(0, -1, rot)
            for (x, y) in tiles:
                if not wall(int(x + bx), int(y + by)):
                    errors.append(f"{tag}: no wall behind tile ({x},{y})")
        if is_solid:
            footprints.append((name, tx, ty, rot, tiles))
            for (x, y) in tiles:
                if wall(x, y):
                    continue
                if m[y][x] != ".":
                    errors.append(f"{tag}: ({x},{y}) is corridor / doorway / narrow passage floor")
                if (x, y) in obj_tiles:
                    errors.append(f"{tag}: ({x},{y}) overlaps an existing object")
                if (x, y) in keep:
                    errors.append(f"{tag}: ({x},{y}) is in a keep-clear zone (entrance, terminal, card, spawn)")
                if (x, y) in solid:
                    errors.append(f"{tag}: ({x},{y}) overlaps {solid[(x, y)]}")
                solid[(x, y)] = name
        elif mount == "decal":
            decal_tiles.extend(tiles)
    for t in decal_tiles:
        if t in solid:
            errors.append(f"decal at {t} is hidden under {solid[t]}")

    def free(x, y, extra):
        return not wall(x, y) and (x, y) not in existing_block and (x, y) not in extra

    def bfs(extra):
        seen, q = {spawn}, deque([spawn])
        while q:
            x, y = q.popleft()
            for dx, dy in N4:
                n = (x + dx, y + dy)
                if n not in seen and free(*n, extra):
                    seen.add(n)
                    q.append(n)
        return seen

    def dead_ends(extra):
        out = set()
        for y in range(ROWS):
            for x in range(COLS):
                if free(x, y, extra) and sum(free(x + dx, y + dy, extra) for dx, dy in N4) <= 1:
                    out.add((x, y))
        return out

    before, after = bfs(set()), bfs(set(solid))
    lost = (before - set(solid)) - after
    if lost:
        errors.append(f"{len(lost)} previously reachable tiles become unreachable: {sorted(lost)[:12]}")
    rep.append(f"Reachable free tiles from player spawn {spawn}: before {len(before)}, after {len(after)} "
               f"(difference = {len(before) - len(after)} tiles now covered by solid props: {len(solid)})")

    goals = []
    for t, x, y, _ in objs:
        if t in ("access_card", "security_door", "exit_door", "guard"):
            goals.append((t, (x, y), (x, y) in after))
        elif t == "security_terminal":
            goals.append((t, (x, y), any((x + dx, y + dy) in after for dx, dy in N4)))
    for t, p, ok in goals:
        if not ok:
            errors.append(f"objective {t} at {p} is not reachable")
    rep.append(f"Objectives / spawns checked: {len(goals)} (cards, terminals, security + exit doors, guard spawns) "
               f"- all reachable: {all(ok for *_, ok in goals)}")

    new_dead = dead_ends(set(solid)) - dead_ends(set())
    if new_dead:
        errors.append(f"solid props create new 1-tile dead-end pockets at {sorted(new_dead)}")
    rep.append(f"New 1-tile dead-end pockets created by props: {len(new_dead)}")

    # room openness: share of room floor still free
    room_floor = sum(1 for y in range(ROWS) for x in range(COLS) if m[y][x] == ".")
    rep.append(f"Room floor tiles: {room_floor}, covered by new solid props: {len(solid)} "
               f"({100 * len(solid) / room_floor:.1f}%), corridor / doorway / narrow tiles used by solids: 0")
    rep.append("Doors were treated as open (unlocked) for reachability; crates and terminals as solid.")
    rep.append("Guard PATROL routes are not in the supplied files: only guard spawn tiles and their "
               "neighbours were kept clear. Patrol compatibility has NOT been verified.")
    return rep, errors, footprints, solid


# ------------------------------------------------------------------ rendering
def load_sprites():
    s = {}
    for d in (OLD_PNG, ENV_PNG, os.path.join(ENV_PNG, "effects")):
        for f in os.listdir(d):
            if f.endswith(".png"):
                s[f[:-4]] = Image.open(os.path.join(d, f)).convert("RGBA")
    return s


def paste_center(img, spr, cx, cy):
    img.alpha_composite(spr, (int(round(cx - spr.width / 2)), int(round(cy - spr.height / 2))))


def place_decor(img, sprites, items):
    for name, tx, ty, rot in items:
        spr = sprites[name]
        cx, cy = sprite_center(name, tx, ty, rot, spr.size)
        if rot:
            spr = spr.rotate(-rot, expand=True)          # clockwise on screen
        paste_center(img, spr, cx, cy)


def place_object(img, sprites, name, tx, ty, rot):
    spr = sprites[name]
    if name in ("assassin", "guard", "laser_guard"):
        spr = spr.rotate(-rot, expand=True, resample=Image.BICUBIC)   # same as build_facility.py
    elif rot:
        spr = spr.rotate(rot, expand=True, resample=Image.BICUBIC)
    paste_center(img, spr, tx * T + T / 2, ty * T + T / 2)


def render_world(m, objs, sprites, solid, overrides=None, extra=()):
    """Base map -> floor decals -> wall-mounted -> solid props -> existing objects -> characters."""
    img = Image.open(os.path.join(OLD_PNG, "facility_base.png")).convert("RGBA")
    layer = {"decal": [], "wall": [], "floor": []}
    for d in DECOR:
        layer[SPECS[d[0]][2]].append(d)
    for k in ("decal", "wall", "floor"):
        place_decor(img, sprites, layer[k])
    overrides = overrides or {}
    items = [overrides.get((t, x, y), (t, x, y, r)) for t, x, y, r in objs] + list(extra)
    for t, x, y, r in items:
        if t not in ("assassin", "guard", "laser_guard"):
            place_object(img, sprites, t, x, y, r)
    return img, [i for i in items if i[0] in ("assassin", "guard", "laser_guard")]


def blocked_fn(m, objs, solid):
    blk = {(x, y) for t, x, y, _ in objs if t in SOLID_EXISTING} | set(solid)
    return lambda x, y: not (0 <= x < COLS and 0 <= y < ROWS) or m[y][x] == "W" or (x, y) in blk


def vision_cone(size, origin, angle, fov, radius, color, blocked):
    """Ray-cast cone that stops at walls and solid props (stand-in for the in-game cone)."""
    pts = [origin]
    for i in range(61):
        a = math.radians(angle - fov / 2 + fov * i / 60)
        r = 0
        while r < radius:
            r += 4
            px, py = origin[0] + r * math.cos(a), origin[1] + r * math.sin(a)
            if blocked(int(px // T), int(py // T)):
                break
        pts.append((origin[0] + r * math.cos(a), origin[1] + r * math.sin(a)))
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    ImageDraw.Draw(layer).polygon(pts, fill=color)
    return layer


def font(sz):
    try:
        return ImageFont.truetype("bahnschrift.ttf", sz)
    except OSError:
        return ImageFont.load_default(sz)


def room_labels(img):
    sys.path.insert(0, os.path.join(ROOT, "src"))
    import build_facility as bf
    d = ImageDraw.Draw(img)
    f = font(22)
    for name, (x, y, w, h) in bf.ROOMS.items():
        label = name.split("_", 1)[1].replace("_", " ").upper()
        d.text(((x + w / 2) * T, (y + h) * T - 16), label, font=f, fill=(150, 160, 185),
               anchor="mm", stroke_width=3, stroke_fill=(20, 24, 34))


# ------------------------------------------------------------------ previews
def hud(win, cards=0):
    P = ga.P
    d = ImageDraw.Draw(win)
    W = win.width
    for i in range(3):
        x = 32 + i * 30
        d.rounded_rectangle([x, 28, x + 22, 50], radius=5, fill=P["teal"] if i < 2 else P["steel_dark"],
                            outline=P["outline"], width=2)
    d.text((32, 58), "HEALTH", font=font(18), fill=(150, 160, 180))
    d.text((W // 2, 40), "04:37", font=font(30), fill=(230, 234, 242), anchor="mm")
    d.text((W // 2, 68), "LOCKDOWN", font=font(18), fill=P["yellow"], anchor="mm")
    card = Image.open(os.path.join(OLD_PNG, "access_card.png")).resize((36, 36), Image.LANCZOS)
    win.alpha_composite(card, (W - 150, 22))
    d.text((W - 108, 40), f"{cards} / 2", font=font(30), fill=(230, 234, 242), anchor="lm")
    d.text((W - 150, 66), "ACCESS CARDS", font=font(18), fill=(150, 160, 180))


def gameplay_view(world, chars, sprites, blocked, focus, cones, effects=(), cards=0):
    """1280x720 window, camera zoom 1.5 centred on `focus` (world px), clamped to the map."""
    img = world.copy()
    for (ox, oy, ang, col) in cones:
        img.alpha_composite(vision_cone(img.size, (ox, oy), ang, 70, 5 * T, col, blocked))
    for t, x, y, r in chars:
        place_object(img, sprites, t, x, y, r)
    for name, cx, cy, rot in effects:
        spr = sprites[name].rotate(-rot, expand=True, resample=Image.BICUBIC) if rot else sprites[name]
        paste_center(img, spr, cx, cy)
    vw, vh = 1280 / 1.5, 720 / 1.5
    x0 = min(max(focus[0] - vw / 2, 0), world.width - vw)
    y0 = min(max(focus[1] - vh / 2, 0), world.height - vh)
    crop = img.crop((int(x0), int(y0), int(x0) + 854, int(y0) + 480))
    win = crop.resize((1281, 720), Image.LANCZOS).crop((0, 0, 1280, 720))
    hud(win, cards)
    return win


def asset_sheet(sprites):
    groups = [
        ("APPROVED ASSETS", ["assassin", "guard", "cargo_crate", "security_terminal", "security_door",
                             "exit_door", "access_card"]),
        ("ENVIRONMENT - REQUIRED", ["wall_console", "server_rack", "research_desk", "specimen_tank",
                                    "power_generator", "locker", "cell_bed", "floor_vent", "hazard_marking",
                                    "wall_light"]),
        ("ENVIRONMENT - OPTIONAL", ["tool_station", "control_desk", "cargo_container", "floor_arrow",
                                    "floor_arrow_exit", "wall_pipe", "storage_rack"]),
        ("LASER GUARD + EFFECTS", ["laser_guard", "laser_projectile", "laser_charge", "laser_impact"]),
    ]
    S, W, PAD = 2, 1600, 24
    P = ga.P
    sheet = Image.new("RGBA", (W, 2400), P["void"])
    d = ImageDraw.Draw(sheet)
    d.text((PAD, 22), "HUNTER'S ASSASSIN - ASSET SHEET (all assets shown at 2x, same scale)",
           font=font(30), fill=(230, 234, 242))
    y = 80
    for title, names in groups:
        d.text((PAD, y), title, font=font(22), fill=P["teal"])
        y += 38
        x, rowh = PAD, 0
        for n in names:
            im = sprites[n]
            cw, ch = max(im.width * S, 96) + 24, im.height * S + 24
            cellw = max(cw, 200)
            if x + cellw > W - PAD:
                x, y, rowh = PAD, y + rowh + 70, 0
            # floor-coloured cell so the asset is seen on its real background
            d.rounded_rectangle([x, y, x + cellw, y + ch], radius=8, fill=P["floor"], outline=P["floor_seam"],
                                width=2)
            big = im.resize((im.width * S, im.height * S), Image.LANCZOS)
            sheet.alpha_composite(big, (x + (cellw - big.width) // 2, y + 12))
            d.text((x + cellw / 2, y + ch + 16), f"{n}.png", font=font(18), fill=(220, 226, 238), anchor="mm")
            d.text((x + cellw / 2, y + ch + 38), f"{im.width} x {im.height}", font=font(16),
                   fill=(140, 150, 172), anchor="mm")
            x += cellw + 18
            rowh = max(rowh, ch)
        y += rowh + 90
    return sheet.crop((0, 0, W, y))


# ------------------------------------------------------------------ main
def main():
    os.makedirs(OUT_MAPS, exist_ok=True)
    os.makedirs(OUT_MOCK, exist_ok=True)
    m, objs = load_map(), load_objects()
    rep, errors, fps, solid = validate(m, objs)
    report = "\n".join(["DECORATION VALIDATION REPORT", "=" * 28] + rep +
                       ["", "ERRORS: none" if not errors else "ERRORS:"] + [" - " + e for e in errors])
    print(report)
    with open(os.path.join(OUT_MAPS, "validation_report.txt"), "w") as f:
        f.write(report + "\n")
    if errors:
        sys.exit("placement errors - fix DECOR before rendering")

    with open(os.path.join(OUT_MAPS, "decoration_objects.txt"), "w") as f:
        f.write("# Proposed environmental decoration for Hunter's Assassin (does NOT replace objects.txt)\n")
        f.write("# type tileX tileY rotation\n")
        f.write("# tileX,tileY = zero-based TOP-LEFT tile of the footprint after rotation; rotation clockwise.\n")
        f.write("# Footprints, solidity and wall-mount offsets: see ASSET_SPECIFICATIONS.md\n")
        for d in DECOR:
            f.write(" ".join(str(v) for v in d) + "\n")
    with open(os.path.join(OUT_MAPS, "decoration_footprints.txt"), "w") as f:
        f.write("# Solid props only. type tileX tileY rotation : occupied tiles (x,y)\n")
        for name, tx, ty, rot, tiles in fps:
            f.write(f"{name} {tx} {ty} {rot} : " + " ".join(f"{x},{y}" for x, y in tiles) + "\n")
    with open(os.path.join(OUT_MAPS, "decoration_collision_preview.txt"), "w") as f:
        for y in range(ROWS):
            f.write("".join("P" if (x, y) in solid else m[y][x] for x in range(COLS)) + "\n")

    sprites = load_sprites()
    blocked = blocked_fn(m, objs, solid)

    # ---- decorated facility preview (all approved objects + guards at spawn)
    world, chars = render_world(m, objs, sprites, solid)
    prev = world.copy()
    for t, x, y, r in chars:
        place_object(prev, sprites, t, x, y, r)
    room_labels(prev)
    assert prev.size == (2400, 1680)
    prev.convert("RGB").save(os.path.join(OUT_MOCK, "decorated_facility_preview.png"))
    prev.resize((1200, 840), Image.LANCZOS).convert("RGB").save(
        os.path.join(OUT_MOCK, "decorated_facility_preview_small.png"))

    YEL, RED = (255, 204, 64, 62), (255, 96, 60, 78)
    c = lambda tx, ty: (tx * T + T / 2, ty * T + T / 2)

    # ---- 1. lab: assassin sneaking past the specimen tanks, corridor guard below
    lab_chars = [ch for ch in chars if ch[0] != "assassin"] + [("assassin", 5, 15, 45)]
    win = gameplay_view(world, lab_chars, sprites, blocked, focus=c(8, 18.5),
                        cones=[(*c(8, 23), 0, YEL)], cards=0)
    win.convert("RGB").save(os.path.join(OUT_MOCK, "gameplay_lab.png"))

    # ---- 2. reactor / maintenance: assassin behind a generator, reactor guard looking up
    re_chars = [ch for ch in chars if ch[0] != "assassin"] + [("assassin", 33, 30, 0)]
    win = gameplay_view(world, re_chars, sprites, blocked, focus=c(31, 29.5),
                        cones=[(*c(37, 30), 270, YEL)], cards=1)
    win.convert("RGB").save(os.path.join(OUT_MOCK, "gameplay_reactor.png"))

    # ---- 3. security: the security-room guard shown as a LASER guard firing at the assassin
    #      (mockup only - which guards become laser guards is decided in the game code)
    gx, gy = c(43, 5)
    ax, ay = c(39, 4)
    ang = math.degrees(math.atan2(ay - gy, ax - gx))
    mx, my = rot_vec(18.5, 11.9, ang)                 # blaster muzzle offset (idle pose)
    muzzle = (gx + mx, gy + my)
    tdx, tdy = ax - muzzle[0], ay - muzzle[1]
    sec_chars = [ch for ch in chars if ch[0] != "assassin" and (ch[1], ch[2]) != (43, 5)]
    sec_chars += [("laser_guard", 43, 5, ang), ("assassin", 39, 4, 0)]
    effects = [("laser_charge", muzzle[0], muzzle[1], 0),
               ("laser_projectile", muzzle[0] + tdx * 0.55, muzzle[1] + tdy * 0.55, ang),
               ("laser_impact", 38 * T + 30, 3 * T + 8, 0)]
    win = gameplay_view(world, sec_chars, sprites, blocked, focus=c(41, 4.5),
                        cones=[(gx, gy, ang, RED)], effects=effects, cards=0)
    win.convert("RGB").save(os.path.join(OUT_MOCK, "gameplay_security.png"))

    asset_sheet(sprites).convert("RGB").save(os.path.join(OUT_MOCK, "asset_sheet.png"))
    print("previews written to", os.path.normpath(OUT_MOCK))


if __name__ == "__main__":
    main()
