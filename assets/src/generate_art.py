"""
Hunter's Assassin - art style sample generator.

Run:  python generate_art.py
Needs: Pillow  (pip install pillow)

Everything is drawn with simple shapes at 8x resolution and then scaled down,
which gives smooth (anti-aliased) edges at the exact target sizes.
Edit the PALETTE or the draw_* functions and re-run to regenerate every PNG.

Outputs (relative to this file):
  ../png/base_map_sample.png      576x384  floors + walls only
  ../png/assassin.png             40x40    transparent, faces RIGHT (0 degrees)
  ../png/guard.png                40x40    transparent, faces RIGHT (0 degrees)
  ../png/cargo_crate.png          48x48    transparent
  ../png/security_terminal.png    48x48    transparent
  ../png/security_door.png        48x48    transparent, horizontal (rotate 90 for vertical walls)
  ../png/exit_door.png            48x48    transparent, green variant of the door
  ../png/access_card.png          24x24    transparent
  ../mockups/mockup_1x.png        576x384  map + objects at world scale
  ../mockups/mockup_window.png    1280x720 how it looks in the game window at 1.5x zoom
"""

import math
import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
PNG_DIR = os.path.join(HERE, "..", "png")
MOCK_DIR = os.path.join(HERE, "..", "mockups")

SS = 8          # supersampling factor
TILE = 48

# ------------------------------------------------------------------ palette
PALETTE = {
    "outline":      (12, 15, 22, 255),
    "void":         (11, 14, 20, 255),
    # floors
    "floor":        (42, 50, 68, 255),
    "floor_seam":   (33, 40, 55, 255),
    "floor_rivet":  (56, 66, 88, 255),
    "corridor":     (36, 43, 59, 255),
    # walls
    "wall":         (22, 27, 39, 255),
    "wall_edge":    (78, 92, 120, 255),
    # props / metal
    "steel":        (92, 105, 132, 255),
    "steel_light":  (128, 142, 170, 255),
    "steel_dark":   (58, 67, 88, 255),
    # accents
    "teal":         (46, 230, 176, 255),
    "teal_dark":    (22, 140, 108, 255),
    "teal_glow":    (46, 230, 176, 70),
    "red":          (232, 70, 84, 255),
    "red_dark":     (150, 36, 50, 255),
    "yellow":       (255, 204, 64, 255),
    "yellow_dark":  (176, 130, 20, 255),
    "green":        (76, 222, 118, 255),
    "green_dark":   (30, 130, 66, 255),
    # characters
    "assassin_body": (70, 80, 106, 255),
    "assassin_head": (30, 35, 48, 255),
    "guard_body":   (104, 116, 142, 255),
    "guard_head":   (62, 71, 92, 255),
    "card":         (230, 234, 242, 255),
    "blade":        (210, 218, 230, 255),
    "shadow":       (0, 0, 0, 90),
}
P = PALETTE


