#!/usr/bin/env python3
"""Demo video: the game played in many universes over one whole song, in chapters with title cards on black.

usage: tools/demo.py [CONFIG] [-o OUT.mp4] [--plan-only] [-j N] [--game PATH] [--work DIR] [--size WxH] [--fps N]
                     [--seed N] [--max-mb MB | --crf N] [--only U,U] [--sheet PNG] [--song SPEC]

CONFIG is tools/demo.json by default: the song, the chapters (a title card each, an end anchored on the song's
structure) and their shots (universe, variant, game test hooks, length). Its header documents the format.
The pipeline, one function each:
  1. song + analysis: the song (a file or ytdl:<id>, from the game's YouTube cache or yt-dlp) and the game's own
     analysis of it (tetris-universe --analysis: beats, downbeat, sections, drops), so cuts land on the beats the
     universes react to;
  2. plan: the config turned into a timeline (chapter ends snapped to downbeats, shot lengths cut on the grid,
     universes no chapter names added to the "fill" chapter); --plan-only prints it, with the coverage of the
     enabled universes (the build's registry_enabled.def, else registry.def minus CMake's default disabled list;
     weight-0 universes such as DEBUG are left out);
  3. record: every shot is the real game (--hidden --clip, autoplay at the clip's calm pace) in its universe at
     that very moment of the song, so the music-reactive visuals match the audio under them; shots run in
     parallel (-j) and are cached in WORK/clips by everything that defines them: a re-run re-records only the
     shots that changed;
  4. cards: titles drawn with Pillow from the fonts shipped in the repo (a soft wipe in, a slow fade out);
  5. assemble: frame-exact segments (the whole song uncut is the audio track: video time = song time), then a
     two-pass x264 encode sized to --max-mb (or --crf), AAC audio; a contact sheet of the shots next to it.

Game options used (added for this tool): `--analysis OUT.json` writes the song analysis and exits; ZEN_CLIP_FILL=1
starts a clip's warm-up on a played-in stack (a shot early in the song has no time to fill the board). Other hooks
(ZEN_WARMUP, ZEN_SCENE_LINES, ZEN_<UNIVERSE>_LINES, ZEN_GROW_RATE...) come from the config's env.
Needs a built tetris-universe (default build/tetris-universe), ffmpeg, numpy and Pillow; yt-dlp for a song not
cached yet (ZENTRIS_YTDLP, PATH or ~/.local/bin).
"""
import argparse
import concurrent.futures
import hashlib
import json
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLIP_FPS = 60  # the game records clips at 60 fps


def log(*a):
    print(*a, flush=True)


def load_config(path):
    """JSON with whole-line // comments."""
    with open(path) as f:
        text = "\n".join(line for line in f.read().splitlines() if not line.lstrip().startswith("//"))
    return json.loads(text)


def short_hash(*parts):
    return hashlib.sha1(json.dumps(parts, sort_keys=True).encode()).hexdigest()[:16]


# ---------------------------------------------------------------------------------------------------------------
# 1. The song and its analysis

def find_ytdlp():
    for p in (os.environ.get("ZENTRIS_YTDLP"), shutil.which("yt-dlp"), os.path.expanduser("~/.local/bin/yt-dlp")):
        if p and os.path.exists(p):
            return p
    return None


def resolve_song(spec, title, work):
    """The song as a file whose name is its title (the game letters the file name as the song's title)."""
    cache = os.path.join(os.environ.get("XDG_CACHE_HOME", os.path.expanduser("~/.cache")), "zentris", "youtube")
    if spec.startswith("ytdl:"):
        vid = spec[5:]
        path = os.path.join(cache, vid + ".mp3")
        if not os.path.exists(path):
            tool = find_ytdlp()
            if not tool:
                sys.exit(f"{spec} is not cached and yt-dlp is missing (set ZENTRIS_YTDLP)")
            log(f"downloading {spec} ...")
            subprocess.run([tool, "-x", "--audio-format", "mp3", "-o", os.path.join(cache, vid + ".%(ext)s"),
                            f"https://www.youtube.com/watch?v={vid}"], check=True)
        if not title:
            for tsv in sorted(os.listdir(os.path.join(cache, "lists"))) if os.path.isdir(os.path.join(cache, "lists")) else []:
                with open(os.path.join(cache, "lists", tsv)) as f:
                    for line in f:
                        parts = line.rstrip("\n").split("\t")
                        if parts[0] == vid and len(parts) > 1:
                            title = parts[1]
    else:
        path = os.path.abspath(spec)
        if not os.path.exists(path):
            sys.exit(f"song not found: {spec}")
    if not title:
        return path
    safe = re.sub(r'[/\\:*?"<>|]', "-", title)
    link_dir = os.path.join(work, "song")
    os.makedirs(link_dir, exist_ok=True)
    link = os.path.join(link_dir, safe + os.path.splitext(path)[1])
    if os.path.islink(link) or os.path.exists(link):
        os.remove(link)
    os.symlink(path, link)
    return link


