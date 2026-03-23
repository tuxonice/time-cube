#!/usr/bin/env node
/**
 * Mock backend server for Time Cube testing.
 *
 * Emulates:
 *   GET  /cube-config        → returns face→task mapping
 *   POST /start/<task_id>    → marks task as started
 *   POST /stop/<task_id>     → marks task as stopped
 *
 * Usage:
 *   node mock-server.js [--port 8080] [--token mytoken]
 */

const http = require("http");
const os = require("os");

const args = parseArgs(process.argv.slice(2));
const PORT = args.port ?? 8080;
const TOKEN = args.token ?? null;

const CUBE_CONFIG = {
  faces: {
    blue:   { task_id: 101, name: "Coding" },
    yellow: { task_id: 102, name: "Meetings" },
    red:    { task_id: 103, name: "Email" },
    green:  { task_id: 104, name: "Research" },
    orange: { task_id: 105, name: "Break" },
    white:  { task_id: 106, name: "Admin" },
  },
};

const TASK_NAMES = Object.fromEntries(
  Object.values(CUBE_CONFIG.faces).map(({ task_id, name }) => [task_id, name])
);

function parseArgs(argv) {
  const result = {};
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === "--port")  result.port  = Number(argv[++i]);
    if (argv[i] === "--token") result.token = argv[++i];
  }
  return result;
}

function localIp() {
  for (const iface of Object.values(os.networkInterfaces()).flat()) {
    if (iface.family === "IPv4" && !iface.internal) return iface.address;
  }
  return "127.0.0.1";
}

function log(method, path, status, note = "") {
  const ts = new Date().toTimeString().slice(0, 8);
  const statusColor = status === 200 ? "\x1b[32m" : "\x1b[31m";
  const reset = "\x1b[0m";
  const extra = note ? `  ← ${note}` : "";
  console.log(`[${ts}]  ${method.padEnd(6)} ${path.padEnd(30)} ${statusColor}${status}${reset}${extra}`);
}

function send(res, status, body) {
  const payload = JSON.stringify(body, null, 2);
  res.writeHead(status, { "Content-Type": "application/json" });
  res.end(payload);
}

function checkAuth(req) {
  if (!TOKEN) return true;
  return req.headers["x-time-cube-token"] === TOKEN;
}

const server = http.createServer((req, res) => {
  if (!checkAuth(req)) {
    log(req.method, req.url, 401, "unauthorized");
    return send(res, 401, { error: "unauthorized" });
  }

  if (req.method === "GET" && req.url === "/cube-config") {
    log("GET", req.url, 200, "returned cube config");
    return send(res, 200, CUBE_CONFIG);
  }

  const match = req.url.match(/^\/(start|stop)\/(\d+)$/);
  if (req.method === "POST" && match) {
    const action = match[1];
    const taskId = Number(match[2]);
    const name = TASK_NAMES[taskId] ?? "unknown";
    const verb = action === "start" ? "▶ Started" : "■ Stopped";
    log("POST", req.url, 200, `${verb} task ${taskId} "${name}"`);
    return send(res, 200, { ok: true, task_id: taskId, action });
  }

  log(req.method, req.url, 404);
  send(res, 404, { error: "not found" });
});

server.listen(PORT, "0.0.0.0", () => {
  const ip = localIp();
  console.log(`Time Cube mock server running on http://0.0.0.0:${PORT}`);
  console.log(`Set endpoint base URL in cube config to: http://${ip}:${PORT}`);
  console.log(TOKEN ? `Expecting token: ${TOKEN}` : "Auth: disabled");
  console.log();
});
