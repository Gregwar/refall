#!/usr/bin/env python3
"""The REFALL logo drawn at any size: a dark disc in a gold ring at the heart of a rosette of 12 glass kites in the
stage hues, the word letter-spaced inside (the window icon, data/icon.png, is this drawing). Two looks: "color" (the
logo) and "outline" (its white contours only, to lay over a picture).

usage: tools/logo.py OUT.png [--size PX] [--outline]
       tools/logo.py --inc src/logo.inc [--size PX]   (the debrief screens' two images, embedded in the game)
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


def _packbits(b):
    """Bytes run-length coded: a byte h then, h < 128, h + 1 literal bytes; h >= 128, one byte repeated h - 126 times."""
    b = np.asarray(b, np.uint8).reshape(-1)
    out, i, n = bytearray(), 0, len(b)
    while i < n:
        j = i
        while j + 1 < n and b[j + 1] == b[i] and j + 1 - i < 129:
            j += 1
        if j > i:
            out += bytes((128 + (j - i + 1) - 2, int(b[i])))
            i = j + 1
        else:
            s = i
            while i < n and i - s < 128 and not (i + 1 < n and b[i + 1] == b[i]):
                i += 1
            out.append(i - s - 1)
            out += b[s:i].tobytes()
    return bytes(out)


def write_inc(path, N):
    """The debrief screens' images as C arrays (src/logo.inc): the colour logo (premultiplied RGBA, its 4 planes one
    after the other) and the contours' alpha (one plane: the game adds their glow), N x N, run-length coded."""
    col = np.asarray(color(N), np.float32) / 255.0
    col[..., :3] *= col[..., 3:]
    col = (col * 255 + 0.5).astype(np.uint8)
    parts = [("LOGO_COLOR_RLE", b"".join(_packbits(col[..., c]) for c in range(4))),
             ("LOGO_CONTOURS_RLE", _packbits(np.asarray(outline(N), np.uint8)))]
    with open(path, "w") as f:
        f.write(f"// Generated by tools/logo.py --inc src/logo.inc --size {N}: the REFALL logo for the debrief screens. The\n"
                "// colour logo (premultiplied RGBA, plane after plane) and its white contours (alpha), N x N, run-length coded:\n"
                "// a byte h, then h + 1 literal bytes when h < 128, else one byte repeated h - 126 times.\n"
                f"#pragma once\n#include <cstdint>\n\nstatic const int LOGO_SIZE = {N};\n")
        for name, data in parts:
            f.write(f"\nstatic const uint8_t {name}[{len(data)}] = {{\n")
            for k in range(0, len(data), 40):
                f.write(",".join(str(v) for v in data[k:k + 40]) + ",\n")
            f.write("};\n")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?")
    ap.add_argument("--size", type=int)
    ap.add_argument("--outline", action="store_true")
    ap.add_argument("--inc", help="write the debrief screens' embedded images (src/logo.inc)")
    args = ap.parse_args()
    if args.inc:
        write_inc(args.inc, args.size or 640)
    elif args.out:
        (outline(args.size or 1024) if args.outline else color(args.size or 1024)).save(args.out)
    else:
        ap.error("an output: OUT.png or --inc FILE")
