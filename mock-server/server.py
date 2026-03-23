#!/usr/bin/env python3
"""
Mock backend server for Time Cube testing.

Emulates:
  GET  /cube-config        → returns face→task mapping
  POST /start/<task_id>    → marks task as started
  POST /stop/<task_id>     → marks task as stopped

Usage:
  python3 server.py [--port 8080] [--token mytoken]
"""

import argparse
import json
import re
import socket
from datetime import datetime
from http.server import BaseHTTPRequestHandler, HTTPServer

CUBE_CONFIG = {
    "faces": {
        "blue":   {"task_id": 101, "name": "Coding"},
        "yellow": {"task_id": 102, "name": "Meetings"},
        "red":    {"task_id": 103, "name": "Email"},
        "green":  {"task_id": 104, "name": "Research"},
        "orange": {"task_id": 105, "name": "Break"},
        "white":  {"task_id": 106, "name": "Admin"},
    }
}

TASK_NAMES = {v["task_id"]: v["name"] for v in CUBE_CONFIG["faces"].values()}


def log(method, path, status, note=""):
    ts = datetime.now().strftime("%H:%M:%S")
    color = "\033[32m" if status == 200 else "\033[31m"
    reset = "\033[0m"
    extra = f"  ← {note}" if note else ""
    print(f"[{ts}]  {method:<6} {path:<30} {color}{status}{reset}{extra}")


class Handler(BaseHTTPRequestHandler):
    token = None

    def log_message(self, *args):
        pass  # suppress default access log

    def _check_auth(self):
        if not Handler.token:
            return True
        return self.headers.get("X-Time-Cube-Token", "") == Handler.token

    def _send(self, status, body):
        payload = json.dumps(body, indent=2).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", len(payload))
        self.end_headers()
        self.wfile.write(payload)

    def do_GET(self):
        if not self._check_auth():
            log("GET", self.path, 401, "unauthorized")
            return self._send(401, {"error": "unauthorized"})

        if self.path == "/cube-config":
            log("GET", self.path, 200, "returned cube config")
            return self._send(200, CUBE_CONFIG)

        log("GET", self.path, 404)
        self._send(404, {"error": "not found"})

    def do_POST(self):
        if not self._check_auth():
            log("POST", self.path, 401, "unauthorized")
            return self._send(401, {"error": "unauthorized"})

        m = re.fullmatch(r"/(start|stop)/(\d+)", self.path)
        if m:
            action, task_id = m.group(1), int(m.group(2))
            name = TASK_NAMES.get(task_id, "unknown")
            verb = "▶ Started" if action == "start" else "■ Stopped"
            log("POST", self.path, 200, f'{verb} task {task_id} "{name}"')
            return self._send(200, {"ok": True, "task_id": task_id, "action": action})

        log("POST", self.path, 404)
        self._send(404, {"error": "not found"})


def local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except Exception:
        return "127.0.0.1"
    finally:
        s.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Time Cube mock backend")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--token", type=str, default="")
    args = parser.parse_args()

    Handler.token = args.token or None

    ip = local_ip()
    print(f"Time Cube mock server running on http://0.0.0.0:{args.port}")
    print(f"Set endpoint base URL in cube config to: http://{ip}:{args.port}")
    print(f"Expecting token: {args.token}" if Handler.token else "Auth: disabled")
    print()

    HTTPServer(("0.0.0.0", args.port), Handler).serve_forever()
