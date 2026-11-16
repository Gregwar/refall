#!/usr/bin/env python3
"""Play Refall from a phone on the same Wi-Fi: the game runs here (headless, on the GPU), this server sends its frames
(JPEG) and its sound (16-bit PCM) to a page in the phone's browser over a WebSocket, and the phone sends back its
gamepad (a Bluetooth controller paired with it, read by the browser's Gamepad API, or the page's touch buttons).

    python3 tools/stream.py [--port 8443] [--game build/refall] [-- GAME ARGS...]

then open https://<this machine's address>:8443 on the phone (the certificate is self-signed: accept it once; the
Gamepad API needs HTTPS). The game starts at the phone's aspect ratio when it connects, again if it quits.
"""
import argparse
import asyncio
import io
import os
import socket
import ssl
import subprocess
import sys
import threading
import time
from pathlib import Path

from aiohttp import WSMsgType, web
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
PAGE = Path(__file__).resolve().parent / "stream.html"
CACHE = Path.home() / ".cache" / "refall" / "stream"
HEIGHT = 720  # the game's frames are this tall, as wide as the phone's screen is in proportion


def lan_address():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("10.255.255.255", 1))
        return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()


def certificate(ip):
    """A self-signed certificate for this address (made once, again when the address changes)."""
    CACHE.mkdir(parents=True, exist_ok=True)
    cert, key, tag = CACHE / "cert.pem", CACHE / "key.pem", CACHE / "address"
    if not (cert.exists() and key.exists() and tag.exists() and tag.read_text() == ip):
        subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "3650",
                        "-keyout", str(key), "-out", str(cert), "-subj", "/CN=refall",
                        "-addext", f"subjectAltName=IP:{ip},DNS:localhost"],
                       check=True, capture_output=True)
        tag.write_text(ip)
    ctx = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
    ctx.load_cert_chain(str(cert), str(key))
    return ctx


def read_exactly(fd, n):
    buf = bytearray(n)
    view, got = memoryview(buf), 0
    while got < n:
        k = os.readv(fd, [view[got:]])
        if k <= 0:
            return None
        got += k
    return buf


class Game:
    """One run of the game: its process, the latest frame, and the sound to send."""

    def __init__(self, exe, args, w, h, loop, on_audio, on_exit):
        self.w, self.h = w, h
        vr, vw = os.pipe()
        ar, aw = os.pipe()
        env = dict(os.environ, REFALL_STREAM_VIDEO_FD=str(vw), REFALL_STREAM_AUDIO_FD=str(aw))
        cmd = [exe, "--stream", "--size", f"{w}x{h}", *args]
        print("[stream] start:", " ".join(cmd), flush=True)
        self.proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, env=env, pass_fds=(vw, aw), cwd=ROOT)
        os.close(vw)
        os.close(aw)
        self.frame, self.frame_id = None, 0
        self.cond = threading.Condition()
        self.alive = True
        self.loop, self.on_audio, self.on_exit = loop, on_audio, on_exit
        threading.Thread(target=self._video, args=(vr,), daemon=True).start()
        threading.Thread(target=self._audio, args=(ar,), daemon=True).start()

    def _video(self, fd):
        while True:
            head = read_exactly(fd, 8)
            if head is None:
                break
            w = int.from_bytes(head[:4], "little")
            h = int.from_bytes(head[4:], "little")
            px = read_exactly(fd, w * h * 3)
            if px is None:
                break
            with self.cond:
                self.frame, self.frame_id = (w, h, px), self.frame_id + 1
                self.cond.notify_all()
        os.close(fd)
        with self.cond:
            self.alive = False
            self.cond.notify_all()
        self.proc.wait()
        print("[stream] the game ended", flush=True)
        self.loop.call_soon_threadsafe(self.on_exit, self)

    def _audio(self, fd):
        while True:
            try:
                data = os.read(fd, 4800 * 4)  # up to 100 ms
            except OSError:
                break
            if not data:
                break
            if len(data) % 4:
                data += os.read(fd, 4 - len(data) % 4)
            self.loop.call_soon_threadsafe(self.on_audio, data)
        os.close(fd)

    def next_frame(self, after, timeout=1.0):
        with self.cond:
            self.cond.wait_for(lambda: self.frame_id != after or not self.alive, timeout)
            return self.frame, self.frame_id

    def send(self, line):
        try:
            self.proc.stdin.write((line + "\n").encode())
            self.proc.stdin.flush()
        except (BrokenPipeError, ValueError, OSError):
            pass

    def stop(self):
        try:
            self.proc.stdin.close()
        except OSError:
            pass
        try:
            self.proc.wait(5)
        except subprocess.TimeoutExpired:
            self.proc.kill()


