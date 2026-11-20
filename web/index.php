<?php
// Refall, the web version's page. Its threads need a cross-origin isolated page: these headers make it one (the
// service worker below is the fallback where PHP's headers don't reach, e.g. a host serving only static files).
header('Cross-Origin-Opener-Policy: same-origin');
header('Cross-Origin-Embedder-Policy: require-corp');
// PHP's own server (php -S HOST:PORT -t SITE SITE/index.php) runs this for every request: the other files go out
// from here with the headers too (the workers' refall.js needs them; Apache's come from .htaccess).
if (PHP_SAPI === 'cli-server') {
    $path = rawurldecode(parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH));
    $file = realpath(__DIR__ . $path);
    if ($path !== '/' && $file !== false && is_file($file) && strpos($file, __DIR__ . DIRECTORY_SEPARATOR) === 0
        && basename($file) !== 'index.php') {
        $types = ['js' => 'text/javascript', 'wasm' => 'application/wasm', 'data' => 'application/octet-stream',
                  'mp3' => 'audio/mpeg', 'png' => 'image/png', 'html' => 'text/html'];
        $ext = strtolower(pathinfo($file, PATHINFO_EXTENSION));
        header('Cross-Origin-Resource-Policy: same-origin');
        header('Content-Type: ' . ($types[$ext] ?? 'application/octet-stream'));
        header('Content-Length: ' . filesize($file));
        readfile($file);
        return;
    }
    if ($path !== '/' && $path !== '/index.php') {
        http_response_code(404);
        return;
    }
}
?>
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Refall</title>
<link rel="icon" href="icon.png">
<!-- the same headers through a service worker, where the server sent none (it reloads the page once) -->
<script src="coi-serviceworker.min.js"></script>
<style>
  html, body { margin: 0; height: 100%; background: #000; overflow: hidden; }
  canvas { display: block; position: fixed; inset: 0; width: 100vw; height: 100vh; outline: none; }
  #cover { position: fixed; inset: 0; display: flex; flex-direction: column; align-items: center; justify-content: center;
           gap: 28px; color: #fff; font: 15px/1.5 system-ui, sans-serif; background: #000; cursor: pointer; text-align: center; }
  #cover img { width: 128px; }
  #cover .title { font-size: 44px; letter-spacing: .35em; font-weight: 300; margin: -12px 0 8px .35em; }
  #cover .go { letter-spacing: .3em; font-size: 18px; }
  #cover .note { opacity: .55; max-width: 560px; padding: 0 16px; }
  #cover.loading { cursor: progress; }
  #cover.hidden { display: none; }
  /* the touch pad: shown on touch screens once the game runs, over the canvas */
  #pad { position: fixed; inset: 0; pointer-events: none; display: none; user-select: none; -webkit-user-select: none;
         touch-action: none; font: 600 13px/1 system-ui, sans-serif; letter-spacing: .12em; color: rgba(255,255,255,.85); }
  #pad.on { display: block; }
  #pad .b { position: absolute; pointer-events: auto; touch-action: none; display: flex; align-items: center; justify-content: center;
            border: 2px solid rgba(255,255,255,.35); background: rgba(255,255,255,.08); border-radius: 50%;
            -webkit-tap-highlight-color: transparent; }
  #pad .b.down { background: rgba(255,255,255,.35); }
  #dpad { position: absolute; left: max(18px, env(safe-area-inset-left)); bottom: 18px; width: 168px; height: 168px;
          pointer-events: auto; touch-action: none; }
  #dpad div { position: absolute; width: 56px; height: 56px; border: 2px solid rgba(255,255,255,.35);
              background: rgba(255,255,255,.08); border-radius: 12px; display: flex; align-items: center; justify-content: center; }
  #dpad div.down { background: rgba(255,255,255,.35); }
  #dpad .u { left: 56px; top: 0; } #dpad .d { left: 56px; bottom: 0; } #dpad .l { left: 0; top: 56px; } #dpad .r { right: 0; top: 56px; }
  #pad .a { right: max(18px, env(safe-area-inset-right)); bottom: 70px; width: 72px; height: 72px; }
  #pad .bb { right: calc(max(18px, env(safe-area-inset-right)) + 84px); bottom: 22px; width: 72px; height: 72px; }
  #pad .hold { right: calc(max(18px, env(safe-area-inset-right)) + 8px); bottom: 168px; width: 64px; height: 40px; border-radius: 20px; }
  #pad .start { left: 50%; transform: translateX(-50%); top: 10px; width: 64px; height: 30px; border-radius: 15px; font-size: 11px; }
  #pad .full { right: max(10px, env(safe-area-inset-right)); top: 10px; width: 38px; height: 38px; border-radius: 10px; font-size: 18px; }
