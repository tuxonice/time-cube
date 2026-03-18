# Time Cube

Time Cube is a physical time-tracking device built from a cube. Each of its six faces represents a different task. When the user places the cube with a specific face pointing upward, the device automatically starts tracking time for the corresponding task through a backend API. Turning the cube switches tasks instantly.

The cube uses an ESP32 microcontroller paired with an MPU6050 accelerometer/gyroscope to detect orientation. Task mappings are fetched dynamically from a backend service, allowing reconfiguration without reflashing firmware.

## Features

- Six task-specific faces, each with a unique [ColorADD](https://www.coloradd.net/) color and symbol
- Solid wooden prototype with internal hardware
- Real-time face orientation detection using MPU6050
- Automatic API calls on task change (start new task, stop previous task)
- Dynamic task configuration fetched from backend (`GET /cube-config`)
- WiFi-connected ESP32 firmware

## How It Works

1. On boot, the ESP32 connects to WiFi.
2. It fetches the color → task mapping from the backend: `GET /cube-config`
3. The MPU6050 reads device orientation continuously.
4. The firmware determines which cube face is currently facing up.
5. When the face changes, the previous task is stopped and the new task starts automatically.

This creates a physical, tactile time-tracking experience without screens, buttons, or apps.

## Hardware

- ESP32 Dev Module
- MPU6050 (I2C)
- Wooden cube housing (7–10 cm recommended)
- Internal mounting frame for stable orientation alignment

**Optional:**
- RGB LED
- Small buzzer / vibration motor
- Battery pack

## Backend

The project uses (and will later extend) [Time Logger](https://github.com/tuxonice/time-logger).

The backend must expose:

| Endpoint | Description |
|---|---|
| `GET /cube-config` | Returns task mapping per color |
| `POST /start/<task_id>` | Starts a task |
| `POST /stop/<task_id>` | Stops a task |

Example `GET /cube-config` response:

```json
{
  "faces": {
    "red":    { "task_id": 101, "name": "Email" },
    "blue":   { "task_id": 102, "name": "Coding" },
    "yellow": { "task_id": 103, "name": "Meetings" },
    "green":  { "task_id": 104, "name": "Research" },
    "orange": { "task_id": 105, "name": "Break" },
    "white": { "task_id": 106, "name": "Admin" }
  }
}
```

## Firmware Overview

The ESP32 firmware handles:

- WiFi setup
- Fetching cube configuration from backend
- Reading MPU6050 accelerometer/gyro data
- Determining the active face via orientation math
- Debouncing and stability filtering
- Sending HTTP requests on task changes

Orientation is mapped → face → color → task ID.

### Face Mapping

Each face is statically associated with a color in firmware; tasks are resolved dynamically at startup.

```cpp
const char* faceColors[6] = {
  "blue",
  "yellow",
  "red",
  "green",
  "orange",
  "purple"
};
```

## Project Goals

- Build a playful, physical time-tracking tool
- Create a low-friction way to switch between tasks
- Explore tangible interfaces and IoT workflows
- Enable customizable task management via backend

## Future Enhancements

- Battery operation + charging dock
- LED feedback for active task
- Dashboard for visualizing time logs
- MQTT support
- Pick-up detection (pause when cube is lifted)
