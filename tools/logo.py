#!/usr/bin/env python3
"""The REFALL logo drawn at any size: a dark disc in a gold ring at the heart of a rosette of 12 glass kites in the
stage hues, the word letter-spaced inside (the window icon, data/icon.png, is this drawing). Two looks: "color" (the
logo) and "outline" (its white contours only, to lay over a picture).

usage: tools/logo.py OUT.png [--size PX] [--outline]
"""
import argparse
import math
import os

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, "src", "embed", "JosefinSans-Light.ttf")
STAGE = [(110, 200, 255), (150, 130, 255), (255, 120, 200), (255, 170, 90), (120, 230, 170), (90, 170, 255),
         (220, 120, 255), (255, 210, 110), (110, 220, 230), (255, 130, 120), (170, 230, 110), (200, 200, 255)]
GOLD = (232, 192, 106)


def _kites(S):
    c = S / 2
    r0, r1, h = 0.30 * S, 0.50 * S, 0.21
    for i in range(12):
        a = -math.pi / 2 + i * 2 * math.pi / 12
        m = (r0 + r1) / 2
        yield i, [(c + math.cos(a) * r0, c + math.sin(a) * r0), (c + math.cos(a - h) * m, c + math.sin(a - h) * m),
                  (c + math.cos(a) * r1, c + math.sin(a) * r1), (c + math.cos(a + h) * m, c + math.sin(a + h) * m)]


def _word(d, S, word="REFALL"):
    """The word's font, letters and their x, its baseline: letter-spaced, fitted inside the disc, capitals centred."""
    c, R = S / 2, 0.34 * S
    size = int(S * 0.14)
    f = ImageFont.truetype(FONT, size)
    tr = size * 0.18
    ws = [d.textlength(ch, font=f) for ch in word]
    tw = sum(ws) + tr * (len(word) - 1)
    k = min(1.0, 1.6 * R / tw)
    if k < 1:
        size = int(size * k)
        f = ImageFont.truetype(FONT, size)
        tr *= k
        ws = [d.textlength(ch, font=f) for ch in word]
        tw = sum(ws) + tr * (len(word) - 1)
    bb = d.textbbox((0, 0), word, font=f, anchor="ls")
    y = c - (bb[1] + bb[3]) / 2
    xs, x = [], c - tw / 2
    for w in ws:
        xs.append(x)
        x += w + tr
    return f, list(zip(word, xs)), y


def color(N):
    """The logo, RGBA, N x N."""
    S = 4 * N
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    c, R = S / 2, 0.34 * S
    for i, pts in _kites(S):
        d.polygon(pts, fill=STAGE[i] + (255,))
        d.line(pts + [pts[0]], fill=(10, 10, 16, 255), width=max(2, S // 110))
    d.ellipse([c - R, c - R, c + R, c + R], fill=(10, 10, 14, 255))
    g = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(g).ellipse([c - R * 0.8, c - R * 0.8, c + R * 0.8, c + R * 0.8], fill=GOLD + (60,))
    im = Image.alpha_composite(im, g.filter(ImageFilter.GaussianBlur(S / 18)))
    d = ImageDraw.Draw(im)
    d.ellipse([c - R, c - R, c + R, c + R], outline=GOLD + (255,), width=max(3, int(S * 0.022)))
    f, letters, y = _word(d, S)
    for ch, x in letters:
        d.text((x, y), ch, font=f, fill=(255, 255, 255, 255), anchor="ls")
    return im.resize((N, N), Image.LANCZOS)


def outline(N):
    """The logo's contours in white (alpha only matters: L, N x N): the kites, the ring, the letters' edges."""
    S = 4 * N
    lw = max(2, int(S * 0.006))
    m = Image.new("L", (S, S), 0)
    d = ImageDraw.Draw(m)
    c, R = S / 2, 0.34 * S
    for _, pts in _kites(S):
        d.line(pts + [pts[0]], fill=255, width=lw, joint="curve")
    # the disc hides the kites' inner tips, as in the logo
    d.ellipse([c - R, c - R, c + R, c + R], fill=0)
    d.ellipse([c - R, c - R, c + R, c + R], outline=255, width=int(lw * 1.5))
    # the letters hollow: their ink, minus itself shrunk by a line's width
    t = Image.new("L", (S, S), 0)
    td = ImageDraw.Draw(t)
    f, letters, y = _word(td, S)
    for ch, x in letters:
        td.text((x, y), ch, font=f, fill=255, anchor="ls")
    inner = t.filter(ImageFilter.MinFilter(2 * (lw // 2) + 1))
    m = ImageChops.lighter(m, ImageChops.subtract(t, inner))
    return m.resize((N, N), Image.LANCZOS)


def as_arrays(N):
    """For compositing (float32, 0..1): the color logo premultiplied (rgb, alpha) and the outline's alpha."""
    col = np.asarray(color(N), np.float32) / 255.0
    a = col[..., 3]
    return col[..., :3] * a[..., None], a, np.asarray(outline(N), np.float32) / 255.0


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out")
    ap.add_argument("--size", type=int, default=1024)
    ap.add_argument("--outline", action="store_true")
    args = ap.parse_args()
    (outline(args.size) if args.outline else color(args.size)).save(args.out)
