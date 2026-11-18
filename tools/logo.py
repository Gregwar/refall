#!/usr/bin/env python3
"""The REFALL logo drawn at any size, as the game draws it (Menu::drawOrbitLogo, src/menuwin.cpp): a banded blue
planet lit from the top left, a tilted gold ring round it (its far half behind the planet, its near half in front),
pieces of the game riding the ring, the word letter-spaced under it. Two looks: "color" (the logo) and "outline" (its
white contours only, to lay over a picture). The window icon is the planet and its ring alone, steeper, to fill the
square (one piece from 64 px; none below, too small to read).

usage: tools/logo.py OUT.png [--size PX] [--outline]
       tools/logo.py --icon                           (the window icon: data/icon.png, src/icon.inc, data/refall.ico)
"""
import argparse
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, "src", "embed", "JosefinSans-Light.ttf")
ICON_SIZES = (16, 32, 48, 64, 128)
SS = 2  # supersampling

# the ring's bands (inner and outer radius, as a fraction of the ring's ellipse; opacity; colour)
BANDS = [(0.76, 0.80, 0.22, 0xb08850), (0.81, 0.90, 0.42, 0xc89a58), (0.905, 0.955, 0.8, 0xe8c06a),
         (0.975, 0.99, 0.9, 0xf6dca0)]
# the pieces on the ring: their cells, colour, place on the ring (radians: > 0..pi the near half) and turn
PIECES = [([(-1, 0), (0, 0), (1, 0), (0, -1)], 0xe854f2, 3.6, 1.7),        # T
          ([(-1, 0), (0, 0), (1, 0), (1, -1)], 0xfa9618, 2.35, 1.7),       # L
          ([(-1, 0), (0, 0), (0, -1), (1, -1)], 0xae5afd, 5.6, 0.4),       # S
          ([(-0.5, -0.5), (0.5, -0.5), (-0.5, 0.5), (0.5, 0.5)], 0x48a4fe, 0.55, 0.4)]  # O
ICON_PIECE = [([(-1, 0), (0, 0), (1, 0), (1, -1)], 0xfa9618, 1.0, -0.62)]


def _c(h):
    return np.array([(h >> 16) & 255, (h >> 8) & 255, h & 255], np.float32)


class _Canvas:
    """S x S premultiplied RGBA (float, 0..255 for colour, 0..1 for alpha), painted back to front."""

    def __init__(self, S):
        self.S = S
        self.y, self.x = np.mgrid[0:S, 0:S].astype(np.float32) + 0.5
        self.rgb = np.zeros((S, S, 3), np.float32)
        self.a = np.zeros((S, S), np.float32)

    def over(self, rgb, a):
        a = np.clip(a, 0, 1)
        self.rgb = self.rgb * (1 - a[..., None]) + np.asarray(rgb, np.float32) * a[..., None]
        self.a = self.a * (1 - a) + a

    def image(self, N):
        if self.S == N:
            rgb = self.rgb / np.maximum(self.a, 1e-6)[..., None]
            return Image.fromarray(np.dstack([np.clip(rgb, 0, 255), np.clip(self.a * 255, 0, 255)]).astype(np.uint8), "RGBA")
        rgb = self.rgb / np.maximum(self.a, 1e-6)[..., None]
        im = Image.fromarray(np.dstack([np.clip(rgb, 0, 255), np.clip(self.a * 255, 0, 255)]).astype(np.uint8), "RGBA")
        return im.resize((N, N), Image.LANCZOS)