def encode(frame, quality):
    w, h, px = frame
    # the rows come bottom first: raw decoder's orientation -1 flips them for free
    img = Image.frombuffer("RGB", (w, h), bytes(px), "raw", "RGB", 0, -1)
    out = io.BytesIO()
    img.save(out, "JPEG", quality=quality)
    return out.getvalue()


class Server:
    def __init__(self, opts):
        self.opts = opts
        self.game = None
        self.ws = None
        self.inflight = 0
        self.ready = asyncio.Event()

    def on_audio(self, data):
        if self.ws is not None and not self.ws.closed:
            asyncio.ensure_future(self._send(b"\x02" + data))

    def on_exit(self, game):
        if self.game is game:
            self.game = None
            if self.ws is not None and not self.ws.closed:
                asyncio.ensure_future(self.ws.send_str("ended"))

    async def _send(self, data):
        try:
            await self.ws.send_bytes(data)
        except (ConnectionError, RuntimeError):
            pass

    def ensure_game(self, w, h):
        if self.game is not None and (self.game.w, self.game.h) != (w, h):
            self.game.stop()
            self.game = None
        if self.game is None:
            self.game = Game(self.opts.game, self.opts.args, w, h, asyncio.get_running_loop(), self.on_audio, self.on_exit)

    async def frames(self, ws):
        """Sends the newest frame whenever the phone has drawn the previous ones (at most two on the way)."""
        loop = asyncio.get_running_loop()
        last = -1
        while not ws.closed:
            game = self.game
            if game is None or self.inflight >= 2:
                self.ready.clear()
                try:
                    await asyncio.wait_for(self.ready.wait(), 0.5)
                except asyncio.TimeoutError:
                    pass
                continue
            frame, fid = await loop.run_in_executor(None, game.next_frame, last)
            if frame is None or fid == last:
                continue
            last = fid
            jpg = await loop.run_in_executor(None, encode, frame, self.opts.quality)
            self.inflight += 1
            await self._send(b"\x01" + jpg)

    async def page(self, request):
        return web.FileResponse(PAGE, headers={"Cache-Control": "no-store"})

    async def socket(self, request):
        ws = web.WebSocketResponse(max_msg_size=1 << 20)
        await ws.prepare(request)
        if self.ws is not None and not self.ws.closed:
            await self.ws.close()  # one player: the newest connection takes over
        self.ws, self.inflight = ws, 0
        print("[stream] phone connected:", request.remote, flush=True)
        sender = None
        async for msg in ws:
            if msg.type != WSMsgType.TEXT:
                continue
            t = msg.data
            if t == "ack":
                self.inflight = max(0, self.inflight - 1)
                self.ready.set()
            elif t.startswith("pad ") and self.game is not None:
                self.game.send(t)
            elif t.startswith("hello "):
                sw, sh = (float(v) for v in t.split()[1:3])
                aspect = min(2.4, max(1.3, max(sw, sh) / max(1.0, min(sw, sh))))
                w = int(round(HEIGHT * aspect / 2)) * 2
                self.ensure_game(w, HEIGHT)
                self.inflight = 0
                if sender is None:
                    sender = asyncio.ensure_future(self.frames(ws))
            elif t == "restart":
                if self.game is None and self.opts.last_size:
                    self.ensure_game(*self.opts.last_size)
            if self.game is not None:
                self.opts.last_size = (self.game.w, self.game.h)
        if sender is not None:
            sender.cancel()
        print("[stream] phone disconnected", flush=True)
        return ws


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=8443)
    ap.add_argument("--game", default=str(ROOT / "build" / "refall"))
    ap.add_argument("--quality", type=int, default=80, help="JPEG quality (default 80)")
    ap.add_argument("args", nargs="*", help="the game's own arguments (after --)")
    opts = ap.parse_args()
    opts.last_size = None
    ip = lan_address()
    ctx = certificate(ip)
    server = Server(opts)
    app = web.Application()
    app.router.add_get("/", server.page)
    app.router.add_get("/ws", server.socket)
    print(f"[stream] open https://{ip}:{opts.port} on the phone (accept the self-signed certificate)", flush=True)

    async def shutdown(app):
        if server.game is not None:
            server.game.stop()

    app.on_shutdown.append(shutdown)
    web.run_app(app, host="0.0.0.0", port=opts.port, ssl_context=ctx, print=None)


if __name__ == "__main__":
    main()