def file_key(path):
    st = os.stat(path)
    return [os.path.realpath(path), st.st_size, int(st.st_mtime)]


def analyze(game, song, work):
    """The game's analysis of the song (cached)."""
    out = os.path.join(work, "analysis-" + short_hash(file_key(song), file_key(game)) + ".json")
    if not os.path.exists(out):
        r = subprocess.run([game, "--hidden", "--seed", "1", "--analysis", out, song], capture_output=True, text=True,
                           timeout=600)
        if not os.path.exists(out):
            sys.exit("the game could not analyze the song:\n" + r.stdout[-2000:] + r.stderr[-2000:])
    with open(out) as f:
        return json.load(f)


class Grid:
    """The song's beat grid, from the game's analysis, extended to cover the whole song."""

    def __init__(self, an):
        beats = list(an["beats"])
        period = float(np.median(np.diff(beats)))
        while beats[0] - period > 0:
            beats.insert(0, beats[0] - period)
        while beats[-1] < an["duration"] + 2 * period:
            beats.append(beats[-1] + period)
        self.beats = np.array(beats)
        # the downbeat phase, re-indexed after the beats added before the first one
        added = beats.index(an["beats"][0])
        self.down = (an["downbeat"] + added) % 4
        self.bars = self.beats[self.down::4]
        self.period = period
        self.duration = an["duration"]
        self.an = an

    def nearest(self, t, unit):
        g = self.bars if unit == "bar" else self.beats
        return int(np.argmin(np.abs(g - t)))

    def shift(self, t, n, unit):
        if unit == "s":
            return t + n
        g = self.bars if unit == "bar" else self.beats
        i = self.nearest(t, unit) + int(round(n))
        if i < 0:
            return float(g[0] + i * self.period * (4 if unit == "bar" else 1))
        return float(g[min(i, len(g) - 1)])

    def bar_of(self, t):
        return float(np.interp(t, self.bars, np.arange(len(self.bars))))

    def landmark(self, term):
        an = self.an
        if term == "start":
            return 0.0
        if term == "end":
            return float(an["duration"])
        m = re.fullmatch(r"([\d.]+)s", term)
        if m:
            return float(m.group(1))
        m = re.fullmatch(r"bar:(-?\d+)", term)
        if m:
            return self.shift(self.bars[0], int(m.group(1)), "bar")
        m = re.fullmatch(r"drop(?::(\d+))?", term)
        if m:
            n = int(m.group(1) or 1)
            drops = [s["start"] for s in an["segments"] if s["kind"] == "DROP"]
            if len(drops) < n:  # a song without enough marked drops: its peaks
                drops = sorted(set(drops) | {p["start"] for p in an["phases"] if p["level"] == 2})
            if len(drops) < n:
                drops.append(an["first_drop"])
            return float(drops[min(n, len(drops)) - 1])
        m = re.fullmatch(r"section:([A-Z]+)(?::(\d+))?", term.upper().replace("SECTION", "section"))
        if m:
            kind, n = m.group(1), int(m.group(2) or 1)
            starts = [s["start"] for s in an["segments"] if s["kind"] == kind]
            if not starts:
                sys.exit(f"anchor {term}: the song has no {kind} (sections: " +
                         " ".join(f"{s['kind']}@{s['start']:.0f}" for s in an["segments"]) + ")")
            return float(starts[min(n, len(starts)) - 1])
        sys.exit(f"unknown anchor '{term}'")

    def anchor(self, expr, snap="bar"):
        """'drop:1 - 2bars' -> song seconds."""
        parts = re.split(r"\s+([+-])\s+", expr.strip())
        t = self.landmark(parts[0].strip())
        snapped = parts[0].strip() in ("start", "end")
        for k in range(1, len(parts), 2):
            sign = 1 if parts[k] == "+" else -1
            m = re.fullmatch(r"([\d.]+)\s*(bars?|beats?|s)", parts[k + 1].strip())
            if not m:
                sys.exit(f"bad offset '{parts[k + 1]}' in '{expr}'")
            unit = {"bar": "bar", "bars": "bar", "beat": "beat", "beats": "beat", "s": "s"}[m.group(2)]
            t = self.shift(t, sign * float(m.group(1)), unit)
        if snap and not snapped:
            g = self.bars if snap == "bar" else self.beats
            t = float(g[self.nearest(t, snap)])
        return max(0.0, min(t, self.duration))

    def length(self, spec, at):
        """'4bars' / '6beats' / '2.5s' from time `at` -> seconds."""
        m = re.fullmatch(r"([\d.]+)\s*(bars?|beats?|s)", spec)
        if not m:
            sys.exit(f"bad length '{spec}'")
        unit = {"bar": "bar", "bars": "bar", "beat": "beat", "beats": "beat", "s": "s"}[m.group(2)]
        return self.shift(at, float(m.group(1)), unit) - at