# ------------------------------------------------------------------ helpers
class Canvas:
    """Draw in final-pixel coordinates; internally rendered at SS x."""

    def __init__(self, w, h, bg=(0, 0, 0, 0)):
        self.w, self.h = w, h
        self.img = Image.new("RGBA", (w * SS, h * SS), bg)
        self.d = ImageDraw.Draw(self.img)

    @staticmethod
    def s(v):
        return v * SS

    def _box(self, x0, y0, x1, y1):
        return [self.s(x0), self.s(y0), self.s(x1), self.s(y1)]

    def rect(self, x0, y0, x1, y1, fill=None, outline=None, width=0, radius=0):
        w = int(round(self.s(width))) if outline else 0
        if radius:
            self.d.rounded_rectangle(self._box(x0, y0, x1, y1), radius=self.s(radius),
                                     fill=fill, outline=outline, width=w)
        else:
            self.d.rectangle(self._box(x0, y0, x1, y1), fill=fill, outline=outline, width=w)

    def ellipse(self, x0, y0, x1, y1, fill=None, outline=None, width=0):
        w = int(round(self.s(width))) if outline else 0
        self.d.ellipse(self._box(x0, y0, x1, y1), fill=fill, outline=outline, width=w)

    def circle(self, cx, cy, r, **kw):
        self.ellipse(cx - r, cy - r, cx + r, cy + r, **kw)

    def poly(self, pts, fill=None, outline=None, width=0):
        sp = [(self.s(x), self.s(y)) for x, y in pts]
        self.d.polygon(sp, fill=fill)
        if outline and width:
            self.d.line(sp + [sp[0]], fill=outline, width=int(round(self.s(width))), joint="curve")

    def line(self, pts, fill, width):
        self.d.line([(self.s(x), self.s(y)) for x, y in pts], fill=fill,
                    width=int(round(self.s(width))), joint="curve")

    def soft_shadow(self, draw_fn, offset=(1.5, 2.0), blur=1.5):
        """Draw a blurred shadow of a shape (draw_fn receives a Canvas-like drawer)."""
        layer = Canvas(self.w, self.h)
        draw_fn(layer)
        sh = layer.img.filter(ImageFilter.GaussianBlur(self.s(blur)))
        moved = Image.new("RGBA", self.img.size, (0, 0, 0, 0))
        moved.paste(sh, (int(self.s(offset[0])), int(self.s(offset[1]))))
        self.img.alpha_composite(moved)

    def result(self):
        return self.img.resize((self.w, self.h), Image.LANCZOS)


OUT_W = 1.4  # standard outline width in final pixels


# ------------------------------------------------------------------ sprites
def draw_assassin():
    """Visible body ~36x36 inside the 40x40 frame. Faces +X (right, 0 degrees)."""
    c = Canvas(40, 40)
    c.soft_shadow(lambda k: k.ellipse(8, 2, 30, 38, fill=P["shadow"]))
    # faint teal halo so the silhouette separates from the dark floor
    halo = Canvas(40, 40)
    halo.ellipse(7, 1.5, 31, 38.5, fill=P["teal_glow"])
    c.img.alpha_composite(halo.img.filter(ImageFilter.GaussianBlur(SS * 1.2)))
    # scarf tail trailing behind
    c.poly([(12, 17.5), (2, 15), (1.5, 21.5), (12, 23.5)], fill=P["teal"], outline=P["outline"], width=1.2)
    # shoulders (wide across Y because the character faces +X)
    c.ellipse(8, 2, 30, 38, fill=P["assassin_body"], outline=P["outline"], width=1.6)
    # teal shoulder stripes
    c.rect(12, 4.2, 23, 8, fill=P["teal"], outline=P["outline"], width=0.9, radius=1.8)
    c.rect(12, 32, 23, 35.8, fill=P["teal"], outline=P["outline"], width=0.9, radius=1.8)
    # hands
    c.circle(27.5, 7.5, 3.2, fill=P["assassin_body"], outline=P["outline"], width=1.2)
    # knife in the right hand (right hand is +Y when facing +X)
    c.poly([(28, 31), (39.3, 32.6), (28, 34.2)], fill=P["blade"], outline=P["outline"], width=1.0)
    c.circle(27.5, 32.5, 3.2, fill=P["assassin_body"], outline=P["outline"], width=1.2)
    # teal scarf ring
    c.circle(19.5, 20, 10, fill=P["teal"], outline=P["outline"], width=1.6)
    # hood / head
    c.circle(20, 20, 7.4, fill=P["assassin_head"], outline=P["outline"], width=1.2)
    # visor slit facing forward
    c.rect(23.2, 16.4, 28.2, 23.6, fill=P["teal"], outline=P["outline"], width=0.8, radius=1.8)
    return c.result()


