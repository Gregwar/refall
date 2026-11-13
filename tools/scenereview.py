#!/usr/bin/env python3
"""Scene review: render batches of random scenes, review them in the browser (phone or computer), and discuss
changes with Claude in threads.

Dashboard tabs:
  scenes      new scenes, one at a time: skip one (-> skipped) or comment on it (-> to process)
  skipped     scenes skipped without a comment (they can still be commented)
  to process  commented scenes, waiting for Claude
  to review   Claude's changes, with before/after snapshots: accept (-> done) or comment again (-> to process)
  suggestions Claude's own proposals, each on its own local branch (suggest/...), not in main: before (main) / after
              (the branch) snapshots; accept (-> merged by Claude), dismiss (-> branch dropped) or comment (-> to
              process; Claude's answer comes back here)
  done        accepted threads, accepted or dismissed suggestions

Commands:
  generate [--count 100] [--jobs 3] [--size 1280x720]
      Renders COUNT snapshots, each a random scene made for a random downloaded song of the default playlist, in a
      random part of it (calm, mid and peak levels in turn), with a filled board to show the blocks.
  serve [--port 8765] [--lan] [--no-browser]
      Runs the dashboard (--lan: reachable from the local network, e.g. a phone).
  pending
      Prints the threads to process (JSON: scene, snapshots, messages).
  watch [--interval 5] [--all]
      Waits until a thread to process appears that was not handed out yet, prints the new ones (JSON), marks them
      handed out and exits: run it in the background to be woken up. --all also returns the ones handed out.
  reply KEY --message TEXT [--no-render] [--image PNG]
      Answers a thread: renders the scene again with the current build (same song, seed, time and level) as the
      "after" snapshot, the previous one being "before", and moves the thread to "to review".
  suggest --code CODE --branch BRANCH --after-bin REFALL_BIN --title TEXT --message TEXT [--level --size]
      Opens a suggestion: the scene (a downloaded song, the same seed) rendered with main's build/refall (before) and
      with the branch's refall (after). Replies to it re-render "after" with the branch's binary and bring it back to
      "suggestions". --before-image / --after-image PNG replace the renders.
  suggestions
      Prints the open suggestions (key, status, branch, title), to avoid proposing the same thing twice.
  resolve KEY
      Closes an accepted (merged) or dismissed (branch dropped) suggestion: -> done.

Everything lives in the user data folder (~/.local/share/zentris/scene-review/): one folder per batch (snapshots,
scenes.jsonl with what reproduces each scene), threads.json (the threads and their state) and feedback.jsonl (every
message, appended, for later processing).
Needs a built refall (build/refall).

With universes, a scene "code" is a universe and its variant ("VECTOR:P3 AMBER", as refall --universe
takes it); "scene" is its label ("VECTOR · P3 AMBER"). Old threads keep their old scene codes, which the
current binaries no longer understand (re-rendering those falls back to a random universe).
"""
import argparse
import concurrent.futures
import contextlib
import datetime
import fcntl
import glob
import json
import os
import random
import re
import shlex
import socket
import subprocess
import sys
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import unquote, urlparse

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "build", "refall")
DATA = os.path.join(os.environ.get("XDG_DATA_HOME", os.path.expanduser("~/.local/share")), "zentris", "scene-review")
CACHE = os.path.join(os.environ.get("XDG_CACHE_HOME", os.path.expanduser("~/.cache")), "zentris", "youtube")
THREADS = os.path.join(DATA, "threads.json")
FEEDBACK = os.path.join(DATA, "feedback.jsonl")
LEVELS = {0: "calm", 1: "mid", 2: "peak"}


def now():
    return datetime.datetime.now().isoformat(timespec="seconds")


# ---------------------------------------------------------------- songs and rendering

def default_playlist_id():
    with open(os.path.join(ROOT, "src", "youtube.hpp")) as f:
        return re.search(r'DEFAULT_PLAYLIST = "[^"]*list=([\w-]+)"', f.read()).group(1)