# ---------------------------------------------------------------------------------------------------------------
# 2. The plan

def enabled_universes(game):
    """Registry ids of the universes the build has, in registry order, without weight-0 ones (DEBUG)."""
    gen = os.path.join(os.path.dirname(os.path.abspath(game)), "generated", "universes", "registry_enabled.def")
    if os.path.exists(gen):
        path, disabled = gen, set()
    else:
        path = os.path.join(ROOT, "src", "universes", "registry.def")
        with open(os.path.join(ROOT, "CMakeLists.txt")) as f:
            m = re.search(r'set\(ZEN_DEFAULT_DISABLED "([^"]*)"\)', f.read())
        disabled = set(m.group(1).split(";")) if m else set()
    out = []
    with open(path) as f:
        for line in f:
            m = re.match(r"UNIVERSE\((\w+),\s*([\d.]+)\)", line)
            if m and m.group(1) not in disabled and float(m.group(2)) > 0:
                out.append(m.group(1))
    return out


def cut_lengths(points, n, accelerate, fixed):
    """Splits grid points (times) among n shots: fixed[i] grid units, or a share (weights, ramped by `accelerate`:
    the first shot that many times longer than the last). Returns the shots' boundary indices into points."""
    units = len(points) - 1
    free = [i for i in range(n) if fixed[i] is None]
    left = units - sum(f for f in fixed if f is not None)
    if left < len(free):
        return None
    if not free:
        lengths = list(fixed)
    else:
        # ideal shares, ramped from `accelerate` (the first) to 1 (the last), at least one unit each; whole units by
        # largest remainder, then handed out in the ramp's order so the lengths only shrink (or only grow)
        w = [accelerate ** ((len(free) - 1 - k) / max(1, len(free) - 1)) for k in range(len(free))]
        ideal = [max(1.0, left * x / sum(w)) for x in w]
        scale = left / sum(ideal)
        ideal = [x * scale for x in ideal]
        whole = [max(1, int(x)) for x in ideal]
        order = sorted(range(len(free)), key=lambda k: -(ideal[k] - int(ideal[k])))
        k = 0
        while sum(whole) < left:
            whole[order[k % len(order)]] += 1
            k += 1
        while sum(whole) > left:  # the floors at 1 overshot: take from the longest
            whole[max(range(len(whole)), key=lambda j: whole[j])] -= 1
        whole.sort(reverse=accelerate >= 1)
        lengths, it = [], iter(whole)
        for i in range(n):
            lengths.append(fixed[i] if fixed[i] is not None else next(it))
    if sum(lengths) != units or min(lengths) < 1:
        return None
    bounds = [0]
    for x in lengths:
        bounds.append(bounds[-1] + x)
    return bounds