def draw_guard():
    """Bulkier guard, visible body ~36x36 inside the 40x40 frame. Faces +X (right, 0 degrees)."""
    c = Canvas(40, 40)
    c.soft_shadow(lambda k: k.ellipse(7, 2, 31, 38, fill=P["shadow"]))
    # flashlight in the right hand (+Y side)
    c.rect(27, 30, 37, 34.4, fill=P["outline"], radius=1.2)
    c.rect(35, 29.2, 39.4, 35.2, fill=P["yellow"], outline=P["outline"], width=1.0, radius=1.2)
    # torso
    c.ellipse(7, 2, 31, 38, fill=P["guard_body"], outline=P["outline"], width=1.6)
    # large red shoulder armour
    for y0 in (2.5, 27.5):
        c.rect(10, y0, 24, y0 + 10, fill=P["red"], outline=P["outline"], width=1.3, radius=4)
        c.line([(12.5, y0 + 2.8), (21.5, y0 + 2.8)], fill=(255, 140, 150, 255), width=1.2)
    # hands
    c.circle(28.5, 8.5, 3.3, fill=P["guard_head"], outline=P["outline"], width=1.2)
    c.circle(28.5, 32, 3.3, fill=P["guard_head"], outline=P["outline"], width=1.2)
    # helmet
    c.circle(19, 20, 9.2, fill=P["guard_head"], outline=P["outline"], width=1.6)
    # red helmet stripe (front to back)
    c.rect(10.5, 18.6, 24, 21.4, fill=P["red_dark"])
    # wide red visor at the front
    c.d.chord(c._box(13.5, 11, 28.2, 29), start=-68, end=68, fill=P["red"])
    c.line([(25.5, 15.5), (26.6, 18)], fill=(255, 170, 178, 255), width=1.1)
    c.d.arc(c._box(9.8, 10.8, 28.2, 29.2), start=0, end=360, fill=P["outline"], width=int(c.s(1.6)))
    return c.result()


def draw_crate():
    c = Canvas(48, 48)
    c.soft_shadow(lambda k: k.rect(5, 5, 43, 43, fill=P["shadow"], radius=4), offset=(2, 3), blur=2)
    c.rect(5, 5, 43, 43, fill=P["steel"], outline=P["outline"], width=OUT_W * 1.2, radius=4)
    c.rect(9, 9, 39, 39, fill=P["steel_dark"], radius=2)
    # cross straps
    c.line([(10, 10), (38, 38)], fill=P["steel"], width=4)
    c.line([(38, 10), (10, 38)], fill=P["steel"], width=4)
    c.rect(9, 9, 39, 39, outline=P["outline"], width=1, radius=2)
    # top highlight
    c.line([(8, 6.8), (40, 6.8)], fill=P["steel_light"], width=1.2)
    # yellow corner tabs
    for x, y in [(5, 5), (37, 5), (5, 37), (37, 37)]:
        c.rect(x, y, x + 6, y + 6, fill=P["yellow"], outline=P["outline"], width=1, radius=1.5)
    return c.result()


def draw_terminal():
    c = Canvas(48, 48)
    c.soft_shadow(lambda k: k.rect(6, 8, 42, 42, fill=P["shadow"], radius=4), offset=(2, 3), blur=2)
    # console body
    c.rect(6, 8, 42, 42, fill=P["steel_dark"], outline=P["outline"], width=OUT_W * 1.2, radius=4)
    c.line([(9, 10), (39, 10)], fill=P["steel"], width=1.2)
    # screen
    c.rect(10, 13, 38, 29, fill=(20, 32, 44, 255), outline=P["outline"], width=1.1, radius=2)
    for i, w in enumerate([18, 12, 15]):
        c.line([(13, 17 + i * 4), (13 + w, 17 + i * 4)], fill=P["yellow"], width=1.4)
    # keypad
    for row in range(2):
        for col in range(4):
            x, y = 12 + col * 6.5, 32 + row * 4.5
            c.rect(x, y, x + 4.5, y + 3, fill=P["steel"], radius=0.8)
    # status light
    c.circle(38, 36, 2.2, fill=P["yellow"], outline=P["outline"], width=0.9)
    return c.result()