def _draw(N, word, outline, icon=False, t=0.0, ss=SS):
    """`t`: seconds into its motion (the planet turning, the pieces riding round, a light running on the ring)."""
    S = N * ss
    cv = _Canvas(S)
    x, y = cv.x, cv.y
    if icon:
        cx, cy, R, tilt, ra, rb, pieces, cs0 = S / 2, S / 2, 0.31 * S, -0.62, 0.495 * S, 0.17 * S, ICON_PIECE, 0.058 * S
        pieces = pieces if N >= 64 else []
    else:
        cx, cy, R, tilt, ra, rb, pieces, cs0 = S / 2, 0.40 * S, 0.165 * S, -0.24, 0.40 * S, 0.087 * S, PIECES, 0.017 * S
    ct, st = math.cos(tilt), math.sin(tilt)
    dx, dy = x - cx, y - cy
    ux, uy = dx * ct + dy * st, -dx * st + dy * ct
    rr = np.sqrt((ux / ra) ** 2 + (uy / rb) ** 2)
    near = uy > 0
    d = np.sqrt(dx * dx + dy * dy) / R  # the planet: < 1 inside
    line = max(1.5, 0.0058 * S)  # an outline's width
    white = _c(0xffffff)

    def band_edge(v, lo, hi):  # antialiased inside [lo, hi] along the ring's radius (in pixels at its widest)
        return np.clip(np.minimum(v - lo, hi - v) * ra / 1.2, 0, 1)

    def ring(front):
        sel = near if front else ~near
        if outline:
            # (the distance to the edge in pixels: its gap in rr over rr's slope, which is steep where the ring is
            # seen edge-on)
            slope = np.sqrt((ux / ra ** 2) ** 2 + (uy / rb ** 2) ** 2) / np.maximum(rr, 1e-6)
            for edge in (0.76, 0.99):
                m = np.clip(1 - np.abs(rr - edge) / slope / line, 0, 1) * sel
                if not front:
                    m = m * (d > 1)
                cv.over(white, m)
            return
        lit = 1.0 if front else 0.55
        for lo, hi, al, c in BANDS:
            cv.over(_c(c) * lit, band_edge(rr, lo, hi) * sel * al)
        if t > 0:  # a light running round it
            th = (0.35 * t) % (2 * math.pi)
            if (th < math.pi) == front:
                ex, ey = ra * 0.93 * math.cos(th), rb * 0.93 * math.sin(th)
                gx, gy = cx + ex * ct - ey * st, cy + ex * st + ey * ct
                g = np.exp(-((x - gx) ** 2 + (y - gy) ** 2) / (2 * (0.03 * S) ** 2)) * 0.35
                cv.rgb += _c(0xffd890) * g[..., None] * cv.a[..., None]

    def planet():
        if outline:
            cv.over(white, np.clip(1 - np.abs(d - 1) * R / line, 0, 1))
            return
        edge = np.clip((1 - d) * R / 1.2, 0, 1)
        nx, ny = dx / R, dy / R
        nz = np.sqrt(np.clip(1 - nx * nx - ny * ny, 0, 1))
        py, px = -nx * st + ny * ct, nx * ct + ny * st
        lon = np.arctan2(px, nz) + 0.12 * t
        band = 0.5 + 0.5 * np.sin(py * 10 + 0.7 * np.sin(lon * 2 + py * 3))
        fine = (0.5 + 0.5 * np.sin(py * 31 + 0.4 * np.sin(lon * 3))) ** 8
        base = _c(0x2032a8) + (_c(0x6a80ff) - _c(0x2032a8)) * band[..., None]
        base = base + (_c(0xb8c8ff) - base) * (0.35 * fine)[..., None]
        base = base + (_c(0x7a4ad0) - base) * (0.35 * np.abs(py) ** 2)[..., None]
        lam = np.clip(nx * -0.52 + ny * -0.62 + nz * 0.59, 0, 1)
        rim = (1 - nz) ** 3
        col = base * ((0.24 if icon else 0.14) + 0.9 * lam)[..., None]
        col = col + (_c(0x8cc4ff) - col) * (rim * (0.25 + 0.6 * lam))[..., None]
        cv.over(col, edge)

    def piece(cells, colour, th, rot, hidden=None):
        """`hidden`: where the planet covers it (a piece behind the planet, drawn as contours)."""
        ex, ey = ra * 0.93 * math.cos(th), rb * 0.93 * math.sin(th)
        px0, py0 = cx + ex * ct - ey * st, cy + ex * st + ey * ct
        depth = math.sin(th)
        cs = cs0 * (1 + 0.25 * depth)
        c = _c(colour) * (0.7 + 0.15 * (depth + 1))
        cr, sr = math.cos(rot), math.sin(rot)
        for a, b in cells:
            qx = px0 + (a * cr - b * sr) * cs * 1.1
            qy = py0 - 0.25 * cs + (a * sr + b * cr) * cs * 1.1
            lx, ly = (x - qx) * cr + (y - qy) * sr, -(x - qx) * sr + (y - qy) * cr
            inner = np.minimum(cs / 2 - np.abs(lx), cs / 2 - np.abs(ly))
            if outline:
                m = np.clip(1 - np.abs(inner) / (line * 0.8), 0, 1)
                cv.over(white, m if hidden is None else m * (1 - hidden))
            else:
                shade = np.clip(0.5 - (lx * 0.6 + ly * 0.8) / cs, 0, 1)[..., None]
                cv.over(np.clip(c * (0.75 + 0.45 * shade), 0, 255), np.clip(inner / 1.2, 0, 1))

    pieces = [(cells, col, (th + 0.12 * t) % (2 * math.pi), rot + 0.4 * t * (1 if k % 2 else -1)) for k, (cells, col, th, rot) in enumerate(pieces)]
    behind = [p for p in pieces if math.sin(p[2]) < 0]
    front = [p for p in pieces if math.sin(p[2]) >= 0]
    ring(False)
    for p in behind:  # (as contours: only where the planet doesn't cover them)
        piece(*p, hidden=np.clip((1 - d) * R / 1.2, 0, 1) if outline else None)
    planet()
    ring(True)
    for p in front:
        piece(*p)
    im = cv.image(N)
    if word:
        _word(im, N, word, ss=ss)
    return im


