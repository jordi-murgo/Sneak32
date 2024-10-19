# Sneak32 - ESP32-based WiFi & BLE Scanner and Detector

## 🕵️‍♂️ Overview

Sneak32 is an advanced WiFi and Bluetooth Low Energy (BLE) scanning and detection tool built on the ESP32 platform. It offers powerful capabilities for network analysis, device tracking, and security research.

## 🎯 Application in Infiltrator Detection

Sneak32 is ideal for detecting devices that may be associated with surveillance or infiltration in social movements or sensitive environments. The tool allows you to capture WiFi and BLE device data in one location (RECON phase) and then analyze connections in your secure space (DETECTION phase) to identify suspicious presences.

## 🔍 Operation Modes

- **RECON Phase:**
  - Captures SSIDs, WiFi client MAC addresses, and BLE device advertisements in hostile or unsecured terrains.
  - Ideal for collecting data without drawing attention (passive mode) or forcing device association (active mode).

- **DETECTION Phase:**
  - Once data is collected in the RECON phase, Sneak32 can be used in your secure space to monitor for the presence of suspicious devices.
  - Uses passive or active scans to simulate networks and prompt devices to connect.

## 🚀 Features

- **Dual-Mode Operation:**
  - Scan Mode: Continuously scans for WiFi networks, devices, and BLE devices.
  - Detection Mode: Monitors for specific WiFi networks or devices.

- **WiFi Capabilities:**
  - Detects WiFi networks (SSIDs) and connected devices.
  - Captures management, control, and data frames.
  - Configurable to focus on management frames only.
  - Detects WiFi devices based on MAC address.

- **BLE Capabilities:**
  - Scans for nearby BLE devices.
  - Option to ignore random BLE addresses.
  - Detects specific BLE devices.

- **Customizable Settings:**
  - Adjustable scan intervals for WiFi and BLE.
  - Configurable RSSI threshold for signal filtering.
  - Passive scan option.
  - Stealth mode to minimize detectability.
  - Device name configuration based on chip model and MAC address.

- **Data Management:**
  - JSON data output for easy parsing and analysis.
  - Option to save all data or only relevant data.
  - Configurable autosave interval.
  - Flash storage for data persistence.

- **User Interface:**
  - Web-based interface (HTML/JavaScript) for configuration and data retrieval.
  - Real-time status updates.
  - Event logging for monitoring device activities.
  - Bluetooth LE connection with specific characteristics for commands, data transfer, and settings.

## 💻 Hardware and Software Compatibility

Sneak32 has been tested with the **ESP32-C3** board, a low-cost RISC-V microcontroller (less than $3), which offers:
- WiFi 802.11 b/g/n (2.4 GHz) and Bluetooth 5.0 LE.
- 4MB Flash memory and 400KB SRAM.
- Support for **Arduino IDE**, **PlatformIO**, and **Espressif IDF**, facilitating programming and customization of the device according to each project's needs.

## 🛠 Hardware Requirements

- ESP32 development board (tested on ESP32, ESP32-C3, ESP32-S3)
- (Optional) NeoPixel LED for status indication
- (Optional) Button for additional functions

## 📚 Software Dependencies
- PlatformIO
- Arduino core for ESP32
- ESP32 BLE library
- WiFi library
- FreeRTOS
- (Optional) Adafruit NeoPixel library

## 🔧 Setup and Configuration

1. **Clone the repository:**

  ```bash
  git clone https://github.com/jordi-murgo/Sneak32.git
  ```

2. **Install necessary dependencies** through Arduino IDE's Library Manager or PlatformIO.

3. **Compile and flash** the firmware to your ESP32 device.

4. **Power on the device** and verify that the LED (if connected) indicates the correct status.

5. **Connect to the device via Bluetooth LE:**
   - Use the provided web interface in `docs/index.html`.
   - Open the `index.html` file in a compatible browser (Chrome or Edge recommended).
   - The default `index.html` are also deployed and available at [https://jordi-murgo.github.io/Sneak32](https://jordi-murgo.github.io/Sneak32)
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

## ⚔️ Usage Strategies in Sensitive Environments

- **Public Environment Monitoring**: Install Sneak32 in public meeting places to capture data from unknown devices and analyze connection patterns.
- **Social Movement Protection**: Use Sneak32 to detect devices attempting to connect to known networks or making suspicious probe requests, helping to identify potential infiltrators.
- **Research Applications**: Sneak32 allows security researchers to collect device data without being detected, thanks to its passive mode.

## 🕶️ Operating Modes

- **Passive Mode**: Does not perform active scans or announce networks, simply listens to communications without being detected.
- **Active Mode**: Announces fake networks (SSIDs) to entice devices to connect, or performs "active scans" to obtain information from nearby BLE devices.

## 🔐 Security Considerations

**Warning:** This project is intended **solely** for educational purposes and authorized network analysis. Unauthorized use of network scanning tools may violate local laws and international regulations.

- **User Responsibility:** The developer is not responsible for misuse of this software.
- **Legal Compliance:** Always ensure you have explicit permission to scan and analyze networks and devices in your environment.

## 🤝 Contributing

Contributions to Sneak32 are welcome! If you'd like to improve Sneak32, please:

1. Fork the project.
2. Create a feature branch (`git checkout -b feature/new-feature`).
3. Make your changes and commit with descriptive messages.
4. Submit a pull request detailing your changes.

## 📄 License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## 👤 Author

Developed by **Jordi Murgó**  
Contact: [jordi.murgo@gmail.com](mailto:jordi.murgo@gmail.com)

## 📅 Version History

- **v0.2** (2024-08-15): Improvements in detection and web interface configuration.
- **v0.1** (2024-07-28): Initial release.

## 📧 Contact and Support

If you have questions or need assistance, feel free to open an **issue** in the repository or contact the author directly.

---

*Note:* Ensure you use this tool ethically and legally, respecting the privacy and ownership of networks and devices in your environment.
