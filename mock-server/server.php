#!/usr/bin/env php
<?php
/**
 * Mock backend server for Time Cube testing.
 *
 * Emulates:
 *   GET  /cube-config        → returns face→task mapping
 *   POST /start/<task_id>    → marks task as started
 *   POST /stop/<task_id>     → marks task as stopped
 *
 * Usage:
 *   php mock-server.php [--port 8080] [--token mytoken]
 */

$args  = parseArgs($argv);
$port  = $args['port']  ?? 8080;
$token = $args['token'] ?? null;

const CUBE_CONFIG = [
    'faces' => [
        'blue'   => ['task_id' => 101, 'name' => 'Coding'],
        'yellow' => ['task_id' => 102, 'name' => 'Meetings'],
        'red'    => ['task_id' => 103, 'name' => 'Email'],
        'green'  => ['task_id' => 104, 'name' => 'Research'],
        'orange' => ['task_id' => 105, 'name' => 'Break'],
        'white'  => ['task_id' => 106, 'name' => 'Admin'],
    ],
];

$taskNames = array_column(CUBE_CONFIG['faces'], 'name', 'task_id');

function parseArgs(array $argv): array
{
    $result = [];
    for ($i = 1; $i < count($argv); $i++) {
        if ($argv[$i] === '--port')  $result['port']  = (int) $argv[++$i];
        if ($argv[$i] === '--token') $result['token'] = $argv[++$i];
    }
    return $result;
}

function localIp(): string
{
    $sock = socket_create(AF_INET, SOCK_DGRAM, SOL_UDP);
    socket_connect($sock, '8.8.8.8', 53);
    socket_getsockname($sock, $ip);
    socket_close($sock);
    return $ip ?? '127.0.0.1';
}

function logRequest(string $method, string $path, int $status, string $note = ''): void
{
    $ts = date('H:i:s');
    $color = $status === 200 ? "\033[32m" : "\033[31m";
    $reset = "\033[0m";
    $extra = $note ? "  ← $note" : '';
    printf("[%s]  %-6s %-30s %s%d%s%s\n", $ts, $method, $path, $color, $status, $reset, $extra);
}

function send(mixed $socket, int $status, mixed $body): void
{
    $phrases = [200 => 'OK', 401 => 'Unauthorized', 404 => 'Not Found'];
    $payload = json_encode($body, JSON_PRETTY_PRINT);
    $response = implode("\r\n", [
        "HTTP/1.1 $status {$phrases[$status]}",
        'Content-Type: application/json',
        'Content-Length: ' . strlen($payload),
        'Connection: close',
        '',
        $payload,
    ]);
    socket_write($socket, $response);
    socket_close($socket);
}

function parseRequest(string $raw): array
{
    $lines   = explode("\r\n", $raw);
    $first   = explode(' ', array_shift($lines));
    $method  = $first[0] ?? 'GET';
    $path    = $first[1] ?? '/';
    $headers = [];
    foreach ($lines as $line) {
        if (str_contains($line, ':')) {
            [$key, $value]           = explode(':', $line, 2);
            $headers[strtolower(trim($key))] = trim($value);
        }
    }
    return compact('method', 'path', 'headers');
}

// --- Server loop ---

$server = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
socket_set_option($server, SOL_SOCKET, SO_REUSEADDR, 1);
socket_bind($server, '0.0.0.0', $port);
socket_listen($server);

$ip = localIp();
echo "Time Cube mock server running on http://0.0.0.0:$port\n";
echo "Set endpoint base URL in cube config to: http://$ip:$port\n";
echo $token ? "Expecting token: $token\n" : "Auth: disabled\n";
echo "\n";

while (true) {
    $client = socket_accept($server);
    $raw    = socket_read($client, 2048);

    ['method' => $method, 'path' => $path, 'headers' => $headers] = parseRequest($raw);

    // Auth check
    if ($token !== null && ($headers['x-time-cube-token'] ?? '') !== $token) {
        logRequest($method, $path, 401, 'unauthorized');
        send($client, 401, ['error' => 'unauthorized']);
        continue;
    }

    // GET /cube-config
    if ($method === 'GET' && $path === '/cube-config') {
        logRequest('GET', $path, 200, 'returned cube config');
        send($client, 200, CUBE_CONFIG);
        continue;
    }

    // POST /start/<task_id>  or  POST /stop/<task_id>
    if ($method === 'POST' && preg_match('#^/(start|stop)/(\d+)$#', $path, $m)) {
        $action = $m[1];
        $taskId = (int) $m[2];
        $name   = $GLOBALS['taskNames'][$taskId] ?? 'unknown';
        $verb   = $action === 'start' ? '▶ Started' : '■ Stopped';
        logRequest('POST', $path, 200, "$verb task $taskId \"$name\"");
        send($client, 200, ['ok' => true, 'task_id' => $taskId, 'action' => $action]);
        continue;
    }

    logRequest($method, $path, 404);
    send($client, 404, ['error' => 'not found']);
}