def plan(cfg, grid, universes):
    """The timeline: cards and shots with their song times, from the config."""
    seed = int(cfg.get("seed", 1))
    named = {s["u"].split(":")[0] for ch in cfg["chapters"] for s in ch.get("shots", [])}
    for u in sorted(named - set(universes)):
        log(f"note: {u} is named in the config but not enabled in this build: skipped")
    timeline, t = [], 0.0
    for ci, ch in enumerate(cfg["chapters"]):
        end = grid.duration if ci == len(cfg["chapters"]) - 1 else grid.anchor(ch["end"])
        if end <= t:
            sys.exit(f"chapter {ch['id']} ends at {end:.2f} s, before it starts ({t:.2f} s)")
        card = ch.get("card")
        shots_at = t
        if card:
            card_end = end
            if "bars" in card:
                card_end = min(end, grid.shift(t, card["bars"], "bar"))
            if "len" in card:
                card_end = min(end, t + grid.length(card["len"], t))
            timeline.append({"kind": "card", "chapter": ch["id"], "t0": t, "t1": card_end, **card})
            shots_at = card_end
        shots = [dict(s) for s in ch.get("shots", []) if s["u"].split(":")[0] in universes]
        if ch.get("fill"):
            order = [u for u in ch.get("fill_order", []) if u in universes]
            order += [u for u in universes if u not in order]
            shots += [{"u": u} for u in order if u not in named]
            named |= set(order)
        if not shots:
            t = end
            continue
        if shots_at >= end:
            sys.exit(f"chapter {ch['id']}: no time left for its shots after the card")
        # runs of shots between pinned starts
        pins = [i for i, s in enumerate(shots) if "start" in s]
        cuts = [0] + pins + [len(shots)]
        starts = {i: grid.anchor(shots[i]["start"]) for i in pins}
        grid_unit = ch.get("grid", "beat")
        for a, b in zip(cuts, cuts[1:]):
            if a == b:
                continue
            r0 = starts.get(a, shots_at)
            r1 = starts.get(b, end)
            run = shots[a:b]
            for unit in ([grid_unit, "beat"] if grid_unit != "beat" else ["beat"]):
                g = grid.bars if unit == "bar" else grid.beats
                inner = [float(x) for x in g if r0 + 0.05 < x < r1 - 0.05]
                points = [r0] + inner + [r1]
                fixed = []
                for s in run:
                    if "len" in s:
                        d = grid.length(s["len"], r0)
                        fixed.append(max(1, int(round(d / (grid.period * (4 if unit == "bar" else 1))))))
                    else:
                        fixed.append(None)
                if all(f is not None for f in fixed):  # all fixed: the last one takes what is left
                    fixed[-1] = None
                bounds = cut_lengths(points, len(run), float(ch.get("accelerate", 1.0)), fixed)
                if bounds:
                    break
            if not bounds:
                sys.exit(f"chapter {ch['id']}: {len(run)} shots do not fit in {r1 - r0:.1f} s on a beat grid")
            for i, s in enumerate(run):
                u, _, variant = s["u"].partition(":")
                env = dict(cfg.get("env", {}))
                env.update(cfg.get("universe_env", {}).get(u, {}))
                env.update(s.get("env", {}))
                timeline.append({"kind": "shot", "chapter": ch["id"], "u": u, "variant": variant,
                                 "t0": points[bounds[i]], "t1": points[bounds[i + 1]], "env": env,
                                 "seed": int(s.get("seed", int(short_hash(seed, ch["id"], u)[:8], 16) | 1))})
        t = end
        if ch.get("outro") and timeline[-1]["kind"] == "shot":
            o = ch["outro"]
            last = timeline[-1]
            if "fade" in o:
                last["fade_out"] = grid.duration - grid.shift(grid.duration, -float(re.match(r"[\d.]+", o["fade"]).group()),
                                                              "bar" if "bar" in o["fade"] else "beat")
            if "title" in o:
                title = dict(o["title"])
                title["at"] = grid.anchor(title.get("at", "end - 4bars"))
                last["title"] = title
    missing = [u for u in universes if u not in {e.get("u") for e in timeline}]
    return timeline, missing


def print_plan(timeline, grid, universes, missing):
    log(f"{'#':>3} {'chapter':<9} {'what':<28} {'start':>7} {'end':>7} {'len':>6} {'bar':>6}  env")
    k = 0
    for e in timeline:
        what = ("card: " + e["text"]) if e["kind"] == "card" else (e["u"] + (":" + e["variant"] if e["variant"] else ""))
        extra = "" if e["kind"] == "card" else " ".join(f"{a}={b}" for a, b in e["env"].items()
                                                         if a not in ("ZEN_SCENE_LINES", "ZEN_CLIP_FILL"))
        if e.get("fade_out"):
            extra += f"  [fade out {e['fade_out']:.1f} s, title at {e['title']['at']:.2f}]" if e.get("title") else ""
        log(f"{k:>3} {e['chapter']:<9} {what[:28]:<28} {e['t0']:7.2f} {e['t1']:7.2f} {e['t1'] - e['t0']:6.2f} "
            f"{grid.bar_of(e['t0']):6.2f}  {extra}")
        k += 1
    shown = [e["u"] for e in timeline if e["kind"] == "shot"]
    log(f"\ncoverage: {len(set(shown))}/{len(universes)} enabled universes in {len(shown)} shots"
        + (f"; MISSING: {', '.join(missing)}" if missing else " (all)"))
    log("  " + " ".join(sorted(set(shown))))


