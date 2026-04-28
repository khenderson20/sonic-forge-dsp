#!/usr/bin/env python3
"""
serve-web.py — Development HTTP server for SonicForge DSP web demo.

Serves web/public/ on http://localhost:8080 with the Cross-Origin
Isolation headers required for SharedArrayBuffer support:

    Cross-Origin-Opener-Policy:   same-origin
    Cross-Origin-Embedder-Policy: require-corp

Without these headers browsers disable SharedArrayBuffer (Chrome 92+,
Firefox 79+), which means the "Live Audio" ring-buffer mode will not
activate.  The plain `python3 -m http.server` command does NOT set
these headers.

Usage
─────
    python3 scripts/serve-web.py            # serves on port 8080
    python3 scripts/serve-web.py --port 9090

Then open:  http://localhost:8080

Notes
─────
• The Three.js CDN assets (unpkg.com) are loaded via <script type="module">
  with crossorigin="anonymous".  unpkg.com serves
  Cross-Origin-Resource-Policy: cross-origin so COEP: require-corp is
  satisfied without any local mirroring.

• This server is for local development only.  Do not expose it to the
  internet without TLS and proper access controls.
"""

import argparse
import os
from http.server import HTTPServer, SimpleHTTPRequestHandler

# Absolute path to the directory containing index.html, main.js, etc.
WEB_PUBLIC = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          '..', 'web', 'public')


class CrossOriginIsolatedHandler(SimpleHTTPRequestHandler):
    """SimpleHTTPRequestHandler that injects COOP / COEP response headers."""

    def __init__(self, *args, **kwargs):
        # Change to the web/public directory so SimpleHTTPRequestHandler
        # serves files relative to it.
        super().__init__(*args, directory=WEB_PUBLIC, **kwargs)

    def end_headers(self):
        # Cross-Origin Opener Policy: prevents the page from sharing a
        # browsing context group with cross-origin openers.
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        # Cross-Origin Embedder Policy: requires all sub-resources to opt in
        # to being embedded cross-origin (CDN assets use CORP: cross-origin).
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        super().end_headers()

    def log_message(self, fmt, *args):  # pylint: disable=arguments-differ
        # Suppress the default per-request log noise; print a concise line.
        print(f'  {self.address_string()}  {fmt % args}')


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--port', type=int, default=8080,
                        help='TCP port to listen on (default: 8080)')
    parser.add_argument('--host', default='localhost',
                        help='Host / bind address (default: localhost)')
    args = parser.parse_args()

    server = HTTPServer((args.host, args.port), CrossOriginIsolatedHandler)

    print()
    print('  SonicForge DSP — Cross-Origin Isolated Dev Server')
    print(f'  Serving:  {os.path.normpath(WEB_PUBLIC)}')
    print(f'  URL:      http://{args.host}:{args.port}')
    print()
    print('  Headers added:')
    print('    Cross-Origin-Opener-Policy:   same-origin')
    print('    Cross-Origin-Embedder-Policy: require-corp')
    print()
    print('  SharedArrayBuffer ring buffer (Live Audio mode) is ENABLED.')
    print()
    print('  Press Ctrl+C to stop.')
    print()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print('\n  Server stopped.')


if __name__ == '__main__':
    main()