def draw_door(accent, accent_dark, lamp):
    """Horizontal sliding door that spans one tile. Rotate 90 for vertical walls."""
    c = Canvas(48, 48)
    # side posts
    for x0 in (0, 40):
        c.rect(x0, 10, x0 + 8, 38, fill=P["steel_dark"], outline=P["outline"], width=OUT_W, radius=2)
    # panels
    c.rect(7, 15, 41, 33, fill=P["steel"], outline=P["outline"], width=OUT_W)
    # hazard stripes in the middle band
    band = Canvas(48, 48)
    band.rect(8, 20, 40, 28, fill=accent)
    for i in range(-2, 10):
        x = 8 + i * 6
        band.poly([(x, 28), (x + 3, 28), (x + 7, 20), (x + 4, 20)], fill=P["outline"])
    mask = Image.new("L", c.img.size, 0)
    ImageDraw.Draw(mask).rectangle(band._box(8, 20, 40, 28), fill=255)
    c.img.paste(band.img, (0, 0), mask)
    c.rect(8, 20, 40, 28, outline=P["outline"], width=1)
    # centre seam
    c.line([(24, 15), (24, 33)], fill=P["outline"], width=1.6)
    # lamp on the posts
    c.circle(4, 14.5, 2, fill=lamp, outline=P["outline"], width=0.8)
    c.circle(44, 14.5, 2, fill=lamp, outline=P["outline"], width=0.8)
    return c.result()


def draw_card():
    """Larger card with a stronger glow so it is easy to spot as a pickup."""
    c = Canvas(24, 24)
    glow = Canvas(24, 24)
    glow.circle(12, 12, 10.5, fill=(255, 204, 64, 170))
    c.img.alpha_composite(glow.img.filter(ImageFilter.GaussianBlur(SS * 2)))
    c.rect(2.5, 5.5, 21.5, 18.5, fill=P["card"], outline=P["outline"], width=1.4, radius=2)
    c.rect(3.3, 6.3, 20.7, 10.2, fill=P["yellow"], radius=1.4)
    c.rect(5.2, 12, 9.6, 16.2, fill=P["yellow_dark"], outline=P["outline"], width=0.7, radius=0.8)
    c.line([(11.5, 13.2), (18.5, 13.2)], fill=P["steel_dark"], width=1.1)
    c.line([(11.5, 15.6), (16.5, 15.6)], fill=P["steel_dark"], width=1.1)
    c.rect(2.5, 5.5, 21.5, 18.5, outline=P["outline"], width=1.4, radius=2)
    return c.result()


# ------------------------------------------------------------------ base map
# W = permanent wall, . = room floor, , = corridor floor.
# Doors/crates/etc. are NOT part of this map: the gaps where doors go are plain floor.
MAP = [
    "WWWWWWWWWWWW",
    "W....W.....W",
    "W....W.....W",
    "W..........W",
    "W....W.....W",
    "WW,WWWWW,WWW",
    "W,,,,,,,,,,,",
    "WWWWWWWWWWWW",
]
COLS, ROWS = len(MAP[0]), len(MAP)


def is_wall(x, y):
    if x < 0 or y < 0 or x >= COLS or y >= ROWS:
        return True
    return MAP[y][x] == "W"