def cached_songs():
    """(video id, title, mp3 path) of the downloaded songs, those of the default playlist first."""
    titles, order = {}, []
    lists = sorted(glob.glob(os.path.join(CACHE, "lists", "*.tsv")))
    first = os.path.join(CACHE, "lists", f"list-{default_playlist_id()}.tsv")
    if first in lists:
        lists.remove(first)
        lists.insert(0, first)
    for tsv in lists:
        with open(tsv, encoding="utf-8", errors="replace") as f:
            for line in f:
                parts = line.rstrip("\n").split("\t")
                if parts and parts[0] and parts[0] not in titles:
                    titles[parts[0]] = parts[1] if len(parts) > 1 else parts[0]
                    order.append(parts[0])
    songs = [(v, titles[v], os.path.join(CACHE, v + ".mp3")) for v in order]
    songs = [s for s in songs if os.path.exists(s[2])]
    if not songs:  # no listing: any downloaded song
        songs = [(os.path.basename(p)[:-4], os.path.basename(p)[:-4], p) for p in glob.glob(os.path.join(CACHE, "*.mp3"))]
    return songs


def run_snapshot(cmd):
    """Runs a --review-shot command; its [review] JSON line, or None."""
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=180).stdout
    except subprocess.TimeoutExpired:
        return None
    for line in out.splitlines():
        if line.startswith("[review] "):
            return json.loads(line[len("[review] "):])
    return None


def render(i, song, level, folder, size):
    vid, title, path = song
    seed = random.getrandbits(63) | 1
    shot = os.path.join(folder, f"scene{i:03d}.png")
    cmd = [GAME, "--hidden", "--mute", "--size", size, "--seed", str(seed), "--review-shot", shot,
           "--review-level", str(level), path]
    info = run_snapshot(cmd)
    if info:
        info.update(id=i, image=os.path.basename(shot), video=vid, title=title, seed=seed, wanted_level=level,
                    level_name=LEVELS.get(info["level"], "?"), snapshot_cmd=shlex.join(cmd))
    return info


def generate(args):
    if not os.path.exists(GAME):
        sys.exit(f"build refall first ({GAME} is missing)")
    songs = cached_songs()
    if not songs:
        sys.exit(f"no downloaded song in {CACHE}: play the default playlist a little first")
    random.shuffle(songs)
    name = datetime.datetime.now().strftime("%Y-%m-%d_%H%M%S")
    folder = os.path.join(DATA, name)
    os.makedirs(folder, exist_ok=True)
    print(f"{len(songs)} downloaded songs; rendering {args.count} scenes into {folder}")
    jobs = [(i, songs[i % len(songs)], i % 3) for i in range(args.count)]
    scenes = []
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        futures = {pool.submit(render, i, s, lv, folder, args.size): i for i, s, lv in jobs}
        for n, fut in enumerate(concurrent.futures.as_completed(futures), 1):
            info = fut.result()
            if info:
                scenes.append(info)
                print(f"[{n}/{args.count}] {info['level_name']:4s} {info['scene']}  ({info['title'][:50]})")
            else:
                print(f"[{n}/{args.count}] failed (scene {futures[fut]})")
    scenes.sort(key=lambda s: s["id"])
    with open(os.path.join(folder, "scenes.jsonl"), "w") as f:
        for s in scenes:
            f.write(json.dumps(s, ensure_ascii=False) + "\n")
    print(f"done: {len(scenes)} scenes. Review them with: tools/scenereview.py serve")


# ---------------------------------------------------------------- scenes and threads

def batches():
    if not os.path.isdir(DATA):
        return []
    return sorted(d for d in os.listdir(DATA) if os.path.exists(os.path.join(DATA, d, "scenes.jsonl")))


def all_scenes():
    """Every scene of every batch, keyed 'batch:id'."""
    out = {}
    for b in batches():
        with open(os.path.join(DATA, b, "scenes.jsonl")) as f:
            for line in f:
                if line.strip():
                    s = json.loads(line)
                    s["batch"], s["key"] = b, f"{b}:{s['id']}"
                    out[s["key"]] = s
    return out


@contextlib.contextmanager
def threads_locked():
    """threads.json, read and written back under a lock (the dashboard and the commands share it)."""
    os.makedirs(DATA, exist_ok=True)
    with open(THREADS + ".lock", "w") as lk:
        fcntl.flock(lk, fcntl.LOCK_EX)
        state = {}
        if os.path.exists(THREADS):
            with open(THREADS) as f:
                state = json.load(f)
        yield state
        tmp = THREADS + ".tmp"
        with open(tmp, "w") as f:
            json.dump(state, f, ensure_ascii=False, indent=1)
        os.replace(tmp, THREADS)


def read_threads():
    if not os.path.exists(THREADS):
        return {}
    with open(THREADS) as f:
        return json.load(f)


