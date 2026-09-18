#!/usr/bin/env python3
"""Draws ArrowEch's packaging art in the plug-in's sticker style (cream paper, ink outlines,
hard offset shadows, the butter blob). Needs Pillow. Outputs are committed, so CI never runs this.

    python3 scripts/make-art.py
"""
import math
import os
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "packaging", "art")
RES = os.path.join(ROOT, "packaging", "resources")

PAPER = (246, 237, 220)
PAPER_DARK = (38, 33, 44)
INK = (23, 19, 28)
WHITE = (255, 253, 247)
GREY = (217, 207, 189)
GREY_DARK = (58, 51, 66)
BUTTER = (255, 210, 63)
TOMATO = (255, 107, 87)
BUBBLEGUM = (255, 143, 199)
SKY = (92, 200, 255)
MINT = (95, 224, 183)
LILAC = (180, 156, 255)

FONT = "/System/Library/Fonts/Supplemental/Arial Rounded Bold.ttf"
SS = 4  # supersampling factor for smooth edges


class Canvas:
    """Draws in logical points; renders at scale * SS and downsamples."""

    def __init__(self, w, h, scale=1, bg=PAPER, transparent=False):
        self.w, self.h, self.k = w, h, scale * SS
        self.out = (w * scale, h * scale)
        mode = "RGBA"
        self.img = Image.new(mode, (w * self.k, h * self.k), (0, 0, 0, 0) if transparent else bg + (255,))
        self.d = ImageDraw.Draw(self.img)

    def p(self, v):
        return v * self.k

    def box(self, x, y, w, h):
        return [self.p(x), self.p(y), self.p(x + w), self.p(y + h)]

    def ellipse(self, x, y, w, h, fill=None, outline=None, width=0):
        self.d.ellipse(self.box(x, y, w, h), fill=fill, outline=outline, width=round(self.p(width)))

    def rrect(self, x, y, w, h, r, fill=None, outline=None, width=0):
        self.d.rounded_rectangle(self.box(x, y, w, h), radius=self.p(r), fill=fill, outline=outline,
                                 width=round(self.p(width)))

    def polygon(self, pts, fill):
        self.d.polygon([(self.p(x), self.p(y)) for x, y in pts], fill=fill)

    def line(self, pts, fill, width):
        pts = [(self.p(x), self.p(y)) for x, y in pts]
        self.d.line(pts, fill=fill, width=round(self.p(width)), joint="curve")
        r = self.p(width) / 2
        for x, y in (pts[0], pts[-1]):
            self.d.ellipse([x - r, y - r, x + r, y + r], fill=fill)

    def text(self, x, y, s, size, fill, anchor="la", stroke=0, stroke_fill=None):
        f = ImageFont.truetype(FONT, round(self.p(size)))
        self.d.text((self.p(x), self.p(y)), s, font=f, fill=fill, anchor=anchor,
                    stroke_width=round(self.p(stroke)), stroke_fill=stroke_fill)

    def text_width(self, s, size):
        f = ImageFont.truetype(FONT, round(self.p(size)))
        return f.getlength(s) / self.k

    def save(self, path):
        self.img.resize(self.out, Image.LANCZOS).save(path)
        print("wrote", os.path.relpath(path, ROOT))


