#!/usr/bin/env python3
"""The REFALL logo drawn at any size: a dark disc in a gold ring at the heart of a rosette of 12 glass kites, the
stages' colours in rainbow order round the wheel as on the title menu, the word letter-spaced inside (the window icon,
data/icon.png, is this drawing). Two looks: "color" (the logo) and "outline" (its white contours only, to lay over a
picture).

usage: tools/logo.py OUT.png [--size PX] [--outline]
       tools/logo.py --inc src/logo.inc [--size PX]   (the debrief screens' two images, embedded in the game)
       tools/logo.py --icon                           (the window icon: data/icon.png and src/icon.inc)
"""
import argparse
import math
import os

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, "src", "embed", "JosefinSans-Light.ttf")
# the stages' colours (STAGE_COL, src/menu.cpp)
STAGE_COL = [(72, 164, 254), (250, 150, 24), (174, 90, 253), (144, 222, 54), (232, 84, 242), (39, 226, 147),
             (252, 80, 127), (0, 178, 160), (250, 78, 185), (22, 191, 244), (204, 203, 20), (92, 60, 255),
             (240, 176, 16), (120, 232, 245), (252, 114, 64)]
GOLD = (232, 192, 106)
ICON_SIZES = (16, 32, 48, 64, 128)


def _hue(c):
    mx, d = max(c), max(c) - min(c)
    r, g, b = c
    if d == 0:
        return 0.0
    return ((g - b) / d + 6) % 6 if mx == r else (b - r) / d + 2 if mx == g else (r - g) / d + 4


# the petals as on the title menu (Worlds::kalTitle): the stages' colours sorted by hue, 12 picked evenly round the wheel
_RAINBOW = sorted(STAGE_COL, key=_hue)
PETALS = [_RAINBOW[i * len(_RAINBOW) // 12] for i in range(12)]


def _mix(c, d, k):
    return tuple(a + (b - a) * k for a, b in zip(c, d))


def _tri(acc, pts, cols):
    """A triangle into acc (S x S x 4, float), its colours (RGB, 0..255) interpolated between its corners, opaque."""
    (x0, y0), (x1, y1), (x2, y2) = pts
    xa, xb = int(max(0, min(x0, x1, x2))), int(min(acc.shape[1] - 1, max(x0, x1, x2))) + 1
    ya, yb = int(max(0, min(y0, y1, y2))), int(min(acc.shape[0] - 1, max(y0, y1, y2))) + 1
    y, x = np.mgrid[ya:yb, xa:xb].astype(np.float32) + 0.5
    den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
    w0 = ((y1 - y2) * (x - x2) + (x2 - x1) * (y - y2)) / den
    w1 = ((y2 - y0) * (x - x2) + (x0 - x2) * (y - y2)) / den
    w2 = 1 - w0 - w1
    inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
    c = np.asarray(cols, np.float32)
    rgb = w0[..., None] * c[0] + w1[..., None] * c[1] + w2[..., None] * c[2]
    r = acc[ya:yb, xa:xb]
    r[inside, :3] = rgb[inside]
    r[inside, 3] = 255


def _glass(acc, pts, c):
    """A kite shaded as the menu's (Worlds::kite): darker at the heart, lighter at the tip, its right half a little
    darker than its left."""
    ix, lx, ox, rx = pts
    inner, outer = _mix(c, (0, 0, 0), 0.45), _mix(c, (255, 255, 255), 0.35)
    _tri(acc, [ix, lx, ox], [inner, c, outer])
    _tri(acc, [ix, ox, rx], [inner, outer, _mix(c, (0, 0, 0), 0.15)])


def _kites(S):
    c = S / 2
    r0, r1, h = 0.30 * S, 0.50 * S, 0.21
    for i in range(12):
        a = -math.pi / 2 + i * 2 * math.pi / 12
        m = (r0 + r1) / 2
        yield i, [(c + math.cos(a) * r0, c + math.sin(a) * r0), (c + math.cos(a - h) * m, c + math.sin(a - h) * m),
                  (c + math.cos(a) * r1, c + math.sin(a) * r1), (c + math.cos(a + h) * m, c + math.sin(a + h) * m)]


def _word(d, S, word="REFALL"):
    """The word's font, letters and their x, its baseline: letter-spaced, fitted inside the disc, capitals centred (a
    single letter, the small icons' R, larger: about half the disc, centred on its own ink)."""
    c, R = S / 2, 0.34 * S
    if len(word) == 1:
        f = ImageFont.truetype(FONT, int(S * 0.46))
        bb = d.textbbox((0, 0), word, font=f, anchor="ls")
        return f, [(word, c - (bb[0] + bb[2]) / 2)], c - (bb[1] + bb[3]) / 2
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


def color(N, word="REFALL"):
    """The logo, RGBA, N x N."""
    S = 4 * N
    acc = np.zeros((S, S, 4), np.float32)
    for i, pts in _kites(S):
        _glass(acc, pts, PETALS[i])
    im = Image.fromarray((acc + 0.5).astype(np.uint8), "RGBA")
    d = ImageDraw.Draw(im)
    c, R = S / 2, 0.34 * S
    for _, pts in _kites(S):
        d.line(pts + [pts[0]], fill=(10, 10, 16, 255), width=max(2, S // 110), joint="curve")
    d.ellipse([c - R, c - R, c + R, c + R], fill=(10, 10, 14, 255))
    g = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(g).ellipse([c - R * 0.8, c - R * 0.8, c + R * 0.8, c + R * 0.8], fill=GOLD + (60,))
    im = Image.alpha_composite(im, g.filter(ImageFilter.GaussianBlur(S / 18)))
    d = ImageDraw.Draw(im)
    d.ellipse([c - R, c - R, c + R, c + R], outline=GOLD + (255,), width=max(3, int(S * 0.022)))
    f, letters, y = _word(d, S, word)
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


def write_icon(png, inc):
    """The window icon: the logo at 256 px (png) and embedded at 16 to 128 px (inc, RGBA), the sizes up to 64 with
    an R in place of the word (too small to read)."""
    color(256).save(png)
    with open(inc, "w") as f:
        f.write("// Generated by tools/logo.py --icon (the REFALL logo, as data/icon.png): the window icon at a few sizes,\n"
                "// RGBA, an R in place of the word up to 64 px.\n#pragma once\n#include <cstdint>\n")
        for n in ICON_SIZES:
            data = np.asarray(color(n, "R" if n <= 64 else "REFALL"), np.uint8).reshape(-1)
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
    ap.add_argument("--inc", help="write the debrief screens' embedded images (src/logo.inc)")
    ap.add_argument("--icon", action="store_true", help="write the window icon (data/icon.png and src/icon.inc)")
    args = ap.parse_args()
    if args.icon:
        write_icon(os.path.join(ROOT, "data", "icon.png"), os.path.join(ROOT, "src", "icon.inc"))
    elif args.inc:
        write_inc(args.inc, args.size or 640)
    elif args.out:
        (outline(args.size or 1024) if args.outline else color(args.size or 1024)).save(args.out)
    else:
        ap.error("an output: OUT.png, --inc FILE or --icon")
