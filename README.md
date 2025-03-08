# Sneak32 - ESP32-based WiFi & BLE Scanner and Detector

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/version-0.2-blue.svg)](https://github.com/jordi-murgo/Sneak32/releases)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/jordi-murgo/Sneak32)

## 📚 Table of Contents

- [Overview](#-overview)
- [Application in Infiltrator Detection](#-application-in-infiltrator-detection)
- [Modes of Operation](#-modes-of-operation)
  - [Core Operation Phases](#core-operation-phases)
  - [Stealth Mode Options](#stealth-mode-options)
  - [Mode Selection Guide](#mode-selection-guide)
- [Features](#-features)
- [Task Architecture](#-task-architecture)
  - [Task Overview](#task-overview)
  - [Task Communication](#task-communication)
  - [Memory Management](#memory-management)
  - [Task Priorities](#task-priorities)
- [BLE Communication](#-ble-communication)
  - [Service UUID](#service-uuid)
  - [Characteristic UUIDs](#characteristic-uuids)
  - [BLE Data Protocol](#ble-data-protocol)
- [Hardware](#-hardware)
  - [Compatibility](#compatibility)
  - [Requirements](#requirements)
  - [Antenna Considerations](#antenna-considerations)
- [Software Dependencies](#-software-dependencies)
- [Setup and Configuration](#-setup-and-configuration)
- [Data Output](#-data-output)
- [Usage Strategies](#️-usage-strategies-in-sensitive-environments)
- [Security Considerations](#-security-considerations)
- [Future Improvements](#-future-improvements)
- [Contributing](#-contributing)
- [License](#-license)
- [Author](#-author)
- [Version History](#-version-history)
- [Contact and Support](#-contact-and-support)

## 🥷 Overview

Sneak32 is an advanced WiFi and Bluetooth Low Energy (BLE) scanning and detection tool built on the ESP32 platform. It offers powerful capabilities for network analysis, device tracking, and security research.

## 🎯 Application in Infiltrator Detection

Sneak32 is ideal for detecting devices that may be associated with surveillance or infiltration in social movements or sensitive environments. The tool allows you to capture WiFi and BLE device data in one location (RECON phase) and then analyze connections in your secure space (DEFENSE phase) to identify suspicious presences.

## 🔍 Modes of Operation

### Core Operation Phases

#### 1. RECON Phase
- **Purpose:** Capture and collect data in potentially hostile or unsecured environments
- **Capabilities:**
  - Captures WiFi data:
    - SSIDs from Beacons and Probe Requests
    - Client MAC addresses
    - Management, control, and data frames
  - Captures BLE device advertisements
- **Scanning Modes:**
  - **Passive Scan:** 
    - WiFi: Only listens for beacons and probe requests
    - BLE: Only listens for advertisements
    - No active requests sent
  - **Active Scan:**
    - WiFi: Sends probe requests to discover hidden networks
    - BLE: Performs active scanning
    - Maximizes data collection but increases visibility
  - **Stealth Mode:** 
    - Complete radio silence except for essential operations
    - No response to BLE scans (except last paired device)
    - Minimal RF footprint for maximum concealment

#### 2. DEFENSE Phase
- **Purpose:** Monitor secure spaces for previously identified suspicious devices
- **Capabilities:**
  - Monitors for specific WiFi/BLE MAC addresses
  - Detects devices based on their WiFi probe requests
  - Can force device visibility through SSID broadcasting
- **Operation Modes:**
  - **Passive Defense:**
    - Silent monitoring of all communications
    - No active scanning or broadcasting
    - Pure listening mode for maximum stealth
  - **Active Defense:**
    - Broadcasts known suspicious SSIDs to trigger auto-connect attempts
    - Performs active BLE scanning to detect target devices
    - Forces suspicious devices to reveal themselves
  - **Stealth Mode:**
    - Complete radio silence except for essential operations
    - No response to BLE scans (except last paired device)
    - Pure passive monitoring

### Mode Selection Guide

Choose your operation mode based on:
1. **Mission Phase:**
   - RECON: For initial data gathering in hostile environments
   - DEFENSE: For monitoring secure spaces
   
2. **Stealth Requirements:**
   - **Stealth Mode:** When absolute concealment is critical
     - No BLE responses (except to last paired device)
     - Minimal RF emissions
     - Maximum concealment
   
3. **Scanning Intensity:**
   - **Passive:** When stealth is important but full radio silence isn't required
     - No active requests
     - Only listens for broadcasts
   - **Active:** When maximum data collection is priority
     - Sends probe/scan requests
     - Broadcasts known SSIDs (in DEFENSE)
     - Higher chance of detection

4. **Risk vs Data Collection:**
   - Higher scanning intensity = More data but increased detection risk
   - Lower scanning intensity = Less data but better concealment

## 🚀 Features

- **WiFi Capabilities:**
  - Passive monitoring of networks and devices
  - Active scanning with probe requests (in Active mode)
  - Management, control, and data frame capture
  - SSID broadcasting for target device detection (in Active DEFENSE)
  - MAC address filtering and classification

- **BLE Capabilities:**
  - Passive advertisement monitoring
  - Active scanning (configurable)
  - Selective scan response (Stealth mode: only to last paired device)
  - Public/Random address classification
  - RSSI-based proximity detection

- **Customizable Settings:**
  - Independent WiFi/BLE scan intervals
  - RSSI threshold filtering
  - Operation mode selection
  - Target device/network lists
  - Auto-configuration options

- **Data Management:**
  - JSON-formatted output
  - Configurable data filtering
  - Flash storage with persistence
  - Adjustable save intervals
  - Data export capabilities

- **User Interface:**
  - Web-based configuration panel
  - Real-time status monitoring
  - Device control interface
  - Event logging system
  - BLE-based secure communication

## 🧩 Task Architecture

### Task Overview

Sneak32 uses a multi-task architecture based on FreeRTOS to efficiently manage its various operations. The system is designed with a modular approach where each major function runs in its own dedicated task with appropriate priorities and stack sizes.

**Core Tasks:**

1. **WiFi Scan Task**
   - **Purpose**: Captures WiFi packets in promiscuous mode
   - **Operation**: Processes management, control, and data frames
   - **Priority**: High (5) - Ensures real-time packet capture
   - **Stack Size**: 4096 bytes
   - **Implementation**: Uses ESP32's WiFi promiscuous mode callback system

2. **BLE Scan Task**
   - **Purpose**: Performs BLE device scanning
   - **Operation**: Periodically scans for BLE advertisements
   - **Priority**: High (4) - Ensures timely detection of BLE devices
   - **Stack Size**: 4096 bytes
   - **Implementation**: Uses a dedicated task loop with configurable intervals

3. **Device Management Task**
   - **Purpose**: Maintains device lists and handles data processing
   - **Operation**: Updates WiFi and BLE device records
   - **Priority**: Medium (3) - Processes captured data
   - **Stack Size**: 8192 bytes (larger due to PSRAM list management)
   - **Implementation**: Receives data from scan tasks via queues

4. **BLE Service Task**
   - **Purpose**: Manages BLE connectivity for remote control
   - **Operation**: Handles BLE server operations and client connections
   - **Priority**: Medium-Low (2) - UI-related operations
   - **Stack Size**: 4096 bytes
   - **Implementation**: Manages BLE characteristics and notifications

5. **Status Task**
   - **Purpose**: Updates system status and handles LED indicators
   - **Operation**: Periodically updates status information
   - **Priority**: Low (1) - Background operations
   - **Stack Size**: 2048 bytes
   - **Implementation**: Provides visual feedback and system monitoring

### Task Communication

Tasks communicate using FreeRTOS primitives to ensure thread safety and efficient data exchange:

1. **Message Queues**:
   - `wifiPacketQueue`: Transfers WiFi packet data (capacity: 50 messages)
   - `bleDeviceQueue`: Transfers BLE device information (capacity: 50 messages)
   - `commandQueue`: Handles command and control messages (capacity: 10 messages)

2. **Mutex Semaphores**:
   - `wifiListMutex`: Protects access to WiFi device and network lists
   - `bleListMutex`: Protects access to BLE device list
   - `flashMutex`: Ensures atomic access to flash storage operations

3. **Message Structures**:
   - `WifiPacketMessage`: Contains WiFi packet data, RSSI, channel, and frame type
   - `BLEDeviceMessage`: Contains BLE device address, name, RSSI, and advertisement data
   - `CommandMessage`: Contains command codes and parameters for system control

### Memory Management

The system employs a dynamic memory allocation strategy based on available resources:

1. **PSRAM Utilization**:
   - Automatically detects and utilizes PSRAM if available
   - Adjusts list capacities based on memory availability:
     - With PSRAM: 255 WiFi stations, 200 networks, 100 BLE devices
     - Without PSRAM: 100 WiFi stations, 50 networks, 50 BLE devices

2. **Custom Allocators**:
   - Uses `DynamicPsramAllocator` for PSRAM-aware memory allocation
   - Ensures efficient memory usage for device lists

3. **Stack Sizing**:
   - Task stacks are sized according to their processing requirements
   - Device Management Task has larger stack (8192 bytes) for handling complex list operations

### Task Priorities

Tasks are assigned priorities based on their real-time requirements:

| Task                 | Priority | Stack Size | Core Affinity |
|----------------------|----------|------------|---------------|
| WiFi Scan Task       | 5        | 4096       | PRO_CPU (0)   |
| BLE Scan Task        | 4        | 4096       | PRO_CPU (0)   |
| Device Management    | 3        | 8192       | APP_CPU (1)   |
| Data Processing      | 2        | 4096       | APP_CPU (1)   |
| BLE Service          | 2        | 4096       | APP_CPU (1)   |
| Status Updates       | 1        | 2048       | APP_CPU (1)   |

## 📡 BLE Communication

Sneak32 utilizes Bluetooth Low Energy (BLE) for wireless communication with client applications. The BLE implementation follows a service-characteristic model that allows for efficient data exchange and device control.

### Service UUID

Sneak32 exposes a single primary service with the following UUID:

| Service Name | UUID                                 | Description                      |
|--------------|--------------------------------------|----------------------------------|
| Sneak32      | `81af4cd7-e091-490a-99ee-caa99032ef4e` | Main service for all functionality |

### Characteristic UUIDs

Within the main service, Sneak32 provides several characteristics for different functions:

| Characteristic Name | UUID    | Properties       | Description                                          |
|---------------------|---------|------------------|------------------------------------------------------|
| Firmware Info       | `0xFFE3` | Read             | Provides firmware version and device information     |
| Settings            | `0xFFE2` | Read, Write      | Allows reading and modifying device settings         |
| Data Transfer       | `0xFFE0` | Notify, Write, Indicate | Used for transferring scan data to client applications |
| Commands            | `0xFFE4` | Write            | Receives commands from client applications           |
| Status              | `0xFFE1` | Read, Notify     | Provides real-time status updates                    |
| Response            | `0xFFE5` | Read, Notify     | Returns responses to commands                        |

### BLE Data Protocol

Sneak32 uses a simple protocol for BLE communication:

1. **Commands**: Client applications can send text commands to the Commands characteristic (0xFFE4). Supported commands include:
   - `VERSION`: Returns firmware version information
   - `SCAN_START`: Starts BLE scanning
   - `SCAN_STOP`: Stops BLE scanning
   - `SET_NAME:[name]`: Changes the device name

2. **Responses**: After processing a command, Sneak32 sends a response through the Response characteristic (0xFFE5).

3. **Data Transfer**: Scan results are sent through the Data Transfer characteristic (0xFFE0) in JSON format. The data includes:
   - WiFi networks (SSIDs)
   - WiFi devices (stations)
   - BLE devices

4. **Status Updates**: The Status characteristic (0xFFE1) provides real-time updates about the device's operation state.

This priority scheme ensures that time-sensitive operations (packet capture) take precedence over background processing and UI updates.

### Watchdog Protection

The system implements task watchdog timers to prevent system lockups:

1. **Task Watchdog Timer (TWDT)**:
   - Monitors task execution and detects if tasks are stuck
   - Generates diagnostic information (core dumps) on failure
   - Helps identify and resolve potential deadlocks or infinite loops

2. **Defensive Programming**:
   - All tasks implement proper error handling and recovery mechanisms
   - Critical sections are protected with timeouts to prevent deadlocks
   - Resource initialization is verified before use

## 💻 Hardware

### Compatibility
Sneak32 has been extensively tested with the **ESP32-C3** board, a low-cost RISC-V microcontroller (less than $3), which offers:
- WiFi 802.11 b/g/n (2.4 GHz) and Bluetooth 5.0 LE
- 4MB Flash memory and 400KB SRAM
- 80 MHz (Low Power) and 160 MHz (High Performance) CPU clock

**Board Compatibility:**
- ✅ ESP32 (WROOM-32*/MINI-1/PICO-D4): Full support
- ✅ ESP32-S3: Full support (recommended for high performance & data capacity)
- ✅ ESP32-C3: Full support (recommended for low power consumption)
- ❌ ESP32-S2: Not supported (no BLE)
- ❌ ESP32-H2: Not supported (no WiFi)
- ❌ ESP32-C6: Not supported (no Arduino-PlatformIO framework support yet)

### Requirements
- ESP32 development board (recommended: ESP32-C3-SuperMini or variants with PCB/external antenna)
- (Optional) RGB LED for visual status indication
- (Optional) Battery for portable operation

### Antenna Considerations
Choose based on your specific needs:

1. **Ceramic Chip Antenna (ESP32-C3-SuperMini):**
   - ✅ Compact and low cost
   - ✅ Good for close-range monitoring
   - ❌ Limited range and capture capabilities

2. **PCB Antenna:**
   - ✅ Better range than ceramic
   - ✅ Still relatively compact
   - ❌ Not as powerful as external antenna

3. **External Antenna:**
   - ✅ Best range and data capture
   - ✅ Ideal for wide-area monitoring
   - ❌ Larger form factor
   - ❌ Additional components needed

## 📚 Software Dependencies

- [PlatformIO](https://platformio.org/)
- [Arduino core for ESP32](https://github.com/espressif/arduino-esp32)
- [ESP32 BLE library](https://github.com/nkolban/ESP32_BLE_Arduino)
- [WiFi library](https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi)
- [FreeRTOS](https://www.freertos.org/)
- (Optional) [Adafruit NeoPixel library](https://github.com/adafruit/Adafruit_NeoPixel)

## 🔧 Setup and Configuration

1. **Clone the repository:**

  ```bash
  git clone https://github.com/jordi-murgo/Sneak32.git
  cd Sneak32
  ```

2. **Install necessary dependencies:**

  - First install [PlatformIO CLI](https://platformio.org/install/cli) or [PlatformIO VSCode Extension](https://platformio.org/install/ide?install=vscode)

   ```bash
   pio lib install
   ```

3. **Compile and flash** the firmware to your ESP32 device:

   With PlatformIO:

    ```bash
    pio run -t upload
    ```

    Or selecting environment and serial port:

    ```bash
    pio run -e esp32-c3-supermini --upload-port /dev/cu.usbmodem101 -t upload
    ```

   With PlatfotmIO Visual Studio Code Extension, use the "Upload" button.

4. **Power on the device** and verify that the LED (if connected) indicates the correct status.

5. **Connect to the device via Bluetooth LE:**

   ![Sneak32 Manager](images/manager-device-info.png)

   - Use the Smeak32 Manager present in [docs directory](docs/index.html) or the deployed [Sneak32 Manager](https://jordi-murgo.github.io/Sneak32/)
   - Click "Search for Device" and select your Sneak32 device.

6. **Configure device options** through the interface:
   - Device name
   - Operation mode
   - Scan intervals and durations
   - Thresholds and filters

7. **Start scanning** and monitoring using the interface options.

## 📊 Data Output

Sneak32 provides detailed JSON output including:

- **Detected WiFi networks:**
  - SSIDs, channels, signal strength, network type, times seen, last seen time.

- **WiFi devices:**
  - MAC addresses, signal strength, channel, times seen, last seen time.

- **BLE devices:**
  - Names (if available), MAC addresses, public/private status, signal strength, times seen, last seen time.

- **Example format:**

```json
  {
    "wifi_networks": [
      {
        "ssid": "MyWiFiNetwork",
        "rssi": -45,
        "channel": 6,
        "type": "beacon",
        "times_seen": 5,
        "last_seen": 1627845600
      }
    ],
    "wifi_devices": [
      {
        "mac_address": "A4:B1:C1:D1:E1:F1",
        "rssi": -50,
        "channel": 6,
        "times_seen": 10,
        "last_seen": 1627845620
      }
    ],
    "ble_devices": [
      {
        "name": "MyBLEDevice",
        "address": "00:1A:7D:DA:71:13",
        "is_public": true,
        "rssi": -60,
        "times_seen": 3,
        "last_seen": 1627845640
      }
    ]
  }
  ```

## ⚔️ Usage Strategies

- **Public Environment Monitoring**: Install Sneak32 in public meeting places to capture data from unknown devices and analyze connection patterns.
- **Social Movement Protection**: Use Sneak32 to detect devices attempting to connect to known networks or making suspicious probe requests, helping to identify potential infiltrators.
- **Research Applications**: Sneak32 allows security researchers to collect device data without being detected, thanks to its passive mode.

## 🕶 Security Considerations

**Warning:** This project is intended **solely** for educational purposes and authorized network analysis. Unauthorized use of network scanning tools may violate local laws and international regulations.

- **User Responsibility:** The developer is not responsible for misuse of this software.
- **Legal Compliance:** Always ensure you have explicit permission to scan and analyze networks and devices in your environment.
- **Privacy Concerns:** Be aware of privacy laws such as GDPR in the EU or CCPA in California when collecting and processing device data.
- **Ethical Use:** Consider the ethical implications of your actions. Respect individual privacy and obtain consent when necessary.

For more information on legal and ethical considerations in network scanning, refer to resources such as:

- [NIST Guidelines for Securing Wireless Local Area Networks](https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-153.pdf)
- [OWASP Wireless Security Testing Guide](https://owasp.org/www-project-wstg/)

## 📅 Version History

- **v0.0.1** (2024-10-01): Initial release.

## 📧 Contact and Support

If you have questions or need assistance, feel free to open an **issue** in the repository or contact the author directly.

---

*Note:* Ensure you use this tool ethically and legally, respecting the privacy and ownership of networks and devices in your environment.

## 🚀 Future Improvements

We are constantly working to enhance Sneak32. Here are some planned improvements for future versions:

1. **Performance Optimizations**
   - Improve scanning efficiency to reduce power consumption

2. **Extended Storage Capabilities**
   - Add support for external memory cards to increase storage capacity
   - Implement data management features for handling larger datasets

3. **Hardware Compatibility**
   - Verify and ensure compatibility with other ESP32* boards and models like ESP32-C6 (currently not supported by Espressif - Platform.IO - Arduino)
   - Support for LoRa32 boards
     - [LILYGO ® TTGO LoRa32](https://www.aliexpress.us/item/32872078587.html)
     - [Heltec LoRa32 ESP32 SX1262 LoRa](https://www.aliexpress.us/item/3256806616057872.html)
   - Support for othr communication boards ([NRF24L01](https://es.aliexpress.com/item/4000603343837.html))

4. **Communications**
   - Stealth communications using ESP-Now, NRF24L01, ZigBee and LoRa
   - Make a BLE Server proxy to LoRa (or LoRaWAN), NRF24L01, ZigBee, and ESP-Now.

We welcome contributions and suggestions from the community to help make these improvements a reality. If you have ideas or want to contribute, please check our [Contributing](#-contributing) section.

## 📝 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🤝 Contributing

Contributions are welcome!

## 👤 Author

**Jordi Murgo** - [@jordi-murgo](https://github.com/jordi-murgo)

Feel free to contact me for any questions or feedback.
