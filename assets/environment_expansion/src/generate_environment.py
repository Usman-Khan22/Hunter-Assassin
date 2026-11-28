"""
Hunter's Assassin - environment expansion sprites.

Run:  python generate_environment.py      (needs Pillow)

Reuses the drawing helpers and palette of the approved art generator
(art_style_sample/src/generate_art.py), so this folder must sit inside
art_style_sample/ as  art_style_sample/environment_expansion/.
Nothing in the approved png/ or maps/ folders is touched.

Every sprite is drawn with simple shapes at 8x resolution and scaled down
(LANCZOS), exactly like the approved assets.

Orientation conventions (see ASSET_SPECIFICATIONS.md):
  * characters / projectile face RIGHT (+X) at rotation 0
  * wall-mounted items (wall_console, wall_light, wall_pipe) have the wall
    along their TOP edge at rotation 0
  * 96x48 floor props are horizontal at rotation 0
"""

import math
import os
import sys

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "src"))
import generate_art as ga  # noqa: E402  (approved generator: palette + Canvas)

Canvas, P, OUT_W = ga.Canvas, ga.P, ga.OUT_W
PNG_DIR = os.path.join(HERE, "..", "png")

# extra colours, all in the same muted family as the approved palette
X = {
    "orange":      (255, 156, 44, 255),
    "orange_dark": (168, 88, 16, 255),
    "orange_hi":   (255, 214, 150, 255),
    "blue":        (82, 164, 255, 255),
    "blue_dim":    (52, 102, 168, 255),
    "cyan":        (96, 222, 236, 255),
    "screen":      (18, 30, 46, 255),
    "glass":       (60, 200, 190, 120),
    "mattress":    (78, 90, 118, 255),
    "blanket":     (62, 73, 98, 255),
    "pillow":      (176, 186, 206, 255),
    "vent":        (25, 30, 42, 255),
    "laser":       (255, 92, 60, 255),
    "laser_core":  (255, 236, 200, 255),
}


def shadow_rect(c, x0, y0, x1, y1, r=4):
    c.soft_shadow(lambda k: k.rect(x0, y0, x1, y1, fill=P["shadow"], radius=r), offset=(2, 3), blur=2)


def bolts(c, pts, r=1.3):
    for x, y in pts:
        c.circle(x, y, r, fill=P["steel_light"], outline=P["outline"], width=0.6)


def glow(c, draw_fn, blur=1.5):
    layer = Canvas(c.w, c.h)
    draw_fn(layer)
    c.img.alpha_composite(layer.img.filter(ImageFilter.GaussianBlur(ga.SS * blur)))


# ======================================================== required assets
def draw_wall_console():
    """48x24, wall along the top edge. Blue screen = facility system (not the yellow security terminal)."""
    c = Canvas(48, 24)
    c.soft_shadow(lambda k: k.rect(3, 0, 45, 20, fill=P["shadow"], radius=2), offset=(1, 2), blur=1.2)
    c.rect(3, 0, 45, 20, fill=P["steel_dark"], outline=P["outline"], width=OUT_W, radius=2)
    c.rect(6, 15, 42, 21.5, fill=P["steel"], outline=P["outline"], width=1.0, radius=1.5)   # front lip
    c.rect(7, 2.5, 27, 13, fill=X["screen"], outline=P["outline"], width=1.0, radius=1.2)
    for i, w in enumerate([14, 9, 12]):
        c.line([(9.5, 5 + i * 3), (9.5 + w, 5 + i * 3)], fill=X["blue"], width=1.2)
    # buttons
    for i, col in enumerate([P["green"], X["blue"], P["steel_light"]]):
        c.rect(30 + i * 4.6, 4, 33.4 + i * 4.6, 7.4, fill=col, outline=P["outline"], width=0.6, radius=0.7)
    c.rect(30, 9.5, 42.6, 12.5, fill=P["outline"], radius=0.8)                                # slot
    for i in range(6):
        c.line([(9 + i * 5.5, 18.2), (12 + i * 5.5, 18.2)], fill=P["steel_dark"], width=1.0)  # keys
    return c.result()


