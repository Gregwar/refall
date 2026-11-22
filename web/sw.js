// Refall's service worker. Two jobs:
// - the headers the game's threads need (a cross-origin isolated page), added to every response, for hosts that
//   can't send them (static hosts such as GitHub Pages; PHP and Apache send their own, the same);
// - the game kept on the device, so it starts offline and installs as an app: the page, refall.js / .wasm / .data
//   and the icons stored when the worker installs (a new version of the site is a new worker: tools/web.sh stamps
//   VERSION), each song stored the first time it is played (kept across versions).
const VERSION = "dev";
const SHELL = "refall-" + VERSION, SONGS = "refall-songs";
const FILES = ["./", "refall.js", "refall.wasm", "refall.data", "icon.png", "icon-192.png", "icon-512.png",
               "icon-maskable.png", "manifest.webmanifest"];

self.addEventListener("install", (e) => {
  e.waitUntil(caches.open(SHELL).then((c) => c.addAll(FILES.map((f) => new Request(f, { cache: "reload" })))));
});

self.addEventListener("activate", (e) => {
  e.waitUntil(caches.keys()
    .then((keys) => Promise.all(keys.filter((k) => k !== SHELL && k !== SONGS).map((k) => caches.delete(k))))
    .then(() => self.clients.claim()));
});

// the page asks a waiting worker to take over (only before the game runs: a running game keeps the files it started with)
self.addEventListener("message", (e) => { if (e.data === "skipWaiting") self.skipWaiting(); });

function isolated(r) {
  if (r.status === 0) return r; // (opaque: can't be changed)
  const h = new Headers(r.headers);
  h.set("Cross-Origin-Opener-Policy", "same-origin");
  h.set("Cross-Origin-Embedder-Policy", "require-corp");
  h.set("Cross-Origin-Resource-Policy", "same-origin");
  return new Response(r.body, { status: r.status, statusText: r.statusText, headers: h });
}

self.addEventListener("fetch", (e) => {
  const req = e.request, url = new URL(req.url);
  if (req.method !== "GET" || url.origin !== location.origin) return;
  if (req.cache === "only-if-cached" && req.mode !== "same-origin") return;
  const scope = new URL(self.registration.scope);
  const path = url.pathname.startsWith(scope.pathname) ? url.pathname.slice(scope.pathname.length) : null;
  e.respondWith((async () => {
    // a song: from the device once played, else from the server (then kept)
    if (path !== null && path.startsWith("songs/")) {
      const songs = await caches.open(SONGS);
      const hit = await songs.match(url.pathname);
      if (hit) return isolated(hit);
      const r = await fetch(req);
      if (r.ok && r.status === 200) await songs.put(url.pathname, r.clone());
      return isolated(r);
    }
    // the page (whatever its ?args=) and the game's files: this version's, stored at install
    const key = req.mode === "navigate" || path === "" || path === "index.php" ? "./" : path;
    if (key !== null && FILES.includes(key)) {
      const hit = await caches.match(new URL(key, scope).href, { cacheName: SHELL });
      if (hit) return isolated(hit);
    }
    try {
      return isolated(await fetch(req));
    } catch (err) {
      const hit = await caches.match(req);
      if (hit) return isolated(hit);
      throw err;
    }
  })());
});
