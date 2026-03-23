# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Time Cube is an IoT time-tracking device built on an **ESP32** microcontroller. A wooden cube has six colored faces; placing the cube with a specific face up automatically starts tracking time for the associated task via a backend API.

## Development Setup

This is an **Arduino sketch** project. There is no Makefile or build script — code is compiled and uploaded via:
- **Arduino IDE**: Open `time-cube.ino`, select "ESP32 Dev Module" board, upload
- **PlatformIO**: Compatible but no `platformio.ini` is present; one would need to be created

Required Arduino libraries:
- `WiFi.h`, `WebServer.h`, `Preferences.h` (ESP32 Arduino core)
- MPU6050 library (for I2C accelerometer/gyroscope)

## Architecture

### Hardware
- **ESP32** — WiFi + microcontroller
- **MPU6050** — I2C accelerometer/gyroscope for orientation detection
- 6 colored cube faces mapped to tasks using the **ColorADD** color system

### Firmware Flow (`time-cube.ino`)
1. Boot → load config from ESP32 NVS (namespace `"time-cube"`)
2. Try WiFi STA connection; on failure, fall back to AP mode (SSID: `"TIME-CUBE"`)
3. Serve config web UI on port 80 (`http://192.168.4.1/` in AP mode)
4. *(Not yet implemented)* Poll MPU6050 orientation → determine active face → call backend API

### Backend API (external, not in this repo)
The firmware expects these endpoints on a configured server:
- `GET /cube-config` — returns JSON task mapping per color face
- `POST /start/<task_id>` — start a task
- `POST /stop/<task_id>` — stop a task

Face-to-color mapping order (index 0–5 = face orientations):
```cpp
const char* faceColors[6] = { "blue", "yellow", "red", "green", "orange", "purple" };
```

### Configuration (NVS)
Stored keys: `wifi_ssid`, `wifi_pass`, `settle_time` (debounce interval in ms, default 4000)

### Web UI
The HTML template in `template/template.html` is served for configuration. It uses a Bootstrap-like layout with a form that POSTs to update WiFi credentials and settle time.

## Current Implementation Status

- **Done**: WiFi (STA + AP fallback), NVS config persistence, web config UI
- **TODO**: MPU6050 sensor reading, face orientation detection, backend API calls, task color→ID mapping