def dots(c, colour, step=22, r=1.6):
    for row, y in enumerate(range(10, c.h + step, step)):
        for x in range(10 + (step // 2 if row % 2 else 0), c.w + step, step):
            c.ellipse(x - r, y - r, r * 2, r * 2, fill=colour)


def blob(c, cx, bottom, w, h, look=(0.0, 0.0), happy=True):
    """The mascot, drawn like Source/UI/Blob.h."""
    x, y = cx - w / 2, bottom - h
    lw = max(2.0, w * 0.022)
    c.ellipse(x + w * 0.03, y + w * 0.03, w, h, fill=INK)
    c.ellipse(x, y, w, h, fill=BUTTER, outline=INK, width=lw)
    ck = w * 0.14
    for ex in (x + w * 0.2, x + w * 0.8):
        c.ellipse(ex - ck / 2, y + h * 0.62 - ck * 0.3, ck, ck * 0.6, fill=BUBBLEGUM)
    es = w * 0.26
    for ex in (cx - w * 0.17, cx + w * 0.17):
        ey = y + h * 0.38
        c.ellipse(ex - es / 2, ey - es / 2, es, es, fill=WHITE, outline=INK, width=lw * 0.8)
        pu = es * 0.5
        px, py = ex + look[0] * es * 0.2, ey + look[1] * es * 0.2
        c.ellipse(px - pu / 2, py - pu / 2, pu, pu, fill=INK)
        hl = pu * 0.3
        c.ellipse(px - pu * 0.15 - hl / 2, py - pu * 0.15 - hl / 2, hl, hl, fill=WHITE)
    mx, my = cx, y + h * 0.68
    if happy:
        mw, mh = w * 0.26, h * 0.2
        c.d.chord(c.box(mx - mw / 2, my - mh / 2, mw, mh), 0, 180, fill=INK)
        tw = mw * 0.5
        c.d.chord(c.box(mx - tw / 2, my + mh * 0.05, tw, mh * 0.4), 180, 360, fill=TOMATO)
    else:
        c.d.arc(c.box(mx - w * 0.1, my - h * 0.05, w * 0.2, h * 0.1), 10, 170, fill=INK, width=round(c.p(lw)))


def sticker_text(c, x, y, s, size, fill, anchor="la", shadow=3.0, stroke=2.5):
    c.text(x + shadow, y + shadow, s, size, INK, anchor, stroke, INK)
    c.text(x, y, s, size, fill, anchor, stroke, INK)


def bubble(c, x, y, w, h, tail, lines, size=15):
    """Speech bubble with a hard shadow; tail = (tip_x, tip_y, base_x) on the bottom edge."""
    tx, ty, bx = tail
    c.rrect(x + 4, y + 4, w, h, 16, fill=INK)
    c.polygon([(bx - 10 + 4, y + h - 2 + 4), (bx + 14 + 4, y + h - 2 + 4), (tx + 4, ty + 4)], INK)
    c.polygon([(bx - 13, y + h - 3), (bx + 17, y + h - 3), (tx, ty)], INK)
    c.rrect(x, y, w, h, 16, fill=WHITE, outline=INK, width=3)
    c.polygon([(bx - 9, y + h - 4), (bx + 13, y + h - 4), (tx + 2, ty - 5)], WHITE)
    lh = size * 1.3
    top = y + h / 2 - lh * len(lines) / 2 + lh / 2
    for i, s in enumerate(lines):
        c.text(x + w / 2, top + i * lh, s, size, INK, "mm")


def arrow(c, pts, colour, width=7.0):
    """Chunky hand-drawn arrow along a quadratic curve (p0, control, p1), with a hard shadow."""
    (x0, y0), (cx, cy), (x1, y1) = pts
    curve = [((1 - t) ** 2 * x0 + 2 * (1 - t) * t * cx + t * t * x1,
              (1 - t) ** 2 * y0 + 2 * (1 - t) * t * cy + t * t * y1) for t in [i / 40 for i in range(41)]]
    ang = math.atan2(y1 - curve[-4][1], x1 - curve[-4][0])
    head = 20
    tip = (x1 + math.cos(ang) * 6, y1 + math.sin(ang) * 6)
    wings = [(x1 + math.cos(ang + a) * head, y1 + math.sin(ang + a) * head) for a in (2.5, -2.5)]
    for dx, col, extra in ((3.5, INK, 0), (0, INK, 5), (0, colour, 0)):
        c.line([(x + dx, y + dx) for x, y in curve], col, width + extra)
    for dx, col in ((3.5, INK), (0, INK)):
        grow = 4 if dx == 0 else 0
        c.polygon([(tip[0] + dx + math.cos(ang) * grow, tip[1] + dx + math.sin(ang) * grow)]
                  + [(wx + dx + math.cos(ang + s) * grow, wy + dx + math.sin(ang + s) * grow)
                     for (wx, wy), s in zip(wings, (2.5, -2.5))], col)
    c.polygon([tip] + wings, colour)


# ---------------------------------------------------------------------------------------------
# DMG window background. Icon centres must match packaging/dmg-settings.py.
DMG_W, DMG_H = 660, 420
PKG_AT = (470, 205)
README_AT = (590, 345)


def dmg_background(scale):
    c = Canvas(DMG_W, DMG_H, scale)
    dots(c, GREY)
    # Title sticker, top left.
    sticker_text(c, 34, 30, "ArrowEch", 46, BUTTER)
    c.text(38, 92, "says it again. and again.", 15, INK)
    # Colour pills, like the rack strip.
    for i, col in enumerate((TOMATO, BUTTER, MINT, SKY, LILAC, BUBBLEGUM)):
        c.rrect(38 + i * 30 + 3, 122 + 3, 24, 12, 6, fill=INK)
        c.rrect(38 + i * 30, 122, 24, 12, 6, fill=col, outline=INK, width=2)
    # Target spot behind the installer icon.
    # Finder puts the icon's name just under it, so the ring sits a little high to keep it clear.
    px, py = PKG_AT
    r, cy = 58, py - 8
    c.ellipse(px - r + 5, cy - r + 5, r * 2, r * 2, fill=INK)
    c.ellipse(px - r, cy - r, r * 2, r * 2, fill=WHITE, outline=INK, width=3)
    c.ellipse(px - r + 9, cy - r + 9, (r - 9) * 2, (r - 9) * 2, outline=BUTTER, width=5)
    # Blob and its advice.
    blob(c, 150, 392, 170, 140, look=(1.0, -0.6))
    bubble(c, 44, 170, 250, 70, (190, 262, 170), ["double-click the box.", "i'll wait here."])
    arrow(c, [(298, 200), (350, 160), (384, 196)], TOMATO)
    c.text(398, 398, "then rescan plug-ins. that's it.", 12, INK, "mm")
    return c


# ---------------------------------------------------------------------------------------------
# Installer window background (drawn bottom-left, under the step list). One per appearance.
def installer_background(dark):
    c = Canvas(620, 418, 2, PAPER_DARK if dark else PAPER)
    dots(c, GREY_DARK if dark else GREY)
    blob(c, 92, 404, 132, 110, look=(0.9, -0.9))
    return c


# ---------------------------------------------------------------------------------------------
# App / volume icon: squircle tile with echo rings and the blob, macOS 11 grid (824 of 1024).
def app_icon():
    c = Canvas(1024, 1024, 1, transparent=True)
    m, s = 100, 824
    c.rrect(m + 14, m + 22, s, s, 185, fill=INK + (255,))
    c.rrect(m, m, s, s, 185, fill=SKY, outline=INK, width=22)
    cx, cy = 512, 600
    for i, col in enumerate((BUBBLEGUM, MINT, BUTTER)):
        r = 380 - i * 80
        c.d.arc(c.box(cx - r, cy - r, r * 2, r * 2), 200, 340, fill=INK, width=round(c.p(34)))
        q = r - 7  # Pillow grows arc width inwards, so inset the colour to leave ink on both sides
        c.d.arc(c.box(cx - q, cy - q, q * 2, q * 2), 202, 338, fill=col, width=round(c.p(20)))
    blob(c, 512, 850, 420, 350, look=(0.0, 0.3))
    return c


def write_icns(png, out):
    with tempfile.TemporaryDirectory() as tmp:
        iconset = os.path.join(tmp, "icon.iconset")
        os.mkdir(iconset)
        src = Image.open(png)
        for size in (16, 32, 128, 256, 512):
            for mult, suffix in ((1, ""), (2, "@2x")):
                src.resize((size * mult,) * 2, Image.LANCZOS).save(
                    os.path.join(iconset, f"icon_{size}x{size}{suffix}.png"))
        subprocess.run(["iconutil", "-c", "icns", iconset, "-o", out], check=True)
    print("wrote", os.path.relpath(out, ROOT))


def main():
    os.makedirs(ART, exist_ok=True)
    one = os.path.join(ART, "dmg-background.png")
    two = os.path.join(ART, "dmg-background@2x.png")
    dmg_background(1).save(one)
    dmg_background(2).save(two)
    tiff = os.path.join(ART, "dmg-background.tiff")
    subprocess.run(["tiffutil", "-cathidpicheck", one, two, "-out", tiff], check=True, capture_output=True)
    print("wrote", os.path.relpath(tiff, ROOT))

    installer_background(False).save(os.path.join(RES, "background.png"))
    installer_background(True).save(os.path.join(RES, "background-dark.png"))

    icon = os.path.join(ART, "icon.png")
    app_icon().save(icon)
    write_icns(icon, os.path.join(ART, "ArrowEch.icns"))


if __name__ == "__main__":
    main()