def draw_server_rack():
    c = Canvas(48, 48)
    shadow_rect(c, 6, 4, 42, 44)
    c.rect(6, 4, 42, 44, fill=P["steel_dark"], outline=P["outline"], width=OUT_W * 1.2, radius=3)
    c.line([(9, 6.5), (39, 6.5)], fill=P["steel"], width=1.2)
    for i in range(4):                       # server units
        y = 9.5 + i * 8.4
        c.rect(9, y, 39, y + 6.6, fill=(34, 40, 55, 255), outline=P["outline"], width=0.9, radius=1)
        for k in range(5):                   # vent slits
            c.line([(11.5 + k * 3, y + 1.8), (11.5 + k * 3, y + 4.8)], fill=P["steel_dark"], width=1.0)
        c.circle(32.5, y + 3.3, 1.3, fill=P["green"] if i % 2 == 0 else X["blue"])
        c.circle(36.3, y + 3.3, 1.3, fill=X["blue"] if i != 2 else P["green"])
    bolts(c, [(9, 42), (39, 42)], r=1.0)
    return c.result()


def draw_research_desk():
    c = Canvas(96, 48)
    shadow_rect(c, 3, 7, 93, 43)
    c.rect(3, 7, 93, 43, fill=P["steel"], outline=P["outline"], width=OUT_W * 1.2, radius=4)
    c.rect(6, 10, 90, 40, fill=(80, 92, 118, 255), radius=2)
    c.line([(6, 9), (90, 9)], fill=P["steel_light"], width=1.2)
    # monitor + keyboard
    c.rect(10, 11.5, 34, 24, fill=X["screen"], outline=P["outline"], width=1.1, radius=1.5)
    c.line([(13, 15), (26, 15)], fill=X["cyan"], width=1.2)
    c.line([(13, 18.5), (22, 18.5)], fill=X["cyan"], width=1.2)
    c.line([(13, 22), (18, 19.5), (22, 21), (30, 16.5)], fill=P["green"], width=1.0)
    c.rect(12, 28, 33, 35, fill=P["steel_dark"], outline=P["outline"], width=0.9, radius=1.2)
    for r in range(2):
        for k in range(6):
            c.rect(13.6 + k * 3.2, 29.6 + r * 2.6, 15.6 + k * 3.2, 30.9 + r * 2.6, fill=P["steel"], radius=0.4)
    # microscope (top-down: base, arm, round eyepiece)
    c.rect(42, 14, 54, 36, fill=P["steel_dark"], outline=P["outline"], width=1.0, radius=2.5)
    c.circle(48, 21, 4.6, fill=P["steel_light"], outline=P["outline"], width=1.0)
    c.circle(48, 21, 2, fill=P["outline"])
    c.circle(48, 31, 2.4, fill=X["cyan"], outline=P["outline"], width=0.7)
    # sample vial rack
    c.rect(60, 12, 88, 21, fill=P["steel_dark"], outline=P["outline"], width=0.9, radius=1.5)
    for i, col in enumerate([P["green"], X["cyan"], P["green"], X["cyan"]]):
        c.circle(64 + i * 6.6, 16.5, 2.3, fill=col, outline=P["outline"], width=0.7)
    # petri dishes / tray
    c.rect(60, 25, 88, 37, fill=(64, 75, 99, 255), outline=P["outline"], width=0.9, radius=2)
    for i in range(3):
        c.circle(65.5 + i * 8.5, 31, 3.4, fill=(150, 220, 210, 255), outline=P["outline"], width=0.7)
        c.circle(65 + i * 8.5, 30.5, 1.1, fill=P["green_dark"])
    return c.result()


