"""
Hunter's Assassin - character animation frames + attack effects.

Run:  python generate_animations.py      (needs Pillow; run generate_environment.py first)

The three characters are re-drawn here as *posable* versions of the approved
sprites: with the default (idle) pose the output matches assassin.png,
guard.png and laser_guard.png, and every animation frame only moves limbs,
feet, the weapon and a small shoulder sway. Body, head, colours and outlines
never change, and the rotation pivot is the frame centre (20, 20) in every frame.

Each frame is drawn at 8x on a 48x48 padded canvas (4 px margin on every side),
checked for anything spilling outside the 40x40 frame (clipping), then cropped
and scaled down with LANCZOS, exactly like the approved assets.

Outputs (relative to environment_expansion/):
  png/animations/assassin/*.png      png/animations/guard/*.png
  png/animations/laser_guard/*.png   png/effects/knife_slash.png, muzzle_flash.png
  mockups/animation_sheet.png        mockups/animation_previews/*.gif
  maps/animation_validation.txt
"""

import math
import os
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "src"))
import generate_art as ga  # noqa: E402

P = ga.P
SS = 8
PAD = 4                   # padding (final px) used only to detect clipping
FW = 40                   # final frame size
OUT = os.path.normpath(os.path.join(HERE, ".."))
ANIM_DIR = os.path.join(OUT, "png", "animations")
FX_DIR = os.path.join(OUT, "png", "effects")
MOCK_DIR = os.path.join(OUT, "mockups")
PREV_DIR = os.path.join(MOCK_DIR, "animation_previews")

ORANGE, ORANGE_DARK, ORANGE_HI = (255, 156, 44, 255), (168, 88, 16, 255), (255, 214, 150, 255)
DIM_VISOR = (96, 104, 124, 255)


# ------------------------------------------------------------------ canvas with padding
class PC:
    """Like generate_art.Canvas but with a PAD margin so overflow can be measured."""

    def __init__(self, w=FW, h=FW):
        self.w, self.h = w, h
        self.img = Image.new("RGBA", ((w + 2 * PAD) * SS, (h + 2 * PAD) * SS), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.img)

    @staticmethod
    def p(v):
        return (v + PAD) * SS

    @staticmethod
    def s(v):
        return v * SS

    def box(self, x0, y0, x1, y1):
        return [self.p(x0), self.p(y0), self.p(x1), self.p(y1)]

    def rect(self, x0, y0, x1, y1, fill=None, outline=None, width=0, radius=0):
        w = int(round(self.s(width))) if outline else 0
        if radius:
            self.d.rounded_rectangle(self.box(x0, y0, x1, y1), radius=self.s(radius), fill=fill, outline=outline, width=w)
        else:
            self.d.rectangle(self.box(x0, y0, x1, y1), fill=fill, outline=outline, width=w)

    def ellipse(self, x0, y0, x1, y1, fill=None, outline=None, width=0):
        w = int(round(self.s(width))) if outline else 0
        self.d.ellipse(self.box(x0, y0, x1, y1), fill=fill, outline=outline, width=w)

    def circle(self, cx, cy, r, **kw):
        self.ellipse(cx - r, cy - r, cx + r, cy + r, **kw)

    def poly(self, pts, fill=None, outline=None, width=0):
        sp = [(self.p(x), self.p(y)) for x, y in pts]
        self.d.polygon(sp, fill=fill)
        if outline and width:
            self.d.line(sp + [sp[0]], fill=outline, width=int(round(self.s(width))), joint="curve")

    def line(self, pts, fill, width):
        self.d.line([(self.p(x), self.p(y)) for x, y in pts], fill=fill, width=int(round(self.s(width))), joint="curve")

    def blur_from(self, fn, blur, offset=(0, 0)):
        layer = PC(self.w, self.h)
        fn(layer)
        sh = layer.img.filter(ImageFilter.GaussianBlur(self.s(blur)))
        if offset != (0, 0):
            sh = sh.rotate(0, translate=(self.s(offset[0]), self.s(offset[1])))
        self.img.alpha_composite(sh)

    def add(self, layer, angle=0.0, pivot=(20, 20), move=(0, 0)):
        """Composite another PC, rotated clockwise by `angle` about `pivot`, then moved."""
        im = layer.img
        if angle or move != (0, 0):
            im = im.rotate(-angle, resample=Image.BICUBIC, center=(self.p(pivot[0]), self.p(pivot[1])),
                           translate=(self.s(move[0]), self.s(move[1])))
        self.img.alpha_composite(im)

    def overflow(self):
        """Max alpha found outside the 40x40 frame (0 = nothing clipped)."""
        a = self.img.getchannel("A")
        inner = Image.new("L", a.size, 0)
        ImageDraw.Draw(inner).rectangle([PAD * SS, PAD * SS, (PAD + self.w) * SS - 1, (PAD + self.h) * SS - 1], fill=255)
        outside = Image.composite(Image.new("L", a.size, 0), a, inner)
        return outside.getextrema()[1]

    def result(self):
        crop = self.img.crop((PAD * SS, PAD * SS, (PAD + self.w) * SS, (PAD + self.h) * SS))
        return crop.resize((self.w, self.h), Image.LANCZOS)