def draw_base_map(m=None):
    """Render a map given as a list of strings (W = wall, . = room floor, , = corridor floor)."""
    m = m or MAP
    cols, rows = len(m[0]), len(m)

    def iw(x, y):
        if x < 0 or y < 0 or x >= cols or y >= rows:
            return True
        return m[y][x] == "W"

    W, H = cols * TILE, rows * TILE
    c = Canvas(W, H, bg=P["void"])

    # floors
    for y in range(rows):
        for x in range(cols):
            ch = m[y][x]
            if ch == "W":
                continue
            x0, y0 = x * TILE, y * TILE
            base = P["corridor"] if ch == "," else P["floor"]
            c.rect(x0, y0, x0 + TILE, y0 + TILE, fill=base)
            # panel seams
            c.rect(x0 + 0.5, y0 + 0.5, x0 + TILE - 0.5, y0 + TILE - 0.5, outline=P["floor_seam"], width=1)
            if ch == ".":
                for rx, ry in [(4, 4), (TILE - 4, 4), (4, TILE - 4), (TILE - 4, TILE - 4)]:
                    c.circle(x0 + rx, y0 + ry, 1, fill=P["floor_rivet"])

    # soft shadow cast by walls onto the floor
    shadow = Canvas(W, H)
    for y in range(rows):
        for x in range(cols):
            if iw(x, y):
                shadow.rect(x * TILE, y * TILE, (x + 1) * TILE, (y + 1) * TILE, fill=(0, 0, 0, 120))
    sh = shadow.img.filter(ImageFilter.GaussianBlur(SS * 4))
    moved = Image.new("RGBA", sh.size, (0, 0, 0, 0))
    moved.paste(sh, (SS * 2, SS * 3))
    c.img.alpha_composite(moved)

    # walls: solid blocks with a lighter rim wherever they touch floor
    for y in range(rows):
        for x in range(cols):
            if not iw(x, y):
                continue
            x0, y0 = x * TILE, y * TILE
            c.rect(x0, y0, x0 + TILE, y0 + TILE, fill=P["wall"])
    for y in range(rows):
        for x in range(cols):
            if not iw(x, y):
                continue
            x0, y0, x1, y1 = x * TILE, y * TILE, (x + 1) * TILE, (y + 1) * TILE
            e, o = P["wall_edge"], P["outline"]
            if not iw(x, y - 1):
                c.line([(x0, y0 + 1), (x1, y0 + 1)], fill=o, width=2.5)
                c.line([(x0, y0 + 3), (x1, y0 + 3)], fill=e, width=2)
            if not iw(x, y + 1):
                c.line([(x0, y1 - 1), (x1, y1 - 1)], fill=o, width=2.5)
                c.line([(x0, y1 - 3), (x1, y1 - 3)], fill=e, width=2)
            if not iw(x - 1, y):
                c.line([(x0 + 1, y0), (x0 + 1, y1)], fill=o, width=2.5)
                c.line([(x0 + 3, y0), (x0 + 3, y1)], fill=e, width=2)
            if not iw(x + 1, y):
                c.line([(x1 - 1, y0), (x1 - 1, y1)], fill=o, width=2.5)
                c.line([(x1 - 3, y0), (x1 - 3, y1)], fill=e, width=2)
    return c.result()