def draw_specimen_tank():
    c = Canvas(48, 48)
    c.soft_shadow(lambda k: k.circle(24, 24, 20, fill=P["shadow"]), offset=(2, 3), blur=2)
    c.circle(24, 24, 20.5, fill=P["steel_dark"], outline=P["outline"], width=OUT_W * 1.2)
    c.circle(24, 24, 17.5, fill=P["steel"], outline=P["outline"], width=1.0)
    # glass + liquid
    c.circle(24, 24, 14.5, fill=(26, 60, 66, 255), outline=P["outline"], width=1.1)
    glow(c, lambda k: k.circle(24, 24, 11, fill=(70, 230, 200, 110)), blur=2.2)
    # specimen silhouette
    c.ellipse(19, 18.5, 29, 30, fill=(30, 90, 84, 255))
    c.circle(24, 18.8, 3.6, fill=(30, 90, 84, 255))
    for x0, x1, y0, y1 in [(19.5, 15.5, 22, 19), (28.5, 32.5, 22, 19), (21, 18.5, 29, 33), (27, 29.5, 29, 33)]:
        c.line([(x0, y0), (x1, y1)], fill=(30, 90, 84, 255), width=1.6)
    # bubbles + glass highlight
    for x, y, r in [(30, 28, 1.1), (31.5, 24, 0.8), (17, 27, 0.9)]:
        c.circle(x, y, r, fill=(170, 250, 235, 200))
    c.d.arc(c._box(13, 13, 35, 35), start=200, end=250, fill=(210, 255, 245, 190), width=int(c.s(1.6)))
    bolts(c, [(24, 5), (24, 43), (5, 24), (43, 24)], r=1.2)
    return c.result()