def rot(dx, dy, ang):
    a = math.radians(ang)
    return dx * math.cos(a) - dy * math.sin(a), dx * math.sin(a) + dy * math.cos(a)


def held_item(draw_fn, grip, angle):
    """Draw an item with its grip at (20,20) pointing +X, then rotate about the grip and move it to `grip`."""
    lay = PC()
    draw_fn(lay)
    out = PC()
    out.add(lay, angle, pivot=(20, 20), move=(grip[0] - 20, grip[1] - 20))
    return out


def foot(c, x, y, col, toe=None):
    c.rect(x - 4, y - 2.9, x + 4, y + 2.9, fill=col, outline=P["outline"], width=1.2, radius=2.4)
    if toe:                     # small accent so the step reads on the dark floor
        c.rect(x + 0.8, y - 1.5, x + 2.6, y + 1.5, fill=toe, radius=0.8)


# ------------------------------------------------------------------ weapons (grip at 20,20, pointing +X)
def knife(length=11.8):
    def f(c):
        c.poly([(20.5, 18.5), (20 + length, 20.1), (20.5, 21.7)], fill=P["blade"], outline=P["outline"], width=1.0)
    return f


def flashlight(c):
    c.rect(18.5, 18, 28.5, 22.4, fill=P["outline"], radius=1.2)
    c.rect(26.5, 17.2, 30.9, 23.2, fill=P["yellow"], outline=P["outline"], width=1.0, radius=1.2)


def blaster(emitter=1.0):
    """emitter: 1 = normal glow, >1 = charging (brighter, bigger), <1 = cooling down."""
    def f(c):
        c.rect(18, 15.9, 28.5, 24.1, fill=P["steel_dark"], outline=P["outline"], width=1.2, radius=1.6)
        c.line([(20, 18.1), (26.5, 18.1)], fill=ORANGE, width=1.3)
        c.rect(27, 17.5, 31.9, 22.5, fill=P["steel_light"], outline=P["outline"], width=1.0, radius=1)
        c.blur_from(lambda k: k.circle(32.5, 20, 2.2 * min(emitter, 1.2),
                                       fill=(255, 120, 60, int(min(255, 210 * emitter)))), 0.8)
        col = ORANGE if emitter <= 1.0 else (255, 196, 110, 255)
        if emitter < 1.0:
            col = (214, 120, 40, 255)
        c.rect(30.9, 17.9, 34, 22.1, fill=col, outline=P["outline"], width=0.9, radius=1)
    return f


# ------------------------------------------------------------------ assassin
ASSASSIN_IDLE = dict(twist=0.0, lh=(27.5, 7.5), rh=(27.5, 32.5), kang=0.0, klen=11.8, feet=(), tail=0.0)