# ------------------------------------------------------------------ mockups
def tile_pos(tx, ty, size):
    """Top-left pixel to centre a sprite of `size` in tile (tx, ty)."""
    return (tx * TILE + (TILE - size[0]) // 2, ty * TILE + (TILE - size[1]) // 2)


def vision_cone(size, origin, angle_deg, fov_deg, radius, color):
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    pts = [origin]
    for i in range(25):
        a = math.radians(angle_deg - fov_deg / 2 + fov_deg * i / 24)
        pts.append((origin[0] + radius * math.cos(a), origin[1] + radius * math.sin(a)))
    d.polygon(pts, fill=color)
    # keep the cone inside the guard's corridor row (cheap stand-in for the in-game raycast)
    mask = Image.new("L", size, 0)
    md = ImageDraw.Draw(mask)
    row = int(origin[1] // TILE)
    for x in range(COLS):
        if not is_wall(x, row):
            md.rectangle([x * TILE, row * TILE, (x + 1) * TILE - 1, (row + 1) * TILE - 1], fill=255)
    out = Image.new("RGBA", size, (0, 0, 0, 0))
    out.paste(layer, (0, 0), mask)
    return out


def build_mockup(sprites, base):
    img = base.copy()
    # guard in the bottom corridor facing left, with its vision cone
    guard_c = (7 * TILE + TILE // 2, 6 * TILE + TILE // 2)
    img.alpha_composite(vision_cone(img.size, guard_c, 180, 70, 4 * TILE, (255, 204, 64, 70)))

    def put(sprite, tx, ty, rotate=0, offset=(0, 0)):
        s = sprite.rotate(rotate, expand=True, resample=Image.BICUBIC) if rotate else sprite
        x, y = tile_pos(tx, ty, s.size)
        img.alpha_composite(s, (x + offset[0], y + offset[1]))

    # doors sit in the wall gaps (vertical wall -> rotate 90)
    put(sprites["security_door"], 5, 3, rotate=90)
    put(sprites["exit_door"], 11, 6, rotate=90)
    # props
    put(sprites["cargo_crate"], 1, 1)
    put(sprites["cargo_crate"], 2, 1)
    put(sprites["cargo_crate"], 1, 2)
    put(sprites["cargo_crate"], 10, 4)
    put(sprites["security_terminal"], 8, 1)
    put(sprites["access_card"], 9, 3)
    # characters
    put(sprites["assassin"], 3, 3, rotate=0)
    put(sprites["guard"], 7, 6, rotate=180)
    return img


def build_window(mock):
    W, H, ZOOM = 1280, 720, 1.5
    win = Image.new("RGBA", (W, H), P["void"])
    big = mock.resize((int(mock.width * ZOOM), int(mock.height * ZOOM)), Image.LANCZOS)
    ox, oy = (W - big.width) // 2, (H - big.height) // 2 + 30
    win.alpha_composite(big, (ox, oy))
    d = ImageDraw.Draw(win)
    try:
        font = ImageFont.truetype("bahnschrift.ttf", 30)
        small = ImageFont.truetype("bahnschrift.ttf", 18)
    except OSError:
        font = ImageFont.load_default(30)
        small = ImageFont.load_default(18)
    # HUD: health pips (top-left)
    for i in range(3):
        x = 32 + i * 30
        d.rounded_rectangle([x, 28, x + 22, 50], radius=5,
                            fill=P["teal"] if i < 2 else P["steel_dark"], outline=P["outline"], width=2)
    d.text((32, 58), "HEALTH", font=small, fill=(150, 160, 180))
    # timer (top-centre)
    d.text((W // 2, 40), "04:37", font=font, fill=(230, 234, 242), anchor="mm")
    d.text((W // 2, 68), "LOCKDOWN", font=small, fill=P["yellow"], anchor="mm")
    # objectives (top-right)
    card = Image.open(os.path.join(PNG_DIR, "access_card.png")).resize((36, 36), Image.LANCZOS)
    win.alpha_composite(card, (W - 150, 22))
    d.text((W - 108, 40), "0 / 2", font=font, fill=(230, 234, 242), anchor="lm")
    d.text((W - 150, 66), "ACCESS CARDS", font=small, fill=(150, 160, 180))
    return win


def main():
    os.makedirs(PNG_DIR, exist_ok=True)
    os.makedirs(MOCK_DIR, exist_ok=True)
    sprites = {
        "assassin": draw_assassin(),
        "guard": draw_guard(),
        "cargo_crate": draw_crate(),
        "security_terminal": draw_terminal(),
        "security_door": draw_door(P["yellow"], P["yellow_dark"], P["red"]),
        "exit_door": draw_door(P["green"], P["green_dark"], P["green"]),
        "access_card": draw_card(),
    }
    for name, im in sprites.items():
        im.save(os.path.join(PNG_DIR, f"{name}.png"))
    base = draw_base_map()
    base.convert("RGB").save(os.path.join(PNG_DIR, "base_map_sample.png"))
    mock = build_mockup(sprites, base)
    mock.convert("RGB").save(os.path.join(MOCK_DIR, "mockup_1x.png"))
    build_window(mock).convert("RGB").save(os.path.join(MOCK_DIR, "mockup_window.png"))
    for f in sorted(os.listdir(PNG_DIR)) + sorted(os.listdir(MOCK_DIR)):
        path = os.path.join(PNG_DIR if f in os.listdir(PNG_DIR) else MOCK_DIR, f)
        print(f, Image.open(path).size, Image.open(path).mode)


if __name__ == "__main__":
    main()