def log_feedback(scene, msg, status):
    entry = {"key": scene["key"], "status": status, **msg, "code": scene["code"], "scene": scene["scene"],
             "level": scene["level_name"], "section": scene["section"], "energy": scene["energy"],
             "time": scene["time"], "video": scene["video"], "title": scene["title"], "song": scene["song"],
             "seed": scene["seed"], "snapshot_cmd": scene["snapshot_cmd"]}
    with open(FEEDBACK, "a") as f:
        f.write(json.dumps(entry, ensure_ascii=False) + "\n")


def user_action(key, action, text=""):
    scene = all_scenes().get(key)
    if scene is None:
        raise KeyError(key)
    with threads_locked() as threads:
        t = threads.setdefault(key, {"key": key, "status": "scenes", "messages": [], "handed": False})
        if action == "skip":
            msg = {"from": "user", "at": now(), "kind": "skip", "text": text}
            t["status"] = "skipped"
        elif action == "accept":
            msg = {"from": "user", "at": now(), "kind": "accept", "text": text}
            # An accepted suggestion still has to be merged by Claude.
            t["status"] = "accepted" if scene.get("branch") else "done"
            t["handed"] = False
        elif action == "dismiss":
            msg = {"from": "user", "at": now(), "kind": "dismiss", "text": text}
            t["status"] = "dismissed"
            t["handed"] = False
        else:
            if not text.strip():
                raise ValueError("empty comment")
            msg = {"from": "user", "at": now(), "kind": "comment", "text": text.strip()}
            t["status"] = "process"
            t["handed"] = False
        t["messages"].append(msg)
        status = t["status"]
    log_feedback(scene, msg, status)
    return t


def thread_view(t, scenes):
    """A thread with its scene and the snapshot files, for the commands' JSON."""
    s = scenes[t["key"]]
    folder = os.path.join(DATA, s["batch"])
    msgs = []
    for m in t["messages"]:
        m = dict(m)
        for k in ("before", "after"):
            if m.get(k):
                m[k] = os.path.join(folder, m[k])
        msgs.append(m)
    return {"key": t["key"], "status": t["status"], "scene": s["scene"], "code": s["code"], "level": s["level_name"],
            "section": s["section"], "energy": s["energy"], "time": s["time"], "title": s["title"],
            "song": s["song"], "snapshot_cmd": s["snapshot_cmd"], "original": os.path.join(folder, s["image"]),
            "latest": latest_image(t, s, folder), "messages": msgs}


def latest_image(t, s, folder):
    for m in reversed(t["messages"]):
        if m.get("after"):
            return os.path.join(folder, m["after"])
    return os.path.join(folder, s["image"])


def cmd_pending(args):
    scenes = all_scenes()
    out = [thread_view(t, scenes) for t in read_threads().values() if t["status"] == "process"]
    print(json.dumps(out, ensure_ascii=False, indent=1))


def cmd_watch(args):
    scenes = None
    while True:
        with threads_locked() as threads:
            new = [t for t in threads.values()
                   if t["status"] in ("process", "accepted", "dismissed") and (args.all or not t.get("handed"))]
            for t in new:
                t["handed"] = True
        if new:
            scenes = all_scenes()
            print(json.dumps([thread_view(t, scenes) for t in new], ensure_ascii=False, indent=1))
            return
        time.sleep(args.interval)


def cmd_reply(args):
    scenes = all_scenes()
    threads = read_threads()
    if args.key not in threads:
        sys.exit(f"no thread {args.key}")
    s = scenes[args.key]
    folder = os.path.join(DATA, s["batch"])
    t = threads[args.key]
    suggestion = bool(s.get("branch"))
    # A suggestion always compares main (the original snapshot) with its branch.
    before = s["image"] if suggestion else next((m["after"] for m in reversed(t["messages"]) if m.get("after")), s["image"])
    after = None
    if args.before_image:  # e.g. a contact sheet of several scenes, paired with --image
        before = f"after/scene{s['id']:03d}-r{len(t['messages'])}-before.png"
        os.makedirs(os.path.join(folder, "after"), exist_ok=True)
        with open(args.before_image, "rb") as src, open(os.path.join(folder, before), "wb") as dst:
            dst.write(src.read())
    if args.image:
        after = f"scene{s['id']:03d}-r{len(t['messages'])}.png"
        os.makedirs(os.path.join(folder, "after"), exist_ok=True)
        with open(args.image, "rb") as src, open(os.path.join(folder, "after", after), "wb") as dst:
            dst.write(src.read())
        after = "after/" + after
    elif not args.no_render:
        after = f"after/scene{s['id']:03d}-r{len(t['messages'])}.png"
        os.makedirs(os.path.join(folder, "after"), exist_ok=True)
        cmd = shlex.split(s["snapshot_cmd"])
        cmd[cmd.index("--review-shot") + 1] = os.path.join(folder, after)
        if suggestion:
            cmd[0] = s["after_bin"]
        info = run_snapshot(cmd)
        if not info:
            sys.exit("could not render the scene again")
        print(f"rendered {info['scene']} ({info['code']})")
    msg = {"from": "claude", "at": now(), "kind": "change", "text": args.message, "before": before, "after": after}
    if after is None:
        msg.pop("before")
    status = "suggest" if suggestion else "review"
    with threads_locked() as threads:
        th = threads[args.key]
        th["messages"].append(msg)
        th["status"] = status
    log_feedback(s, msg, status)
    print(f"{args.key} -> {'suggestions' if suggestion else 'to review'}")


