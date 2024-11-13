#include "BLEScan.h"
#include "BLEDeviceList.h"
#include "MACAddress.h"
#include "AppPreferences.h"

extern BLEDeviceList bleDeviceList;
extern AppPreferencesData appPrefs;

BLEScanClass BLEScanner;

BLEScanAdvertisedDeviceCallbacks::BLEScanAdvertisedDeviceCallbacks(BLEScanClass* scan) : scan(scan) {}

void BLEScanClass::setup() {
    Serial.println("Setting up BLE Scanner");
    pBLEScan = BLEDevice::getScan();
    callbacks = new BLEScanAdvertisedDeviceCallbacks(this);
    start();
    Serial.println("BLE Scanner setup complete");
}

void BLEScanClass::start() {
    Serial.println("Starting BLE Scan task");
    pBLEScan->setAdvertisedDeviceCallbacks(callbacks);
    pBLEScan->setActiveScan(true);
    if (!isScanning) {
        isScanning = true;
        xTaskCreatePinnedToCore(
            [](void* parameter) { static_cast<BLEScanClass*>(parameter)->scan_loop(); },
            "BLE_Scan_Task", 4096, this, 1, &scanTaskHandle, 0);
    }
    Serial.println("BLE Scan task started");
}

void BLEScanClass::stop() {
    Serial.println("Stopping BLE Scan task");
    pBLEScan->stop();
    pBLEScan->setAdvertisedDeviceCallbacks(nullptr);
    if (isScanning) {
        isScanning = false;
        vTaskDelete(scanTaskHandle);
        scanTaskHandle = nullptr;
        pBLEScan->stop();
    }
    Serial.println("BLE Scan task stopped");
}

void BLEScanClass::scan_loop() {
    Serial.println("ScanLoop started");
    while (true) {
        delay(appPrefs.ble_scan_delay * 1000);
        Serial.println("Starting BLE Scan");
        pBLEScan->setInterval(100);
        pBLEScan->setWindow(90);
        pBLEScan->setActiveScan(!appPrefs.passive_scan);
        BLEScanResults foundDevices = pBLEScan->start(appPrefs.ble_scan_duration, false);
        Serial.printf("BLE Scan complete. %d devices found.\n", foundDevices.getCount());
    }
}

void printHexDump(const uint8_t* data, size_t length) {
    char ascii[17];
    char formatted_ascii[17];

    for (size_t i = 0; i < length; i++) {
        // Print offset at the start of each line
        if (i % 16 == 0) {
            if (i != 0) {
                Serial.printf("  |%s|\n", formatted_ascii);
            }
            Serial.printf("%08x  ", i);
            // Initialize ascii buffer for new line
            memset(ascii, 0, sizeof(ascii));
            memset(formatted_ascii, ' ', sizeof(formatted_ascii) - 1);
            formatted_ascii[16] = '\0';
        }

        // Print hex value
        Serial.printf("%02x ", data[i]);
        
        // Store ASCII representation
        ascii[i % 16] = (data[i] >= 32 && data[i] <= 126) ? data[i] : '.';
        formatted_ascii[i % 16] = ascii[i % 16];

        // Print extra space between 8th and 9th bytes
        if ((i + 1) % 8 == 0 && (i + 1) % 16 != 0) {
            Serial.print(" ");
        }
    }

    // Print padding spaces if the last line is incomplete
    if (length % 16 != 0) {
        size_t padding = 16 - (length % 16);
        for (size_t i = 0; i < padding; i++) {
            Serial.print("   ");
            if ((length + i + 1) % 8 == 0 && (length + i + 1) % 16 != 0) {
                Serial.print(" ");
            }
        }
        Serial.printf("  |%s|\n", formatted_ascii);
    } else if (length > 0) {
        Serial.printf("  |%s|\n", formatted_ascii);
    }
}

void BLEScanAdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice advertisedDevice) {
    try {
        int rssi = advertisedDevice.getRSSI();

        if (rssi >= appPrefs.minimal_rssi) {
            // Move string operations outside of the critical section
            std::string addressStr = advertisedDevice.getAddress().toString();
            std::string nameStr = advertisedDevice.haveName() ? advertisedDevice.getName() : "";
            std::string appearanceStr = std::to_string(advertisedDevice.getAppearance());
            std::string uuidStr = advertisedDevice.haveServiceUUID() ? advertisedDevice.getServiceUUID().toString() : "";

            esp_bd_addr_t bleaddr;
            memcpy(bleaddr, advertisedDevice.getAddress().getNative(), sizeof(esp_bd_addr_t));

            boolean isPublic = false;
            String addressType;
            switch (advertisedDevice.getAddressType()) {
                case BLE_ADDR_TYPE_PUBLIC:
                    addressType = "public";
                    isPublic = true;
                    break;
                case BLE_ADDR_TYPE_RANDOM:
                    addressType = "random";
                    break;
                case BLE_ADDR_TYPE_RPA_PUBLIC:
                    addressType = "RPA-public";
                    isPublic = true;
                    break;
                case BLE_ADDR_TYPE_RPA_RANDOM:
                    addressType = "RPA-random";
                    break;
                default:
                    addressType = "unknown";
                    break;
            }

            // Base output always includes address and type
            Serial.printf("Address: %s (%s)", 
                addressStr.c_str(), addressType.c_str());

            // Optional fields only if they have value
            if (!nameStr.empty()) {
                Serial.printf(", Name: '%s'", nameStr.c_str());
            }

            uint16_t appearance = advertisedDevice.getAppearance();
            if (appearance != 0) {
                Serial.printf(", Appearance: %d", appearance);
            }

            if (advertisedDevice.haveServiceUUID()) {
                Serial.printf(", Service UUID: %s", uuidStr.c_str());
            }

            Serial.println(); // End the line

            // Print payload in hexdump format
            if (advertisedDevice.getPayloadLength() > 0) {
                Serial.println("Payload hexdump:");
                printHexDump(advertisedDevice.getPayload(), advertisedDevice.getPayloadLength());
            }

            if (addressStr == "00:00:00:00:00:00") {
                // This is an invalid address, ignore it
                return;
            }

            if (!isPublic && appPrefs.ignore_random_ble_addresses) {
                // This is a random BLE address, ignore it
                return;
            }

            // Use a lock_guard for exception-safe locking
            {
                std::lock_guard<std::mutex> lock(scan->mtx);

                if (!appPrefs.ignore_random_ble_addresses || isPublic) {
                    bleDeviceList.updateOrAddDevice(MacAddress(bleaddr), rssi, String(nameStr.c_str()), isPublic);
                }

            }
        }
    } catch (const std::exception &e) {
        Serial.printf("Exception in onResult: %s\n", e.what());
    } catch (...) {
        Serial.println("Unknown exception in onResult");
    }
}