def draw_assassin(**pose):
    q = {**ASSASSIN_IDLE, **pose}
    c = PC()
    c.blur_from(lambda k: k.ellipse(8, 2, 30, 38, fill=P["shadow"]), 1.5, offset=(1.5, 2.0))
    for fx, fy in q["feet"]:
        foot(c, fx, fy, P["assassin_body"], P["teal"])
    body = PC()
    body.blur_from(lambda k: k.ellipse(7, 1.5, 31, 38.5, fill=P["teal_glow"]), 1.2)
    t = q["tail"]
    body.poly([(12, 17.5), (2, 15 + t), (1.5, 21.5 + t), (12, 23.5)], fill=P["teal"], outline=P["outline"], width=1.2)
    body.ellipse(8, 2, 30, 38, fill=P["assassin_body"], outline=P["outline"], width=1.6)
    body.rect(12, 4.2, 23, 8, fill=P["teal"], outline=P["outline"], width=0.9, radius=1.8)
    body.rect(12, 32, 23, 35.8, fill=P["teal"], outline=P["outline"], width=0.9, radius=1.8)
    c.add(body, q["twist"], pivot=(19, 20))
    c.circle(*q["lh"], 3.2, fill=P["assassin_body"], outline=P["outline"], width=1.2)
    c.add(held_item(knife(q["klen"]), (q["rh"][0] + 0.5 - 0.5, q["rh"][1]), q["kang"]))
    c.circle(*q["rh"], 3.2, fill=P["assassin_body"], outline=P["outline"], width=1.2)
    c.circle(19.5, 20, 10, fill=P["teal"], outline=P["outline"], width=1.6)
    c.circle(20, 20, 7.4, fill=P["assassin_head"], outline=P["outline"], width=1.2)
    c.rect(23.2, 16.4, 28.2, 23.6, fill=q.get("visor", P["teal"]), outline=P["outline"], width=0.8, radius=1.8)
    return c


# ------------------------------------------------------------------ guards (melee + laser)
GUARD_IDLE = dict(twist=0.0, lh=(28.5, 8.5), rh=(28.5, 32.0), wang=0.0, feet=(), visor=None, emitter=1.0,
                  flash=0.0, item_on_top=False)
LASER_IDLE = dict(GUARD_IDLE, rh=(24.5, 31.9))


def draw_guard(laser=False, **pose):
    q = {**(LASER_IDLE if laser else GUARD_IDLE), **pose}
    acc, acc_hi = (ORANGE, ORANGE_HI) if laser else (P["red"], (255, 140, 150, 255))
    stripe = ORANGE_DARK if laser else P["red_dark"]
    visor = q["visor"] or acc
    c = PC()
    c.blur_from(lambda k: k.ellipse(7, 2, 31, 38, fill=P["shadow"]), 1.5, offset=(1.5, 2.0))
    for fx, fy in q["feet"]:
        foot(c, fx, fy, P["guard_body"], ORANGE if laser else P["red"])
    if not laser and not q["item_on_top"]:
        c.add(held_item(flashlight, q["rh"], q["wang"]))
    body = PC()
    body.ellipse(7, 2, 31, 38, fill=P["guard_body"], outline=P["outline"], width=1.6)
    for y0 in (2.5, 27.5):
        body.rect(10, y0, 24, y0 + 10, fill=acc, outline=P["outline"], width=1.3, radius=4)
        body.line([(12.5, y0 + 2.8), (21.5, y0 + 2.8)], fill=acc_hi, width=1.2)
    c.add(body, q["twist"], pivot=(19, 20))
    if laser:
        c.add(held_item(blaster(q["emitter"]), q["rh"], q["wang"]))
    elif q["item_on_top"]:
        c.add(held_item(flashlight, q["rh"], q["wang"]))
    c.circle(*q["lh"], 3.3, fill=P["guard_head"], outline=P["outline"], width=1.2)
    c.circle(*q["rh"], 3.3, fill=P["guard_head"], outline=P["outline"], width=1.2)
    c.circle(19, 20, 9.2, fill=P["guard_head"], outline=P["outline"], width=1.6)
    c.rect(10.5, 18.6, 24, 21.4, fill=stripe)
    c.d.chord(c.box(13.5, 11, 28.2, 29), start=-68, end=68, fill=visor)
    c.line([(25.5, 15.5), (26.6, 18)], fill=acc_hi if visor == acc else (150, 158, 178, 255), width=1.1)
    c.d.arc(c.box(9.8, 10.8, 28.2, 29.2), start=0, end=360, fill=P["outline"], width=int(c.s(1.6)))
    if q["flash"]:      # muzzle flash INSIDE the frame (small); the big one is effects/muzzle_flash.png
        mx, my = rot(14.0, 0, q["wang"])
        cx, cy = q["rh"][0] + mx, q["rh"][1] + my
        c.blur_from(lambda k: k.circle(cx, cy, 2.6, fill=(255, 200, 120, 230)), 0.9)
        c.circle(cx, cy, 1.9 * q["flash"], fill=(255, 250, 230, 255))
    return c