def cmd_suggest(args):
    for b in (GAME, args.after_bin):
        if not os.path.exists(b):
            sys.exit(f"missing {b}")
    songs = cached_songs()
    if not songs:
        sys.exit("no downloaded song to render the scene on")
    folder = os.path.join(DATA, "suggestions")
    os.makedirs(os.path.join(folder, "after"), exist_ok=True)
    tag = f"{os.getpid()}-{int(time.time() * 1000)}"
    tmp_before, tmp_after = os.path.join(folder, f"in-{tag}-a.png"), os.path.join(folder, f"in-{tag}-b.png")
    # the same song, seed and level before and after: only the branch's change differs
    seed = random.getrandbits(63) | 1
    cmd = [GAME, "--hidden", "--mute", "--size", args.size, "--seed", str(seed), "--universe", args.code,
           "--review-shot", tmp_before, "--review-level", str(args.level), songs[0][2]]
    info = run_snapshot(cmd)
    after_cmd = [args.after_bin] + cmd[1:]
    after_cmd[after_cmd.index("--review-shot") + 1] = tmp_after
    info_after = run_snapshot(after_cmd)
    if not info or not info_after:
        sys.exit("could not render the scene")
    # Given images replace the renders (a moment the review shot doesn't show).
    for src, dst in ((args.before_image, tmp_before), (args.after_image, tmp_after)):
        if src:
            with open(src, "rb") as a, open(dst, "wb") as b:
                b.write(a.read())
    with threads_locked() as threads:
        jsonl = os.path.join(folder, "scenes.jsonl")
        sid = sum(1 for line in open(jsonl) if line.strip()) if os.path.exists(jsonl) else 0
        image, after = f"scene{sid:03d}.png", f"after/scene{sid:03d}-r0.png"
        os.replace(tmp_before, os.path.join(folder, image))
        os.replace(tmp_after, os.path.join(folder, after))
        cmd[cmd.index("--review-shot") + 1] = os.path.join(folder, image)
        level = info["level"]
        scene = {"id": sid, "image": image, "code": info["code"], "scene": info["scene"], "level": level,
                 "level_name": LEVELS.get(level, "?"), "section": info["section"],
                 "energy": [0.25, 0.55, 0.9][level], "bpm": info["bpm"], "time": 0, "duration": 0, "phase": 0,
                 "phases": 1, "levels": "", "title": args.title, "song": "", "video": "", "seed": 0,
                 "source": "suggestion", "branch": args.branch, "after_bin": os.path.abspath(args.after_bin),
                 "snapshot_cmd": shlex.join(cmd)}
        with open(jsonl, "a") as f:
            f.write(json.dumps(scene, ensure_ascii=False) + "\n")
        key = f"suggestions:{sid}"
        msg = {"from": "claude", "at": now(), "kind": "suggest", "text": args.message, "before": image, "after": after}
        threads[key] = {"key": key, "status": "suggest", "messages": [msg], "handed": True}
    scene["key"], scene["batch"] = key, "suggestions"
    log_feedback(scene, msg, "suggest")
    print(f"{key} -> suggestions ({args.title}, branch {args.branch})")


def cmd_suggestions(args):
    scenes, threads = all_scenes(), read_threads()
    for k, t in threads.items():
        s = scenes.get(k)
        if s and s.get("branch"):
            print(f"{k}\t{t['status']}\t{s['branch']}\t{s.get('title', '')}")