def _word(im, N, word, ink=(255, 255, 255, 255), ss=SS):
    """The word letter-spaced, centred under the planet."""
    big = Image.new("RGBA", (N * ss, N * ss), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    font = ImageFont.truetype(FONT, int(0.095 * N * ss))
    sp = 0.05 * N * ss
    ws = [d.textlength(ch, font=font) for ch in word]
    xx = (N * ss - sum(ws) - sp * (len(word) - 1)) / 2
    for ch, w in zip(word, ws):
        d.text((xx, 0.80 * N * ss), ch, font=font, fill=ink, anchor="ls")
        xx += w + sp
    im.alpha_composite(big.resize((N, N), Image.LANCZOS))


def color(N, word="REFALL", t=0.0, ss=SS):
    """The logo, N x N RGBA on a transparent ground (`t`: seconds into its motion)."""
    return _draw(N, word, False, t=t, ss=ss)


def outline(N, t=0.0, ss=SS):
    """Its white contours alone (the planet, the ring's edges, the pieces, the word), as an L image."""
    return _draw(N, "REFALL", True, t=t, ss=ss).getchannel("A")


def icon(N):
    """The window icon: the planet and its ring filling the square."""
    return _draw(N, None, False, icon=True)


def as_arrays(N):
    """For compositing (float32, 0..1): the color logo premultiplied (rgb, alpha) and the outline's alpha."""
    col = np.asarray(color(N), np.float32) / 255.0
    a = col[..., 3]
    return col[..., :3] * a[..., None], a, np.asarray(outline(N), np.float32) / 255.0


def write_icon(png, inc, ico):
    """The window icon: at 256 px (png), embedded at 16 to 128 px (inc, RGBA), and the Windows .exe's icon (ico)."""
    icon(256).save(png)
    big = icon(256)
    big.save(ico, sizes=[(n, n) for n in (16, 24, 32, 48, 64, 128, 256)],
             append_images=[icon(n) for n in (16, 24, 32, 48, 64, 128)])
    with open(inc, "w") as f:
        f.write("// Generated by tools/logo.py --icon (the REFALL planet, as data/icon.png): the window icon at a few sizes,\n"
                "// RGBA.\n#pragma once\n#include <cstdint>\n")
        for n in ICON_SIZES:
            data = np.asarray(icon(n), np.uint8).reshape(-1)
            f.write(f"\nstatic const uint8_t ICON_{n}[{len(data)}] = {{\n")
            for k in range(0, len(data), 32):
                f.write(",".join(str(v) for v in data[k:k + 32]) + ",\n")
            f.write("};\n")
        f.write("\nstruct IconImage { int size; const uint8_t* rgba; };\nstatic const IconImage ICON_IMAGES[] = {" +
                ", ".join(f"{{{n}, ICON_{n}}}" for n in ICON_SIZES) + "};\n")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?")
    ap.add_argument("--size", type=int)
    ap.add_argument("--outline", action="store_true")
    ap.add_argument("--icon", action="store_true", help="write the window icon (data/icon.png, src/icon.inc, data/refall.ico)")
    args = ap.parse_args()
    if args.icon:
        write_icon(os.path.join(ROOT, "data", "icon.png"), os.path.join(ROOT, "src", "icon.inc"),
                   os.path.join(ROOT, "data", "refall.ico"))
    elif args.out:
        (outline(args.size or 1024) if args.outline else color(args.size or 1024)).save(args.out)
    else:
        ap.error("an output: OUT.png or --icon")
