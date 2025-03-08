# Sneak32 - ESP32 WiFi/BLE Scanner Development Guide

## Build Commands
```bash
# Build project with default environment (esp32-s3)
pio run

# Upload to device
pio run -t upload

# Build and upload specific environment
pio run -e esp32-c3-supermini --upload-port /dev/cu.usbmodem101 -t upload

# Monitor serial output
pio device monitor
```

## Code Style Guidelines
- **Naming**: Classes use PascalCase (BLEDeviceManager), methods use camelCase (processPacket)
- **Headers**: Use include guards (#ifndef CLASS_H) or #pragma once
- **Memory**: Use DynamicPsramAllocator for large collections on ESP32
- **Comments**: Document classes with brief description and parameters
- **Error Handling**: Use try/catch blocks for recoverable errors, log with log_e/log_i
- **Logging**: Use ESP log levels (log_i, log_e, log_w, log_d) with appropriate priority
- **Tasks**: Each major component should run in its own FreeRTOS task
- **Synchronization**: Use queues for inter-task communication, mutex for data protection

## FreeRTOS Guidelines
- Task priorities: Use consistent priorities (WiFi > BLE > Processing > UI)
- Stack sizes: Minimum 4096 bytes for most tasks
- Queues: Use typesafe struct-based message queues
- Task notification: For simple signaling between tasks
- Consider task pinning to specific cores for performance-critical tasks

## Project Structure
- `src/`: C++ source files
- `docs/`: Web interface files
- `scripts/`: Build automation
- `include/`: Header files for external libraries
- `lib/`: External library dependencies