# ---------------------------------------------------------------------------------------------------------------
# 3. Recording

def shot_key(e, game, song, size):
    return short_hash(file_key(game), file_key(song), size, e["u"], e["variant"], e["env"], e["seed"],
                      round(e["t0"] * CLIP_FPS), round((e["t1"] - e["t0"]) * CLIP_FPS))


def record_shot(e, game, song, size, clips):
    """One shot: the game in its universe at that moment of the song (a cached clip)."""
    key = shot_key(e, game, song, size)
    out = os.path.join(clips, key + ".mp4")
    e["clip"] = out
    if os.path.exists(out):
        if os.path.exists(out + ".log"):
            with open(out + ".log") as f:
                label = re.search(r"\[clip\] wrote .* \((.*)\)", f.read())
            if label:
                e["label"] = label.group(1)
        return e, True, "cached"
    start = round(e["t0"] * CLIP_FPS) / CLIP_FPS  # on the game's frame grid
    dur = e["t1"] - e["t0"] + 0.25
    spec = e["u"] + (":" + e["variant"] if e["variant"] else "")
    tmp = out + ".part.mp4"
    cmd = [game, "--hidden", "--seed", str(e["seed"]), "--size", size, "--universe", spec, "--clip", tmp,
           "--clip-at", f"{start:.4f}", "--clip-before", "0", "--clip-after", f"{dur:.3f}", song]
    env = dict(os.environ)
    env.update({k: str(v) for k, v in e["env"].items()})
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=900, env=env)
        text = r.stdout + r.stderr
    except subprocess.TimeoutExpired:
        return e, False, "timeout"
    with open(out + ".log", "w") as f:
        f.write(" ".join(cmd) + "\n" + text)
    if "[clip] wrote" in text and os.path.exists(tmp):
        os.replace(tmp, out)
        label = re.search(r"\[clip\] wrote .* \((.*)\)", text)
        e["label"] = label.group(1) if label else spec
        return e, True, e["label"]
    return e, False, text[-400:]


def record_all(timeline, game, song, size, work, jobs):
    clips = os.path.join(work, "clips")
    os.makedirs(clips, exist_ok=True)
    shots = [e for e in timeline if e["kind"] == "shot"]
    todo = [e for e in shots if not os.path.exists(os.path.join(clips, shot_key(e, game, song, size) + ".mp4"))]
    log(f"recording {len(todo)} of {len(shots)} shots ({len(shots) - len(todo)} cached), {jobs} at a time")
    failed = []
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        for k, (e, ok, msg) in enumerate(ex.map(lambda s: record_shot(s, game, song, size, clips), shots)):
            if not ok:
                failed.append(e)
                log(f"  FAILED {e['u']} at {e['t0']:.2f}: {msg}")
            elif msg != "cached":
                log(f"  [{k + 1}/{len(shots)}] {e['u']:<12} {e['t0']:7.2f} s  {msg}")
    if failed:
        sys.exit(f"{len(failed)} shots failed (logs in {clips})")


# ---------------------------------------------------------------------------------------------------------------
# 4. Title cards

INK = (244, 238, 228)