# ------------------------------------------------------------------ death: lying pose (fell forward, face down)
def draw_lying(kind):
    c = PC()
    if kind == "assassin":
        body_col, head_col, acc = P["assassin_body"], P["assassin_head"], P["teal"]
    else:
        body_col, head_col = P["guard_body"], P["guard_head"]
        acc = ORANGE if kind == "laser_guard" else P["red"]
    c.blur_from(lambda k: k.ellipse(3, 6, 37, 34, fill=P["shadow"]), 1.5, offset=(1.5, 2.0))
    # legs + feet trailing behind (toward -X)
    for y in (14.5, 25.5):
        c.rect(5, y - 3, 14, y + 3, fill=body_col, outline=P["outline"], width=1.2, radius=2.5)
        foot(c, 4.8, y, head_col)
    # dropped weapon on the floor beside the right hand
    if kind == "assassin":
        c.add(held_item(knife(9), (28, 36.2), -8))
    elif kind == "guard":
        c.add(held_item(flashlight, (22, 36.6), -5))
    else:
        c.add(held_item(blaster(0.6), (19, 35.2), -4))
    # arms spread toward the front corners
    for hy, sy in ((5.5, 12), (34.5, 28)):
        c.line([(23, sy), (30, hy)], fill=P["outline"], width=5.2)
        c.line([(23, sy), (30, hy)], fill=body_col, width=3)
        c.circle(30.5, hy, 3.2, fill=head_col, outline=P["outline"], width=1.2)
    # torso seen from the back
    c.ellipse(9, 8.5, 31, 31.5, fill=body_col, outline=P["outline"], width=1.6)
    if kind == "assassin":
        c.rect(19, 9.6, 27, 13, fill=acc, outline=P["outline"], width=0.9, radius=1.6)
        c.rect(19, 27, 27, 30.4, fill=acc, outline=P["outline"], width=0.9, radius=1.6)
        c.circle(30, 20, 8.2, fill=acc, outline=P["outline"], width=1.4)                 # scarf ring
        c.circle(31, 20, 6.2, fill=head_col, outline=P["outline"], width=1.2)            # hood, face down
    else:
        for y0 in (8.0, 24.0):
            c.rect(17, y0, 28, y0 + 8, fill=acc, outline=P["outline"], width=1.2, radius=3.2)
        c.circle(31, 20, 7.4, fill=head_col, outline=P["outline"], width=1.4)            # helmet, face down
        c.rect(24.5, 18.9, 38, 21.1, fill=ORANGE_DARK if kind == "laser_guard" else P["red_dark"])
        c.circle(31, 20, 7.4, outline=P["outline"], width=1.4)
    return c


def collapse(kind):
    """Frame 1 of death: limbs go slack, head drops forward, body sags (scaled ~0.93)."""
    if kind == "assassin":
        c = draw_assassin(twist=8, lh=(30, 5.5), rh=(30.5, 34), kang=24, klen=10, tail=3,
                          feet=((10, 13), (10, 27)), visor=(30, 110, 92, 255))
    else:
        laser = kind == "laser_guard"
        c = draw_guard(laser=laser, twist=8, lh=(30.5, 6), rh=(24.5 if laser else 27.5, 33.2), wang=12,
                       feet=((9.5, 13), (9.5, 27)), visor=DIM_VISOR, emitter=0.6)
    # sag: scale about the pivot, nudged forward
    big = c.img
    w, h = big.size
    k = 0.93
    small = big.resize((int(w * k), int(h * k)), Image.LANCZOS)
    out = PC()
    cx, cy = PC.p(20), PC.p(20)
    out.img.alpha_composite(small, (int(cx - cx * k + PC.s(1)), int(cy - cy * k)))
    return out


# ------------------------------------------------------------------ effects
def knife_slash():
    """48x48 crescent, sweeping in front of the character (see spec for alignment)."""
    c = PC(48, 48)
    layer = PC(48, 48)
    layer.ellipse(-8, 2, 36, 46, fill=(235, 255, 248, 235))
    layer.d.ellipse(layer.box(-14, 5, 31, 43), fill=(0, 0, 0, 0))
    # keep only the front part of the crescent and taper both ends
    mask = Image.new("L", layer.img.size, 0)
    md = ImageDraw.Draw(mask)
    md.pieslice(layer.box(-8, 2, 36, 46), start=-62, end=62, fill=255)
    m2 = mask.filter(ImageFilter.GaussianBlur(PC.s(2.2)))
    crescent = Image.new("RGBA", layer.img.size, (0, 0, 0, 0))
    crescent.paste(layer.img, (0, 0), Image.composite(m2, Image.new("L", m2.size, 0), mask))
    glow = crescent.filter(ImageFilter.GaussianBlur(PC.s(1.4)))
    tint = Image.new("RGBA", glow.size, (46, 230, 176, 0))
    tint.putalpha(glow.getchannel("A").point(lambda v: int(v * 0.55)))
    c.img.alpha_composite(tint)
    c.img.alpha_composite(crescent)
    return c


