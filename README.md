# ESP32 BLE WiFi Scanner

## Overview

This project implements a WiFi scanner using an ESP32 microcontroller, which communicates the collected data via Bluetooth Low Energy (BLE). The scanner is capable of detecting WiFi networks and devices, providing information such as SSID, RSSI, channel, and device MAC addresses. The data can be accessed and configured through a web interface, making it a versatile tool for network analysis and monitoring.

## Features

- **WiFi Scanning**: Continuously scans WiFi channels for networks and devices.
- **BLE Communication**: Transmits collected data via BLE for low-power operation.
- **Configurable Settings**: Allows adjustment of scanning parameters through BLE.
- **Web Interface**: Provides a user-friendly interface for data visualization and device configuration.
- **Real-time Updates**: Delivers scanning results in real-time to connected clients.
- **Persistent Configuration**: Saves user settings to non-volatile memory.

## Hardware Requirements

- ESP32 development board
- (Optional) NeoPixel LED for status indication

## Software Dependencies

- Arduino core for ESP32
- ESP32 BLE library
- WiFi library
- FreeRTOS
- (Optional) Adafruit NeoPixel library

## Main Components

### WiFi Scanning

The core functionality revolves around WiFi scanning in promiscuous mode. The ESP32 cycles through WiFi channels, capturing various types of frames:

- Management frames (e.g., beacons, probe requests/responses)
- Control frames
- Data frames

### Data Structures

Two main data structures are used to store scanning results:

1. `WifiDevice`: Stores information about individual WiFi devices (stations).
2. `WifiNetwork`: Stores information about detected WiFi networks (SSIDs).

### BLE Communication

The project uses BLE to expose the following services and characteristics:

- Device Information Service
- Custom UART Service with characteristics for:
  - Data transfer
  - Configuration of scanning parameters

### Configuration Options

Users can configure the following parameters via BLE:

1. Only Probe Requests mode
2. Minimal RSSI threshold
3. Scanning loop delay

### LED Indication (Optional)

If enabled, a NeoPixel LED provides visual feedback on the device's status:

- Green: Device initialized
- Blue: BLE connected or data transmission in progress
- Off: Normal operation or BLE disconnected

## Key Functions

- `promiscuous_rx_cb`: Callback function for processing captured WiFi frames.
- `updateOrAddSSID` / `updateOrAddStation`: Functions to manage detected networks and devices.
- `generateJsonString`: Creates a JSON representation of the collected data.
- `setupBLE`: Initializes BLE services and characteristics.
- `loop`: Main program loop that manages WiFi channel hopping and status reporting.

## Configuration and Preferences

The project uses the ESP32's non-volatile storage to save and load user preferences, ensuring that settings persist across power cycles.

## Security Considerations

This project is designed for educational and network analysis purposes. Be aware of and comply with local laws and regulations regarding WiFi scanning and monitoring.

## Future Enhancements

Potential areas for improvement and expansion include:

- Advanced filtering options for captured data
- Integration with external databases for device identification
- Support for capturing and analyzing additional WiFi frame types
- Enhanced power management for longer battery life in portable scenarios

## Contributing

Contributions to this project are welcome. Please feel free to submit issues, feature requests, or pull requests to help improve the functionality and usability of this WiFi scanner.

## License

This project is licensed under the MIT License. See the LICENSE file for details.