def smooth(a, b, x):
    if b == a:
        return 1.0 if x >= b else 0.0
    t = min(1.0, max(0.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


class Card:
    """A line of text drawn once (premultiplied, with a soft glow), animated per frame: a soft wipe from the left
    with a fade in, a hairline that opens from the middle, a slow fade out."""

    def __init__(self, text, font_path, cap_px, width, height, tracking=0.02, hairline=False, y=0.5):
        font = ImageFont.truetype(font_path, int(cap_px * 1.45))
        # letter by letter, with tracking
        advs = [font.getlength(c) for c in text]
        track = tracking * cap_px
        tw = int(sum(advs) + track * (len(text) - 1))
        asc, desc = font.getmetrics()
        pad = int(cap_px * 0.8)
        im = Image.new("L", (tw + 2 * pad, asc + desc + 2 * pad), 0)
        d = ImageDraw.Draw(im)
        x = pad
        for c, a in zip(text, advs):
            d.text((x, pad), c, font=font, fill=255)
            x += a + track
        bbox = im.getbbox() or (0, 0, im.width, im.height)
        glow = im.filter(ImageFilter.GaussianBlur(cap_px * 0.35))
        a = np.asarray(im, np.float32) / 255.0
        g = np.asarray(glow, np.float32) / 255.0
        alpha = np.clip(a + 0.35 * g, 0, 1)
        self.alpha = alpha
        self.ink = np.array(INK, np.float32) / 255.0
        cy = int(height * y)
        # the glyph box's vertical middle on cy
        mid = (bbox[1] + bbox[3]) / 2
        self.x0 = int(width / 2 - im.width / 2)
        self.y0 = int(cy - mid)
        self.text_x = (pad + self.x0, pad + self.x0 + tw)
        self.hairline = hairline
        self.hair_y = int(cy + (bbox[3] - bbox[1]) * 0.5 + cap_px * 0.55)
        self.hair_w = tw * 0.42
        self.width, self.height = width, height

    def draw(self, frame, t, t_in, t_out, wipe=0.9, fade=0.7):
        """Composites onto frame (float32 HxWx3, 0..1) at card time t; [t_in, t_out]: shown."""
        if t < t_in or t > t_out:
            return
        out_k = 1.0 - smooth(t_out - fade, t_out, t)
        h, w = self.alpha.shape
        x0, y0 = self.x0, self.y0
        sx0, sy0 = max(0, -x0), max(0, -y0)
        fx0, fy0 = max(0, x0), max(0, y0)
        fx1, fy1 = min(self.width, x0 + w), min(self.height, y0 + h)
        if fx1 <= fx0 or fy1 <= fy0:
            return
        a = self.alpha[sy0:sy0 + fy1 - fy0, sx0:sx0 + fx1 - fx0]
        # the wipe: a soft edge travelling across the text
        p = smooth(t_in, t_in + wipe, t)
        lx0, lx1 = self.text_x
        soft = 0.35 * (lx1 - lx0)
        edge = lx0 - soft + p * (lx1 - lx0 + 2 * soft)
        xs = np.arange(fx0, fx1, dtype=np.float32)
        ramp = np.clip((edge - xs) / soft, 0, 1)
        ramp = ramp * ramp * (3 - 2 * ramp)
        k = a * ramp[None, :] * out_k * smooth(t_in, t_in + wipe * 0.6, t)
        region = frame[fy0:fy1, fx0:fx1]
        region *= (1 - k)[..., None]
        region += k[..., None] * self.ink
        if self.hairline:
            hk = smooth(t_in + wipe * 0.4, t_in + wipe * 1.6, t)
            half = self.hair_w * hk
            cx = self.width / 2
            if half > 1:
                xa, xb = int(cx - half), int(cx + half)
                xs = np.arange(xa, xb, dtype=np.float32)
                fall = 1 - np.abs(xs - cx) / half  # thinning toward the ends
                hk2 = 0.55 * out_k * np.clip(fall * 1.6, 0, 1)
                row = frame[self.hair_y:self.hair_y + max(1, int(self.height / 1080)), xa:xb]
                row *= (1 - hk2)[None, :, None]
                row += hk2[None, :, None] * self.ink


def make_card(spec, cfg, width, height):
    style = spec.get("style", "chapter")
    font = os.path.join(ROOT, cfg.get("fonts", {}).get("title", "src/embed/CormorantGaramond-Italic.ttf"))
    if style == "title":
        return Card(spec["text"], font, height * 0.085, width, height, tracking=0.015, hairline=True, y=0.47)
    return Card(spec["text"], font, height * 0.052, width, height, tracking=0.02, hairline=False, y=0.5)


# ---------------------------------------------------------------------------------------------------------------
# 5. Assembly

def read_frames(clip, first, step, n, width, height):
    """n frames of a clip at 60 fps: indices first, first + step, ..."""
    vf = f"select='gte(n\\,{first})*not(mod(n-{first}\\,{step}))',scale={width}:{height}"
    p = subprocess.Popen(["ffmpeg", "-v", "error", "-i", clip, "-vf", vf, "-vsync", "0", "-f", "rawvideo",
                          "-pix_fmt", "rgb24", "-"], stdout=subprocess.PIPE)
    size = width * height * 3
    last = None
    for _ in range(n):
        buf = p.stdout.read(size)
        if len(buf) < size:
            buf = last if last is not None else bytes(size)  # a clip a frame short: hold its last frame
        last = buf
        yield buf
    p.stdout.close()
    p.kill()
    p.wait()


def assemble(timeline, cfg, grid, song, out, width, height, fps, work, max_mb, crf):
    duration = grid.duration
    total = int(round(duration * fps))
    inter = os.path.join(work, "assembled.mkv")
    enc = subprocess.Popen(["ffmpeg", "-y", "-v", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{width}x{height}",
                            "-r", str(fps), "-i", "-", "-c:v", "libx264", "-preset", "veryfast", "-crf", "8",
                            "-pix_fmt", "yuv444p", inter], stdin=subprocess.PIPE)
    black = np.zeros((height, width, 3), np.float32)
    cards = {}
    step = CLIP_FPS // fps if CLIP_FPS % fps == 0 else None
    if step is None:
        sys.exit("--fps must divide 60 (30 or 60)")
    written = 0
    for e in timeline:
        f0, f1 = int(round(e["t0"] * fps)), int(round(e["t1"] * fps))
        f1 = min(f1, total)
        if f1 <= f0:
            continue
        if f0 != written:
            sys.exit(f"timeline gap at frame {written} ({e})")
        if e["kind"] == "card":
            card = make_card(e, cfg, width, height)
            t_in = e["t0"] + 0.15 if e.get("style") == "title" else e["t0"] + 0.1
            if e.get("style") == "title" and e["t0"] < 0.5:
                t_in = e["t0"] + 0.6
            t_out = e["t1"] - 0.12
            for f in range(f0, f1):
                fr = black.copy()
                card.draw(fr, f / fps, t_in, t_out, wipe=1.8 if e.get("style") == "title" else 0.8,
                          fade=1.0 if e.get("style") == "title" else 0.5)
                enc.stdin.write((np.clip(fr, 0, 1) * 255 + 0.5).astype(np.uint8).tobytes())
        else:
            start = round(e["t0"] * CLIP_FPS) / CLIP_FPS
            first = max(0, int(round((f0 / fps - start) * CLIP_FPS)))
            title = make_card(e["title"], cfg, width, height) if e.get("title") else None
            fade_from = grid.duration - e["fade_out"] if e.get("fade_out") else None
            for k, buf in enumerate(read_frames(e["clip"], first, step, f1 - f0, width, height)):
                t = (f0 + k) / fps
                if title is None and fade_from is None:
                    enc.stdin.write(buf)
                    continue
                fr = np.frombuffer(buf, np.uint8).reshape(height, width, 3).astype(np.float32) / 255.0
                if fade_from is not None and t > fade_from:
                    x = min(1.0, (t - fade_from) / (grid.duration - 0.3 - fade_from))
                    fr *= (1 - x) ** 1.6
                if title is not None:
                    title.draw(fr, t, e["title"]["at"], grid.duration - 0.6, wipe=1.8, fade=2.5)
                enc.stdin.write((np.clip(fr, 0, 1) * 255 + 0.5).astype(np.uint8).tobytes())
        written = f1
        log(f"  {written / fps:7.2f} s  {e.get('text') or e.get('label') or e['u']}")
    for _ in range(written, total):
        enc.stdin.write((black * 255).astype(np.uint8).tobytes())
    enc.stdin.close()
    enc.wait()
    encode_final(inter, song, out, duration, max_mb, crf, work)


def encode_final(inter, song, out, duration, max_mb, crf, work):
    audio_k = 160
    common = ["-i", inter, "-i", song, "-map", "0:v", "-map", "1:a", "-t", f"{duration:.3f}", "-pix_fmt", "yuv420p",
              "-c:v", "libx264", "-preset", "slow", "-tune", "film", "-movflags", "+faststart"]
    if crf is not None:
        subprocess.run(["ffmpeg", "-y", "-v", "error", *common, "-crf", str(crf), "-c:a", "aac", "-b:a", f"{audio_k}k",
                        out], check=True)
    else:
        video_k = int(max_mb * 8e6 / duration / 1000 * 0.97) - audio_k
        log(f"two-pass encode at {video_k} kb/s video + {audio_k} kb/s audio ({max_mb} MB)")
        passlog = os.path.join(work, "x264pass")
        subprocess.run(["ffmpeg", "-y", "-v", "error", *common, "-b:v", f"{video_k}k", "-pass", "1",
                        "-passlogfile", passlog, "-an", "-f", "mp4", os.devnull], check=True)
        subprocess.run(["ffmpeg", "-y", "-v", "error", *common, "-b:v", f"{video_k}k", "-pass", "2",
                        "-passlogfile", passlog, "-c:a", "aac", "-b:a", f"{audio_k}k", out], check=True)
    log(f"wrote {out} ({os.path.getsize(out) / 1e6:.1f} MB, {duration:.1f} s)")


def contact_sheet(timeline, out_png, path_video, fps):
    """A tile per shot (its middle frame from the final video), labelled with the time and universe."""
    shots = [e for e in timeline if e["kind"] in ("shot", "card")]
    tw, th, cols = 384, 216, 8
    rows = (len(shots) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * tw, rows * (th + 22)), (12, 12, 14))
    font = ImageFont.truetype(os.path.join(ROOT, "src", "embed", "TenorSans-Regular.ttf"), 14)
    d = ImageDraw.Draw(sheet)
    with tempfile.TemporaryDirectory() as tmp:
        for k, e in enumerate(shots):
            mid = (e["t0"] + e["t1"]) / 2 if e["kind"] == "shot" else e["t0"] + 0.75 * (e["t1"] - e["t0"])
            png = os.path.join(tmp, f"{k}.png")
            subprocess.run(["ffmpeg", "-v", "error", "-ss", f"{mid:.3f}", "-i", path_video, "-frames:v", "1",
                            "-vf", f"scale={tw}:{th}", png], check=True)
            x, y = (k % cols) * tw, (k // cols) * (th + 22)
            if os.path.exists(png):
                sheet.paste(Image.open(png), (x, y))
            name = e.get("label") or e.get("u") or e["text"]
            d.text((x + 6, y + th + 3), f"{e['t0']:6.2f}  {name}", font=font, fill=(220, 215, 205))
    sheet.save(out_png)
    log(f"wrote {out_png}")


# ---------------------------------------------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("config", nargs="?", default=os.path.join(ROOT, "tools", "demo.json"))
    ap.add_argument("-o", "--out", default="demo.mp4")
    ap.add_argument("--game", default=os.path.join(ROOT, "build", "tetris-universe"))
    ap.add_argument("--work", default=os.path.join(os.environ.get("XDG_CACHE_HOME", os.path.expanduser("~/.cache")),
                                                   "zentris", "demo"), help="clip cache and intermediates")
    ap.add_argument("--song", help="override the config's song (a file or ytdl:<id>)")
    ap.add_argument("--size", help="WxH (config: 1920x1080)")
    ap.add_argument("--fps", type=int, help="30 or 60 (config: 30)")
    ap.add_argument("--seed", type=int)
    ap.add_argument("--max-mb", type=float, help="two-pass encode to this size (config: max_mb)")
    ap.add_argument("--crf", type=int, help="constant quality instead of a size target")
    ap.add_argument("-j", "--jobs", type=int, default=6, help="shots recorded at once")
    ap.add_argument("--plan-only", action="store_true", help="print the timeline and the coverage, then exit")
    ap.add_argument("--only", help="record only these universes (comma separated), for tests")
    ap.add_argument("--sheet", help="contact sheet PNG (default: next to the video)")
    args = ap.parse_args()

    cfg = load_config(args.config)
    for k in ("size", "fps", "seed", "max_mb", "song"):
        if getattr(args, k) is not None:
            cfg[k] = getattr(args, k)
    game = os.path.abspath(args.game)
    if not os.path.exists(game):
        sys.exit(f"build tetris-universe first ({game} is missing), or pass --game")
    work = os.path.abspath(args.work)
    os.makedirs(work, exist_ok=True)
    width, height = map(int, cfg.get("size", "1920x1080").split("x"))
    fps = int(cfg.get("fps", 30))

    song = resolve_song(cfg["song"], cfg.get("song_title") if not args.song else None, work)
    an = analyze(game, song, work)
    grid = Grid(an)
    log(f"song: {os.path.basename(song)}  {an['duration']:.1f} s, {an['bpm']:.1f} BPM, sections: "
        + " ".join(f"{s['kind']}@{s['start']:.1f}" for s in an["segments"]))
    universes = enabled_universes(game)
    timeline, missing = plan(cfg, grid, universes)
    print_plan(timeline, grid, universes, missing)
    if args.plan_only:
        return
    if args.only:
        keep = set(args.only.split(","))
        record_all([e for e in timeline if e.get("u") in keep], game, song, f"{width}x{height}", work, args.jobs)
        return
    record_all(timeline, game, song, f"{width}x{height}", work, args.jobs)
    out = os.path.abspath(args.out)
    assemble(timeline, cfg, grid, song, out, width, height, fps, work, cfg.get("max_mb", 29.5), args.crf)
    contact_sheet(timeline, args.sheet or os.path.splitext(out)[0] + "-sheet.png", out, fps)


if __name__ == "__main__":
    main()
