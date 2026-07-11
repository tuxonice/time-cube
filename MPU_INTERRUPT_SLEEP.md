# MPU Interrupt-Based Sleep Mode

This feature adds power-efficient sleep mode to the Time Cube using MPU-6050 motion detection interrupts.

## Quick Setup

1. **Hardware**: Connect MPU-6050 INT pin to ESP32 GPIO34
2. **Configuration**: Set `enableSleepMode = true` (default)
3. **Upload**: Flash the firmware
4. **Test**: Device should sleep after 5 seconds of stability

That's it! The feature is enabled by default with optimal settings.

## Overview

The Time Cube can now enter deep sleep mode when stable, waking only when motion is detected. This significantly reduces power consumption while maintaining responsive face change detection.

## How It Works

1. **Normal Operation**: Continuously monitors MPU for face changes
2. **Stability Detection**: When a stable face is detected and no motion occurs for 5 seconds
3. **Sleep Mode**: ESP32 enters deep sleep, consuming minimal power
4. **Motion Wake-up**: MPU interrupt wakes the ESP32 when motion is detected
5. **Face Detection**: Device reads sensors, determines new face, posts to network
6. **Return to Sleep**: After processing, returns to sleep mode

## Hardware Setup

### Required Connection
Connect the MPU-6050 INT pin to ESP32 GPIO34 (default):

```
MPU-6050 INT pin → ESP32 GPIO34
```

### Pin Configuration
- Default interrupt pin: GPIO34
- Configurable via `interruptPin` setting
- GPIO34 is recommended as it's an input-only pin suitable for wake-up

## Configuration Options

Add these settings to your configuration:

```cpp
struct Configuration {
  // ... existing settings ...
  int interruptPin = 34;        // GPIO pin for MPU interrupt
  bool enableSleepMode = true;  // Enable/disable sleep mode
  int motionThreshold = 20;     // Motion sensitivity (1-255)
  int sleepDelayMs = 5000;      // Delay before sleep (ms)
};
```

### Configuration Details

- **interruptPin**: GPIO pin connected to MPU INT pin
- **enableSleepMode**: Toggle sleep mode on/off
- **motionThreshold**: Motion detection sensitivity (lower = more sensitive)
- **sleepDelayMs**: Time to wait after stability before sleep

## Power Savings

- **Active Mode**: Continuous polling (~50mA)
- **Deep Sleep Mode**: Minimal power (~10μA)
- **Battery Life**: Extended by 100-1000x depending on usage

## MPU-6050 Register Configuration

The feature configures the following MPU registers:

- **0x1F (MOT_THR)**: Motion detection threshold
- **0x20 (MOT_DUR)**: Motion detection duration (1ms)
- **0x38 (INT_ENABLE)**: Enable motion interrupt
- **0x37 (INT_PIN_CFG)**: Interrupt pin behavior
- **0x69 (ACCEL_INTEL_CONFIG)**: Motion detection on all axes

## Interrupt Handling

### Motion Detection
- Motion on X, Y, or Z axis triggers interrupt
- Configurable threshold prevents false triggers
- Interrupt is latched until cleared

### Wake-up Process
1. ESP32 wakes from deep sleep
2. Interrupt flag is cleared
3. MPU sensors are read
4. Face detection is performed
5. Network notification is sent
6. Device returns to sleep

## Usage Examples

### Enable Sleep Mode
```cpp
systemConfiguration.enableSleepMode = true;
systemConfiguration.sleepDelayMs = 3000;  // 3 seconds
systemConfiguration.motionThreshold = 15; // More sensitive
```

### Disable Sleep Mode
```cpp
systemConfiguration.enableSleepMode = false;
```

### Custom Interrupt Pin
```cpp
systemConfiguration.interruptPin = 26;  // Use GPIO26 instead
```

## Troubleshooting

### Device Not Waking
- Check MPU INT pin connection
- Verify interrupt pin configuration
- Ensure motion threshold is appropriate

### Too Many False Wake-ups
- Increase `motionThreshold` value
- Check for mechanical vibrations
- Ensure stable mounting

### Not Entering Sleep Mode
- Verify `enableSleepMode` is true
- Check if device is detecting motion
- Increase `sleepDelayMs` if needed

## Technical Details

### Sleep Mode Current Consumption
- Deep sleep: ~10μA
- Wake-up time: ~2ms
- Active processing: ~50mA for ~100ms

### Interrupt Response Time
- Motion detection: ~1ms
- Wake-up latency: ~2ms
- Total response: ~5ms

### MPU Configuration
- Sample rate: 1kHz
- Motion detection: All axes
- Interrupt: Active high, latched

## Compatibility

- **ESP32**: Supported
- **MPU-6050**: Supported
- **Existing Features**: Fully compatible
- **Network**: Maintains connectivity on wake

## Future Enhancements

- Configurable sleep delay per face
- Battery voltage monitoring
- Sleep statistics tracking
- Adaptive motion thresholds