def cmd_resolve(args):
    with threads_locked() as threads:
        if args.key not in threads:
            sys.exit(f"no thread {args.key}")
        threads[args.key]["status"] = "done"
    print(f"{args.key} -> done")


def lan_ip():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        try:
            s.connect(("192.0.2.1", 9))  # no packet is sent: this only picks the outgoing interface
            return s.getsockname()[0]
        except OSError:
            return "127.0.0.1"


def serve(args):

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def reply(self, body, ctype="application/json", code=200):
            data = body if isinstance(body, bytes) else body.encode()
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            self.wfile.write(data)

        def do_GET(self):
            path = urlparse(self.path).path
            if path == "/":
                self.reply(PAGE, "text/html; charset=utf-8")
            elif path == "/api/state":
                self.reply(json.dumps({"scenes": list(all_scenes().values()), "threads": read_threads()}))
            elif path.startswith("/img/"):
                parts = [unquote(p) for p in path[5:].split("/")]
                if len(parts) < 2 or parts[0] not in batches() or any(p in ("", ".", "..") for p in parts):
                    return self.reply("not found", "text/plain", 404)
                file = os.path.join(DATA, *parts)
                if not file.endswith(".png") or not os.path.exists(file):
                    return self.reply("not found", "text/plain", 404)
                with open(file, "rb") as f:
                    self.reply(f.read(), "image/png")
            else:
                self.reply("not found", "text/plain", 404)

        def do_POST(self):
            if urlparse(self.path).path != "/api/action":
                return self.reply("not found", "text/plain", 404)
            req = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            try:
                t = user_action(req.get("key"), req.get("action"), req.get("text", ""))
            except (KeyError, ValueError) as e:
                return self.reply(str(e), "text/plain", 400)
            self.reply(json.dumps(t))

    host = "0.0.0.0" if args.lan else "127.0.0.1"
    server = ThreadingHTTPServer((host, args.port), Handler)
    url = f"http://{lan_ip() if args.lan else '127.0.0.1'}:{args.port}/"
    print(f"scene review at {url}\nthreads: {THREADS}\nCtrl+C to stop", flush=True)
    if not args.no_browser:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    g = sub.add_parser("generate", help="render a batch of random scenes")
    g.add_argument("--count", type=int, default=100)
    g.add_argument("--jobs", type=int, default=3, help="renders at the same time")
    g.add_argument("--size", default="1280x720")
    s = sub.add_parser("serve", help="run the dashboard")
    s.add_argument("--port", type=int, default=8765)
    s.add_argument("--lan", action="store_true", help="listen on the local network (phone)")
    s.add_argument("--no-browser", action="store_true")
    sub.add_parser("pending", help="print the threads to process")
    w = sub.add_parser("watch", help="wait for new threads to process")
    w.add_argument("--interval", type=float, default=5)
    w.add_argument("--all", action="store_true")
    r = sub.add_parser("reply", help="answer a thread (renders the after snapshot)")
    r.add_argument("key")
    r.add_argument("--message", required=True)
    r.add_argument("--no-render", action="store_true", help="answer without a new snapshot")
    r.add_argument("--image", help="use this PNG as the after snapshot")
    r.add_argument("--before-image", help="use this PNG as the before snapshot")
    sg = sub.add_parser("suggest", help="open a suggestion (a branch's change, before/after)")
    sg.add_argument("--code", required=True)
    sg.add_argument("--branch", required=True)
    sg.add_argument("--after-bin", required=True, help="the branch's refall binary")
    sg.add_argument("--title", required=True)
    sg.add_argument("--message", required=True)
    sg.add_argument("--level", type=int, default=1, choices=[0, 1, 2])
    sg.add_argument("--size", default="1280x720")
    sg.add_argument("--before-image", help="use this PNG as the before snapshot (e.g. a zentris shot with the HUD)")
    sg.add_argument("--after-image", help="use this PNG as the after snapshot")
    sub.add_parser("suggestions", help="list the suggestions")
    rs = sub.add_parser("resolve", help="close a merged or dropped suggestion")
    rs.add_argument("key")
    args = ap.parse_args()
    {"generate": generate, "serve": serve, "pending": cmd_pending, "watch": cmd_watch, "reply": cmd_reply,
     "suggest": cmd_suggest, "suggestions": cmd_suggestions, "resolve": cmd_resolve}[args.cmd](args)


if __name__ == "__main__":
    main()