def muzzle_flash():
    """24x24, points right; its centre goes on the muzzle (see spec)."""
    c = PC(24, 24)
    c.blur_from(lambda k: k.ellipse(4.5, 6.5, 20.5, 17.5, fill=(255, 130, 60, 190)), 1.3)
    pts = []
    for i in range(12):
        a = math.radians(i * 30)
        r = (8.6 if i == 0 else 6.2) if i % 2 == 0 else 3.2
        rx = r * (1.2 if math.cos(a) > 0.3 else 0.85)
        pts.append((12 + math.cos(a) * rx, 12 + math.sin(a) * r))
    c.poly(pts, fill=(255, 176, 70, 255))
    c.circle(12.5, 12, 3.6, fill=(255, 240, 200, 255))
    c.circle(12.8, 12, 1.8, fill=(255, 255, 255, 255))
    return c


# ------------------------------------------------------------------ frame definitions
# feet: left foot is on the -Y side (top of the sprite) because the character faces +X.
STEP_L = ((32.4, 13.8), (6.4, 26.2))     # left foot forward, right foot back
STEP_N = ((24, 13.8), (14, 26.2))        # passing pose (feet under the body)
STEP_R = ((32.4, 26.2), (6.4, 13.8))     # right foot forward, left foot back


SHOOT = {   # laser guard shooting poses (also used to place the effects in the previews / spec)
    "shoot_1": dict(rh=(23.8, 31.0), wang=-3, emitter=1.6, twist=-2, lh=(29.2, 9.0)),               # aim + charge
    "shoot_2": dict(rh=(22.4, 31.6), wang=-3, emitter=1.6, flash=1.0, twist=2, lh=(27.6, 8.6)),      # fire, recoil
    "shoot_3": dict(rh=(23.4, 31.8), wang=-1, emitter=0.7, twist=1, lh=(28.2, 8.5)),                 # recover
}


def muzzle(rh, wang):
    """Front of the blaster emitter, relative to the sprite centre (20,20), facing +X."""
    mx, my = rot(14.0, 0, wang)
    return rh[0] + mx - 20, rh[1] + my - 20


def frames():
    F = {}
    # ---------------- assassin
    a = "assassin"
    F[(a, "idle")] = draw_assassin()
    F[(a, "walk_1")] = draw_assassin(feet=STEP_L, twist=-4, rh=(27.6, 32.2), lh=(25.2, 8.0), tail=1.5)
    F[(a, "walk_2")] = draw_assassin(feet=STEP_N, tail=0.0)
    F[(a, "walk_3")] = draw_assassin(feet=STEP_R, twist=4, rh=(25.6, 32.2), lh=(29, 7.8), tail=-1.5)
    F[(a, "attack_1")] = draw_assassin(twist=7, rh=(23.2, 33.6), kang=22, lh=(29.2, 9.2), feet=((29.5, 13.8),), tail=2)
    F[(a, "attack_2")] = draw_assassin(twist=-9, rh=(29.4, 27.6), kang=-12, klen=10.2, lh=(24.6, 7.2),
                                       feet=((32.4, 13.8), (6.8, 26.2)), tail=-2)
    F[(a, "attack_3")] = draw_assassin(twist=-3, rh=(27, 31), kang=6, lh=(26.6, 7.6), tail=-0.5)
    F[(a, "death_1")] = collapse(a)
    F[(a, "death_2")] = draw_lying(a)
    # ---------------- melee guard
    for kind, laser in (("guard", False), ("laser_guard", True)):
        g = lambda **k: draw_guard(laser=laser, **k)  # noqa: E731
        rx, ry = (LASER_IDLE if laser else GUARD_IDLE)["rh"]
        F[(kind, "idle")] = g()
        F[(kind, "walk_1")] = g(feet=STEP_L, twist=-4, rh=(rx + 0.3, ry), lh=(26.6, 9.0))
        F[(kind, "walk_2")] = g(feet=STEP_N)
        F[(kind, "walk_3")] = g(feet=STEP_R, twist=4, rh=(rx - 1.6, ry), lh=(29.6, 8.2))
        F[(kind, "death_1")] = collapse(kind)
        F[(kind, "death_2")] = draw_lying(kind)
    k = "guard"   # baton-style swing with the flashlight
    F[(k, "attack_1")] = draw_guard(twist=8, rh=(22, 33.8), wang=8, lh=(29.6, 9.6), item_on_top=True, feet=((29.5, 13.8),))
    F[(k, "attack_2")] = draw_guard(twist=-10, rh=(29.2, 26.0), wang=-45, lh=(25.2, 7.4), item_on_top=True,
                                    feet=((32.4, 13.8), (6.8, 26.2)))
    F[(k, "attack_3")] = draw_guard(twist=-3, rh=(28.6, 30.4), wang=-8, lh=(27.2, 8.0))
    k = "laser_guard"
    for n, pose in SHOOT.items():
        F[(k, n)] = draw_guard(laser=True, **pose)
    return F