</style>
</head>
<body>
<canvas id="canvas" tabindex="0" oncontextmenu="event.preventDefault()"></canvas>
<div id="cover">
  <img src="icon.png" alt=""><div class="title">REFALL</div>
  <div class="go" id="status">LOADING...</div>
  <div class="note">The Voyage, in your browser. Keyboard, gamepad, or the touch pad on a phone; F for full screen.<br>
  Best in Chrome or Edge.</div>
</div>
<div id="pad">
  <div id="dpad"><div class="u">&#9650;</div><div class="l">&#9664;</div><div class="r">&#9654;</div><div class="d">&#9660;</div></div>
  <div class="b a" data-b="0">A</div>
  <div class="b bb" data-b="1">B</div>
  <div class="b hold" data-b="4">HOLD</div>
  <div class="b start" data-b="9">PAUSE</div>
  <div class="b full" data-full>&#x26F6;</div>
</div>
<script>
  const cover = document.getElementById("cover"), status = document.getElementById("status");
  const canvas = document.getElementById("canvas");
  let ready = false, started = false;
  // the browser lets a page play sound only after a click or a key: the game starts on one
  function start() {
    if (!ready || started) return;
    started = true;
    cover.classList.add("hidden");
    canvas.focus();
    // (?args=...: the game's command line, for testing, e.g. ?args=--stage 2:1 --universe NOTEPAD)
    const args = new URLSearchParams(location.search).get("args");
    Module.callMain(args ? args.split(" ").filter((a) => a) : []);
    showPad();
  }

  // ---- The touch pad: a standard gamepad the page makes up (A rotates, B rotates back, the cross moves and drops,
  // HOLD, PAUSE), added to the browser's own gamepads, so the game reads it as any other.
  const touch = matchMedia("(pointer: coarse)").matches || "ontouchstart" in window;
  const vpad = { id: "Touch pad (STANDARD GAMEPAD)", index: 3, connected: true, mapping: "standard", timestamp: 0,
                 axes: [0, 0, 0, 0], buttons: Array.from({ length: 17 }, () => ({ pressed: false, touched: false, value: 0 })) };
  function setButton(b, on) {
    vpad.buttons[b] = { pressed: on, touched: on, value: on ? 1 : 0 };
    vpad.timestamp = performance.now();
  }
  if (touch && navigator.getGamepads) {
    const real = navigator.getGamepads.bind(navigator);
    navigator.getGamepads = () => { const l = Array.from(real()); while (l.length < 4) l.push(null); if (!l[3]) l[3] = vpad; return l; };
    const pad = document.getElementById("pad");
    pad.addEventListener("contextmenu", (e) => e.preventDefault());
    for (const el of pad.querySelectorAll(".b[data-b]")) {
      const b = +el.dataset.b;
      el.addEventListener("pointerdown", (e) => { e.preventDefault(); el.setPointerCapture(e.pointerId); el.classList.add("down"); setButton(b, true); });
      const up = () => { el.classList.remove("down"); setButton(b, false); };
      el.addEventListener("pointerup", up);
      el.addEventListener("pointercancel", up);
    }
    // the cross: one finger slides between its arms (the arm under it is held)
    const dpad = document.getElementById("dpad"), arms = { u: 12, d: 13, l: 14, r: 15 };
    let held = null;
    function aim(e) {
      const r = dpad.getBoundingClientRect(), x = e.clientX - r.left - r.width / 2, y = e.clientY - r.top - r.height / 2;
      const arm = Math.hypot(x, y) < 14 ? null : Math.abs(x) > Math.abs(y) ? (x < 0 ? "l" : "r") : (y < 0 ? "u" : "d");
      if (arm === held) return;
      if (held) { setButton(arms[held], false); dpad.querySelector("." + held).classList.remove("down"); }
      held = arm;
      if (held) { setButton(arms[held], true); dpad.querySelector("." + held).classList.add("down"); }
    }
    dpad.addEventListener("pointerdown", (e) => { e.preventDefault(); dpad.setPointerCapture(e.pointerId); aim(e); });
    dpad.addEventListener("pointermove", (e) => { if (e.buttons || e.pointerType === "touch") aim(e); });
    const release = () => { if (held) { setButton(arms[held], false); dpad.querySelector("." + held).classList.remove("down"); held = null; } };
    dpad.addEventListener("pointerup", release);
    dpad.addEventListener("pointercancel", release);
    pad.querySelector("[data-full]").addEventListener("click", () => {
      if (document.fullscreenElement) document.exitFullscreen();
      else document.documentElement.requestFullscreen().then(() => screen.orientation && screen.orientation.lock && screen.orientation.lock("landscape").catch(() => {})).catch(() => {});
    });
  }
  function showPad() {
    if (!touch) return;
    document.getElementById("pad").classList.add("on");
    const e = new Event("gamepadconnected"); // (the game listens once it runs)
    e.gamepad = vpad;
    dispatchEvent(e);
    watchPads();
  }
  // a real gamepad in use (a button pressed, a stick pushed) hides the touch pad; touching the screen brings it back
  function watchPads() {
    const pad = document.getElementById("pad"), real = navigator.getGamepads ? () => Array.from(navigator.getGamepads()) : () => [];
    const seen = new Map();
    (function poll() {
      for (const g of real()) {
        if (!g || g === vpad) continue;
        const busy = g.buttons.some((b) => b.pressed) || g.axes.some((a) => Math.abs(a) > 0.5);
        if (busy && seen.get(g.index) !== g.timestamp && pad.classList.contains("on")) {
          // (release any touch button still held, so nothing sticks while the pad is hidden)
          for (let b = 0; b < vpad.buttons.length; b++) if (vpad.buttons[b].pressed) setButton(b, false);
          for (const el of pad.querySelectorAll(".down")) el.classList.remove("down");
          document.getElementById("dpad").dispatchEvent(new Event("pointercancel"));
          pad.classList.remove("on");
        }
        seen.set(g.index, g.timestamp);
      }
      requestAnimationFrame(poll);
    })();
    addEventListener("touchstart", () => pad.classList.add("on"), { passive: true, capture: true });
  }
  cover.addEventListener("click", start);
  addEventListener("keydown", (e) => { if (!started) { e.preventDefault(); start(); } });
  var Module = {
    canvas,
    noInitialRun: true,
    print: (t) => console.log(t),
    printErr: (t) => console.warn(t),
    setStatus: (t) => { if (!ready && t) status.textContent = t.replace(/\(.*\)/, "").toUpperCase() || "LOADING..."; },
    onRuntimeInitialized: () => { ready = true; cover.classList.remove("loading"); status.textContent = touch ? "TAP TO START" : "CLICK OR PRESS A KEY TO START"; },
  };
  if (!crossOriginIsolated && !navigator.serviceWorker)
    status.textContent = "THIS BROWSER CAN'T RUN IT HERE (NO SHAREDARRAYBUFFER)";
  cover.classList.add("loading");
</script>
<script async src="refall.js"></script>
</body>
</html>
