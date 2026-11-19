#!/usr/bin/env python3
"""Serves the web build's site (build-web/site, made by tools/web.sh) locally, with the headers its threads need
(cross-origin isolation): python3 tools/webserve.py [folder] [port], then open http://localhost:8000.

From other devices (the LAN), browsers give threads only to secure pages: add a certificate,
python3 tools/webserve.py build-web/site 8443 cert.pem key.pem, and open https://<this machine's IP>:8443 (a
self-signed one makes the browser warn once: go on anyway). One can be made with
openssl req -x509 -newkey rsa:2048 -nodes -days 365 -subj /CN=refall -keyout key.pem -out cert.pem"""
import functools
import http.server
import ssl
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
    server = http.server.ThreadingHTTPServer(("", port), handler)
    scheme = "http"
    if len(sys.argv) > 4:
        tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        tls.load_cert_chain(sys.argv[3], sys.argv[4])
        server.socket = tls.wrap_socket(server.socket, server_side=True)
        scheme = "https"
    print(f"Refall (web) on {scheme}://localhost:{port}  ({folder})")
    server.serve_forever()