ANIMS = {   # name: (frames in play order, ms per frame list, loop?)
    "assassin_walk":      (["walk_1", "walk_2", "walk_3", "walk_2"], [110] * 4, True),
    "assassin_attack":    (["idle", "attack_1", "attack_2", "attack_3", "idle"], [300, 90, 90, 90, 300], False),
    "assassin_death":     (["idle", "death_1", "death_2"], [300, 180, 900], False),
    "guard_walk":         (["walk_1", "walk_2", "walk_3", "walk_2"], [110] * 4, True),
    "guard_attack":       (["idle", "attack_1", "attack_2", "attack_3", "idle"], [300, 110, 110, 110, 300], False),
    "guard_death":        (["idle", "death_1", "death_2"], [300, 180, 900], False),
    "laser_guard_walk":   (["walk_1", "walk_2", "walk_3", "walk_2"], [110] * 4, True),
    "laser_guard_shoot":  (["idle", "shoot_1", "shoot_2", "shoot_3", "idle"], [300, 600, 80, 150, 300], False),
    "laser_guard_death":  (["idle", "death_1", "death_2"], [300, 180, 900], False),
}


# ------------------------------------------------------------------ previews
def font(sz):
    try:
        return ImageFont.truetype("bahnschrift.ttf", sz)
    except OSError:
        return ImageFont.load_default(sz)


def floor_bg(w, h):
    bg = Image.new("RGBA", (w, h), P["floor"])
    d = ImageDraw.Draw(bg)
    for x in range(0, w, 72):
        d.line([(x, 0), (x, h)], fill=P["floor_seam"], width=2)
    for y in range(0, h, 72):
        d.line([(0, y), (w, y)], fill=P["floor_seam"], width=2)
    return bg


def gif(name, imgs, durations, fx=None):
    """Native size (left) + 1.5x gameplay scale (middle) + 4x zoom (right) on a floor background."""
    W, H = 400, 200
    out = []
    for i, im in enumerate(imgs):
        fr = floor_bg(W, H)
        fr.alpha_composite(im, (20, 80))
        fr.alpha_composite(im.resize((60, 60), Image.LANCZOS), (75, 70))
        big = im.resize((160, 160), Image.LANCZOS)
        fr.alpha_composite(big, (170, 20))
        if fx and fx[i]:
            for spr, off in fx[i]:
                for scale, ox, oy in ((1, 20, 80), (1.5, 75, 70), (4, 170, 20)):
                    s = spr.resize((int(spr.width * scale), int(spr.height * scale)), Image.LANCZOS)
                    fr.alpha_composite(s, (int(ox + (20 + off[0]) * scale - s.width / 2),
                                           int(oy + (20 + off[1]) * scale - s.height / 2)))
        d = ImageDraw.Draw(fr)
        d.text((22, 180), "1x        1.5x (game)          4x", font=font(13), fill=(170, 180, 200))
        out.append(fr.convert("RGB").convert("P", palette=Image.ADAPTIVE, colors=255))
    out[0].save(os.path.join(PREV_DIR, f"{name}.gif"), save_all=True, append_images=out[1:],
                duration=durations, loop=0, disposal=2)