def draw_power_generator():
    c = Canvas(96, 96)
    shadow_rect(c, 6, 6, 90, 90, r=7)
    c.rect(6, 6, 90, 90, fill=P["steel"], outline=P["outline"], width=OUT_W * 1.4, radius=7)
    c.line([(12, 9.5), (84, 9.5)], fill=P["steel_light"], width=1.5)
    # warning strip along the bottom
    band = Canvas(96, 96)
    band.rect(10, 78, 86, 86, fill=P["yellow"])
    for i in range(-2, 16):
        x = 10 + i * 7
        band.poly([(x, 86), (x + 3.5, 86), (x + 8, 78), (x + 4.5, 78)], fill=P["outline"])
    mask = Image.new("L", c.img.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle(band._box(10, 78, 86, 86), radius=band.s(1.5), fill=255)
    c.img.paste(band.img, (0, 0), mask)
    c.rect(10, 78, 86, 86, outline=P["outline"], width=1.1, radius=1.5)
    # central turbine
    c.rect(14, 14, 62, 72, fill=P["steel_dark"], outline=P["outline"], width=1.2, radius=4)
    c.circle(38, 43, 20, fill=(34, 40, 55, 255), outline=P["outline"], width=1.3)
    for a in range(0, 360, 45):
        c.d.pieslice(c._box(21, 26, 55, 60), start=a, end=a + 24, fill=P["steel"])
    c.circle(38, 43, 20, outline=P["outline"], width=1.3)
    c.circle(38, 43, 5.5, fill=P["steel_light"], outline=P["outline"], width=1.1)
    # energy cells
    for x in (68, 78):
        c.rect(x - 4, 16, x + 4, 72, fill=P["outline"], radius=4)
        glow(c, lambda k, x=x: k.rect(x - 3.5, 20, x + 3.5, 68, fill=(96, 222, 236, 150), radius=3), blur=1.2)
        c.rect(x - 2.6, 19, x + 2.6, 69, fill=X["cyan"], radius=2.6)
        c.rect(x - 0.8, 22, x + 0.8, 66, fill=(220, 255, 255, 255), radius=0.8)
        for y in (32, 45, 58):
            c.line([(x - 3.4, y), (x + 3.4, y)], fill=P["outline"], width=1.2)
    # warning lamp + bolts
    c.circle(80, 10.5, 2.4, fill=X["orange"], outline=P["outline"], width=0.8)
    bolts(c, [(11, 11), (85, 74), (11, 74)], r=1.5)
    return c.result()


def draw_locker():
    c = Canvas(48, 48)
    shadow_rect(c, 7, 4, 41, 44, r=3)
    c.rect(7, 4, 41, 44, fill=P["steel"], outline=P["outline"], width=OUT_W * 1.2, radius=3)
    c.line([(24, 5), (24, 43)], fill=P["outline"], width=1.4)
    for x0 in (10, 27):
        c.rect(x0, 7, x0 + 11, 41, fill=(84, 97, 124, 255), outline=P["outline"], width=0.9, radius=1.5)
        for k in range(3):
            c.line([(x0 + 2, 10 + k * 2.6), (x0 + 9, 10 + k * 2.6)], fill=P["steel_dark"], width=1.0)
        c.rect(x0 + 2.5, 20, x0 + 8.5, 23.5, fill=P["card"], outline=P["outline"], width=0.6, radius=0.6)
    # handles
    c.rect(19.5, 26, 21.5, 33, fill=P["steel_light"], outline=P["outline"], width=0.6, radius=0.8)
    c.rect(26.5, 26, 28.5, 33, fill=P["steel_light"], outline=P["outline"], width=0.6, radius=0.8)
    # ID tags (yellow = facility property)
    c.rect(12, 20, 19, 23.5, fill=P["yellow"], radius=0.6)
    c.line([(9, 6), (39, 6)], fill=P["steel_light"], width=1.0)
    return c.result()


def draw_cell_bed():
    c = Canvas(96, 48)
    shadow_rect(c, 4, 7, 92, 41, r=4)
    c.rect(4, 7, 92, 41, fill=P["steel_dark"], outline=P["outline"], width=OUT_W * 1.2, radius=4)
    c.rect(7.5, 10.5, 88.5, 37.5, fill=X["mattress"], outline=P["outline"], width=1.0, radius=3)
    c.rect(10, 13, 25, 35, fill=X["pillow"], outline=P["outline"], width=1.0, radius=4)
    c.line([(13, 16), (13, 32)], fill=(150, 160, 182, 255), width=1.0)
    # blanket
    c.rect(38, 10.5, 88.5, 37.5, fill=X["blanket"], outline=P["outline"], width=1.0, radius=3)
    c.rect(38, 10.5, 44, 37.5, fill=(90, 104, 134, 255), outline=P["outline"], width=0.9, radius=1.5)
    for y in (17, 24, 31):
        c.line([(48, y), (84, y)], fill=(56, 66, 89, 255), width=1.0)
    bolts(c, [(7, 10), (89, 10), (7, 38), (89, 38)], r=1.0)
    return c.result()


def draw_floor_vent():
    """Flush with the floor: no drop shadow, only a dark recess."""
    c = Canvas(48, 48)
    c.rect(6, 6, 42, 42, fill=P["steel_dark"], outline=P["outline"], width=OUT_W, radius=3)
    c.rect(9, 9, 39, 39, fill=X["vent"], radius=1.5)
    for i in range(7):
        y = 11 + i * 4
        c.rect(10, y, 38, y + 2, fill=P["steel_dark"], radius=0.6)
        c.line([(10, y + 0.4), (38, y + 0.4)], fill=(74, 85, 110, 255), width=0.6)
    bolts(c, [(8.5, 8.5), (39.5, 8.5), (8.5, 39.5), (39.5, 39.5)], r=1.0)
    return c.result()


def draw_hazard_marking():
    """Seamless painted stripes: tiles placed side by side join into one strip."""
    c = Canvas(48, 48)
    c.rect(0, 0, 48, 48, fill=(255, 204, 64, 190))
    for i in range(-5, 9):
        x = i * 12
        c.poly([(x, 48), (x + 6, 48), (x + 54, 0), (x + 48, 0)], fill=(14, 17, 24, 200))
    return c.result()


def draw_wall_light():
    """24x12, wall along the top edge; small glow kept inside the frame."""
    c = Canvas(24, 12)
    glow(c, lambda k: k.ellipse(4, 3, 20, 12, fill=(190, 245, 255, 120)), blur=1.2)
    c.rect(3, 0, 21, 6.5, fill=P["steel_dark"], outline=P["outline"], width=1.1, radius=1.4)
    c.rect(6, 4.5, 18, 8.5, fill=(210, 250, 255, 255), outline=P["outline"], width=0.9, radius=1.6)
    c.line([(8, 6.5), (16, 6.5)], fill=(255, 255, 255, 255), width=1.0)
    return c.result()


# ======================================================== optional assets
def draw_tool_station():
    c = Canvas(96, 48)
    shadow_rect(c, 3, 7, 93, 43)
    c.rect(3, 7, 93, 43, fill=P["steel"], outline=P["outline"], width=OUT_W * 1.2, radius=4)
    c.rect(6, 10, 90, 40, fill=(80, 92, 118, 255), radius=2)
    # orange toolbox
    c.rect(9, 13, 31, 33, fill=X["orange"], outline=P["outline"], width=1.1, radius=2)
    c.rect(9, 20, 31, 22, fill=X["orange_dark"])
    c.rect(15, 15, 25, 17.5, fill=P["outline"], radius=1)
    # wrench
    c.line([(38, 33), (52, 17)], fill=P["steel_light"], width=2.6)
    c.circle(53, 15.5, 3.8, fill=P["steel_light"], outline=P["outline"], width=0.9)
    c.circle(54.3, 14.2, 1.6, fill=(80, 92, 118, 255))
    # vice
    c.rect(60, 14, 74, 24, fill=P["steel_dark"], outline=P["outline"], width=1.0, radius=1.5)
    c.rect(64, 24, 70, 33, fill=P["steel_dark"], outline=P["outline"], width=1.0, radius=1)
    c.line([(62, 19), (72, 19)], fill=P["outline"], width=1.2)
    # parts tray + bolts
    c.rect(78, 13, 88, 35, fill=P["steel_dark"], outline=P["outline"], width=0.9, radius=1.5)
    bolts(c, [(81, 17), (85, 21), (81, 25), (85, 29), (81, 32)], r=1.1)
    c.line([(37, 37), (56, 37)], fill=P["yellow"], width=1.4)  # tape measure
    return c.result()


def draw_control_desk():
    """Operator side at the bottom (screens along the top edge)."""
    c = Canvas(96, 48)
    shadow_rect(c, 3, 6, 93, 42)
    c.rect(3, 6, 93, 42, fill=P["steel_dark"], outline=P["outline"], width=OUT_W * 1.2, radius=5)
    c.rect(6, 28, 90, 39, fill=P["steel"], outline=P["outline"], width=1.0, radius=3)   # console lip
    for i in range(3):
        x = 8 + i * 27.5
        c.rect(x, 9, x + 25, 24, fill=X["screen"], outline=P["outline"], width=1.1, radius=1.5)
        if i == 1:   # camera feed grid
            c.line([(x + 12.5, 10), (x + 12.5, 23)], fill=X["blue_dim"], width=0.9)
            c.line([(x + 1, 16.5), (x + 24, 16.5)], fill=X["blue_dim"], width=0.9)
            c.circle(x + 6, 13, 1.4, fill=P["yellow"])
            c.circle(x + 18, 20, 1.4, fill=P["red"])
        else:
            for k, w in enumerate([16, 10, 13]):
                c.line([(x + 3, 12.5 + k * 3.6), (x + 3 + w, 12.5 + k * 3.6)], fill=X["blue"], width=1.2)
    for k in range(9):
        col = [P["yellow"], X["blue"], P["steel_light"]][k % 3]
        c.rect(10 + k * 8.6, 31, 15 + k * 8.6, 34.5, fill=col, outline=P["outline"], width=0.6, radius=0.8)
    return c.result()


def draw_cargo_container():
    c = Canvas(96, 48)
    shadow_rect(c, 2, 5, 94, 44, r=3)
    c.rect(2, 5, 94, 44, fill=(78, 92, 120, 255), outline=P["outline"], width=OUT_W * 1.3, radius=3)
    for i in range(14):                         # corrugated roof ridges
        x = 10 + i * 5.7
        c.line([(x, 9), (x, 40)], fill=(62, 74, 98, 255), width=1.6)
        c.line([(x + 1.6, 9), (x + 1.6, 40)], fill=(98, 112, 142, 255), width=0.8)
    c.rect(36, 19, 60, 30, fill=(62, 74, 98, 255), outline=P["outline"], width=0.9, radius=1.2)
    c.line([(40, 24.5), (56, 24.5)], fill=P["steel_light"], width=1.8)  # stencil bar
    for x, y in [(2, 5), (88, 5), (2, 38), (88, 38)]:
        c.rect(x, y, x + 6, y + 6, fill=P["yellow"], outline=P["outline"], width=1, radius=1.5)
    return c.result()


def draw_floor_arrow(color=(205, 214, 230, 150)):
    """Painted chevron pointing RIGHT. Flush with the floor."""
    c = Canvas(48, 48)
    for dx in (0, 11):
        c.poly([(9 + dx, 12), (17 + dx, 12), (29 + dx, 24), (17 + dx, 36), (9 + dx, 36), (21 + dx, 24)], fill=color)
    return c.result()


def draw_wall_pipe():
    """96x24, wall along the top edge."""
    c = Canvas(96, 24)
    c.soft_shadow(lambda k: k.rect(0, 2, 96, 17, fill=P["shadow"], radius=3), offset=(0, 2), blur=1.2)
    for y0, y1, col in [(2, 8.5, P["steel"]), (10, 16.5, P["steel_dark"])]:
        c.rect(-2, y0, 98, y1, fill=col, outline=P["outline"], width=1.0)
        c.line([(-2, y0 + 1.6), (98, y0 + 1.6)], fill=P["steel_light"], width=0.9)
    for x in (12, 48, 84):                      # brackets
        c.rect(x - 2.5, 0, x + 2.5, 18, fill=P["steel_dark"], outline=P["outline"], width=0.9, radius=1)
    c.circle(66, 13.2, 5, fill=X["orange"], outline=P["outline"], width=1.0)   # valve wheel
    c.circle(66, 13.2, 1.8, fill=P["outline"])
    c.line([(61.5, 13.2), (70.5, 13.2)], fill=P["outline"], width=0.9)
    c.line([(66, 8.7), (66, 17.7)], fill=P["outline"], width=0.9)
    return c.result()


def draw_storage_rack():
    """Shelf seen from above, loaded with small boxes. Used in Storage."""
    c = Canvas(96, 48)
    shadow_rect(c, 3, 6, 93, 42, r=2)
    c.rect(3, 6, 93, 42, fill=P["steel_dark"], outline=P["outline"], width=OUT_W * 1.2, radius=2)
    for x in (3, 33, 63, 93):
        c.rect(x - 1.8, 5, x + 1.8, 43, fill=P["steel"], outline=P["outline"], width=0.8)
    boxes = [(7, 10, 19, 22), (21, 12, 30, 21), (8, 25, 28, 38), (36, 9, 52, 24), (54, 11, 61, 20),
             (37, 27, 47, 38), (49, 26, 60, 38), (66, 10, 80, 22), (82, 12, 90, 20), (67, 25, 89, 38)]
    for i, (x0, y0, x1, y1) in enumerate(boxes):
        col = (118, 130, 156, 255) if i % 3 else (150, 132, 104, 255)
        c.rect(x0, y0, x1, y1, fill=col, outline=P["outline"], width=0.9, radius=1)
        c.line([((x0 + x1) / 2, y0 + 0.8), ((x0 + x1) / 2, y1 - 0.8)], fill=(0, 0, 0, 60), width=1.0)
    return c.result()


# ======================================================== laser guard + effects
# laser_guard.png is drawn by generate_animations.draw_guard(laser=True) so idle + animation frames share one source.


def draw_laser_projectile():
    c = Canvas(24, 8)
    glow(c, lambda k: k.rect(1, 1.2, 23, 6.8, fill=(255, 80, 50, 200), radius=2.8), blur=0.9)
    c.rect(2.5, 2, 22.5, 6, fill=X["laser"], radius=2)
    c.rect(6, 3.1, 21.8, 4.9, fill=X["laser_core"], radius=0.9)
    return c.result()


def draw_laser_charge():
    c = Canvas(24, 24)
    glow(c, lambda k: k.circle(12, 12, 9, fill=(255, 100, 50, 150)), blur=1.6)
    c.circle(12, 12, 8.5, outline=(255, 140, 70, 230), width=1.2)
    for a in range(0, 360, 60):
        r0, r1 = 9.5, 11.2
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        c.line([(12 + ca * r0, 12 + sa * r0), (12 + ca * r1, 12 + sa * r1)], fill=(255, 170, 90, 220), width=1.2)
    c.circle(12, 12, 4.6, fill=X["laser"])
    c.circle(12, 12, 2.4, fill=X["laser_core"])
    return c.result()


def draw_laser_impact():
    c = Canvas(24, 24)
    glow(c, lambda k: k.circle(12, 12, 8, fill=(255, 110, 50, 170)), blur=1.6)
    pts = []
    for i in range(16):
        a = math.radians(i * 22.5 + 8)
        r = 10.5 if i % 2 == 0 else 4.4
        pts.append((12 + math.cos(a) * r, 12 + math.sin(a) * r))
    c.poly(pts, fill=X["laser"])
    c.circle(12, 12, 3.8, fill=X["laser_core"])
    for a, d in [(35, 9.5), (150, 10), (260, 8.8)]:
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        c.circle(12 + ca * d, 12 + sa * d, 0.9, fill=X["orange_hi"])
    return c.result()


# ======================================================== registry
# name: (draw function, expected size)
REQUIRED = {
    "wall_console":    (draw_wall_console, (48, 24)),
    "server_rack":     (draw_server_rack, (48, 48)),
    "research_desk":   (draw_research_desk, (96, 48)),
    "specimen_tank":   (draw_specimen_tank, (48, 48)),
    "power_generator": (draw_power_generator, (96, 96)),
    "locker":          (draw_locker, (48, 48)),
    "cell_bed":        (draw_cell_bed, (96, 48)),
    "floor_vent":      (draw_floor_vent, (48, 48)),
    "hazard_marking":  (draw_hazard_marking, (48, 48)),
    "wall_light":      (draw_wall_light, (24, 12)),
}
OPTIONAL = {
    "tool_station":     (draw_tool_station, (96, 48)),
    "control_desk":     (draw_control_desk, (96, 48)),
    "cargo_container":  (draw_cargo_container, (96, 48)),
    "floor_arrow":      (draw_floor_arrow, (48, 48)),
    "floor_arrow_exit": (lambda: draw_floor_arrow((76, 222, 118, 170)), (48, 48)),
    "wall_pipe":        (draw_wall_pipe, (96, 24)),
    "storage_rack":     (draw_storage_rack, (96, 48)),
}
CHARACTERS = {
    "laser_guard":      (lambda: __import__("generate_animations").draw_guard(laser=True).result(), (40, 40)),
    "laser_projectile": (draw_laser_projectile, (24, 8)),
    "laser_charge":     (draw_laser_charge, (24, 24)),
    "laser_impact":     (draw_laser_impact, (24, 24)),
}
ALL = {**REQUIRED, **OPTIONAL, **CHARACTERS}


def main():
    fx_dir = os.path.join(PNG_DIR, "effects")
    os.makedirs(fx_dir, exist_ok=True)
    for name, (fn, size) in ALL.items():
        im = fn()
        assert im.size == size, f"{name}: {im.size} != {size}"
        assert im.mode == "RGBA"
        out_dir = fx_dir if name.startswith("laser_") and name != "laser_guard" else PNG_DIR
        im.save(os.path.join(out_dir, f"{name}.png"))
        print(f"{name + '.png':24s} {im.size[0]}x{im.size[1]}")


if __name__ == "__main__":
    main()
