#!/usr/bin/env python3
"""Serves the web build's site (build-web/site, made by tools/web.sh) locally, with the headers its threads need
(cross-origin isolation): python3 tools/webserve.py [folder] [port], then open http://localhost:8000."""
import functools
import http.server
import sys


class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        super().end_headers()


Handler.extensions_map.update({".wasm": "application/wasm", ".js": "text/javascript", ".mjs": "text/javascript"})

if __name__ == "__main__":
    folder = sys.argv[1] if len(sys.argv) > 1 else "build-web/site"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8000
    handler = functools.partial(Handler, directory=folder)
    print(f"Refall (web) on http://localhost:{port}  ({folder})")
    http.server.ThreadingHTTPServer(("", port), handler).serve_forever()