def animation_sheet(F):
    rows = [("assassin", ["idle", "walk_1", "walk_2", "walk_3", "attack_1", "attack_2", "attack_3", "death_1", "death_2"]),
            ("guard", ["idle", "walk_1", "walk_2", "walk_3", "attack_1", "attack_2", "attack_3", "death_1", "death_2"]),
            ("laser_guard", ["idle", "walk_1", "walk_2", "walk_3", "shoot_1", "shoot_2", "shoot_3", "death_1", "death_2"])]
    S, cell = 3, 150
    W = 40 + cell * 9
    H = 90 + len(rows) * 250 + 230
    sheet = Image.new("RGBA", (W, H), P["void"])
    d = ImageDraw.Draw(sheet)
    d.text((24, 22), "HUNTER'S ASSASSIN - CHARACTER ANIMATION FRAMES (3x zoom, native 40x40 below; idle = approved sprite)",
           font=font(22), fill=(230, 234, 242))
    y = 80
    for kind, names in rows:
        d.text((24, y), kind.upper().replace("_", " "), font=font(20), fill=P["teal"])
        y += 32
        for i, n in enumerate(names):
            x = 24 + i * cell
            d.rounded_rectangle([x, y, x + 132, y + 132], radius=8, fill=P["floor"], outline=P["floor_seam"], width=2)
            sheet.alpha_composite(F[(kind, n)].resize((120, 120), Image.LANCZOS), (x + 6, y + 6))
            d.rounded_rectangle([x + 46, y + 140, x + 86, y + 180], radius=4, fill=P["floor"])
            sheet.alpha_composite(F[(kind, n)], (x + 46, y + 140))
            fname = f"{kind}.png" if n == "idle" else f"{kind}_{n}.png"
            if n == "idle" and kind != "laser_guard":
                fname = f"{kind}.png (approved)"
            d.text((x + 66, y + 196), fname, font=font(12), fill=(200, 208, 224), anchor="mm")
        y += 218
    d.text((24, y), "EFFECTS (3x)", font=font(20), fill=P["teal"])
    y += 32
    x = 24
    for n in ("knife_slash", "muzzle_flash", "laser_charge", "laser_impact", "laser_projectile"):
        im = Image.open(os.path.join(FX_DIR, f"{n}.png"))
        w, h = im.width * S, im.height * S
        d.rounded_rectangle([x, y, x + max(w, 100) + 12, y + 156], radius=8, fill=P["floor"], outline=P["floor_seam"], width=2)
        sheet.alpha_composite(im.resize((w, h), Image.LANCZOS), (x + 6 + (max(w, 100) - w) // 2, y + 6 + (144 - h) // 2))
        d.text((x + (max(w, 100) + 12) / 2, y + 172), f"{n}.png  {im.width}x{im.height}", font=font(12),
               fill=(200, 208, 224), anchor="mm")
        x += max(w, 100) + 40
    return sheet.crop((0, 0, W, y + 190))


# ------------------------------------------------------------------ validation helpers
def centroid(im, thresh=128):
    a = im.getchannel("A")
    px = a.load()
    sx = sy = n = 0
    for y in range(im.height):
        for x in range(im.width):
            if px[x, y] >= thresh:
                sx += x
                sy += y
                n += 1
    return sx / n, sy / n


def head_center(im):
    """Centroid of the head/helmet disc region (rows 12-28, cols 10-28) - should not move while walking."""
    return centroid(im.crop((10, 12, 28, 28)))


def main():
    for sub in ("assassin", "guard", "laser_guard"):
        os.makedirs(os.path.join(ANIM_DIR, sub), exist_ok=True)
    os.makedirs(FX_DIR, exist_ok=True)
    os.makedirs(PREV_DIR, exist_ok=True)

    report = ["ANIMATION VALIDATION", "=" * 20]
    F_canvas = frames()
    F = {k: v.result() for k, v in F_canvas.items()}

    # idle must match the approved sprites
    approved = {"assassin": os.path.join(ROOT, "png", "assassin.png"),
                "guard": os.path.join(ROOT, "png", "guard.png"),
                "laser_guard": os.path.join(OUT, "png", "laser_guard.png")}
    for kind, path in approved.items():
        ref = Image.open(path).convert("RGBA")
        a, b = ref.tobytes(), F[(kind, "idle")].tobytes()
        mad = sum(abs(x - y) for x, y in zip(a, b)) / len(a)
        report.append(f"idle pose vs {os.path.basename(path)}: mean abs difference {mad:.2f} / 255")

    errors = []
    for (kind, name), cv in F_canvas.items():
        if name == "idle":
            continue
        im = F[(kind, name)]
        ov = cv.overflow()
        if im.size != (40, 40) or im.mode != "RGBA":
            errors.append(f"{kind}_{name}: wrong size/mode {im.size} {im.mode}")
        if ov > 60:     # soft shadow / glow tails below this are invisible
            errors.append(f"{kind}_{name}: clipped at the frame edge (alpha {ov} outside 40x40)")
        corners = [im.getpixel(p)[3] for p in ((0, 0), (39, 0), (0, 39), (39, 39))]
        if max(corners) > 12:     # faint shadow blur allowed, nothing visible
            errors.append(f"{kind}_{name}: corner pixels are not transparent")
        im.save(os.path.join(ANIM_DIR, kind, f"{kind}_{name}.png"))

    report.append("")
    report.append("frame                    overflow  centroid-shift  head-shift   (px, vs idle)")
    for kind in ("assassin", "guard", "laser_guard"):
        c0, h0 = centroid(F[(kind, "idle")]), head_center(F[(kind, "idle")])
        for (k, name), im in F.items():
            if k != kind or name == "idle":
                continue
            c1, h1 = centroid(im), head_center(im)
            cs = math.dist(c0, c1)
            hs = math.dist(h0, h1)
            ov = F_canvas[(k, name)].overflow()
            report.append(f"{k + '_' + name:24s} {ov:6d}    {cs:8.2f}       {hs:6.2f}")
            if name.startswith("walk") and hs > 0.6:
                errors.append(f"{k}_{name}: head moves {hs:.2f}px (jitter)")
            if name.startswith("walk") and cs > 1.5:
                errors.append(f"{k}_{name}: body centre moves {cs:.2f}px (jitter)")

    # effects
    fx = {"knife_slash": knife_slash(), "muzzle_flash": muzzle_flash()}
    for n, cv in fx.items():
        im = cv.result()
        im.save(os.path.join(FX_DIR, f"{n}.png"))
        report.append(f"effect {n}.png {im.size[0]}x{im.size[1]} overflow {cv.overflow()}")

    report.append("")
    report.append("laser muzzle offset from sprite centre at 0 deg: idle %.1f, %.1f" % muzzle(LASER_IDLE["rh"], 0))
    for n, pose in SHOOT.items():
        report.append(f"laser muzzle offset {n}: %.1f, %.1f" % muzzle(pose["rh"], pose["wang"]))
    report += ["", "ERRORS: none" if not errors else "ERRORS:"] + [" - " + e for e in errors]
    text = "\n".join(report)
    print(text)
    with open(os.path.join(OUT, "maps", "animation_validation.txt"), "w") as f:
        f.write(text + "\n")
    if errors:
        sys.exit("animation validation failed")

    # previews
    slash = Image.open(os.path.join(FX_DIR, "knife_slash.png"))
    flash = Image.open(os.path.join(FX_DIR, "muzzle_flash.png"))
    charge = Image.open(os.path.join(FX_DIR, "laser_charge.png"))
    for anim, (names, durs, _loop) in ANIMS.items():
        kind = anim.rsplit("_", 1)[0]
        imgs = [F[(kind, n)] for n in names]
        effects = None
        if anim == "assassin_attack":
            effects = [None, None, [(slash, (12, 4))], None, None]
        if anim == "laser_guard_shoot":
            m1 = muzzle(SHOOT["shoot_1"]["rh"], SHOOT["shoot_1"]["wang"])
            m2 = muzzle(SHOOT["shoot_2"]["rh"], SHOOT["shoot_2"]["wang"])
            effects = [None, [(charge, m1)], [(flash, (m2[0] + 6, m2[1]))], None, None]
        if anim.endswith("death"):
            continue   # deaths are shown in the sheet; GIFs only for the six requested animations
        gif(anim, imgs, durs, effects)
    animation_sheet(F).convert("RGB").save(os.path.join(MOCK_DIR, "animation_sheet.png"))
    print("frames, effects, sheet and GIFs written")


if __name__ == "__main__":
    main()
