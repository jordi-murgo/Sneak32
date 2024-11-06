#include "BLESettings.h"
#include <sstream>
#include <Arduino.h>
#include <BLEDevice.h>
#include "WifiScan.h"
#include "WifiDetect.h"
#include "BLEScan.h"
#include "BLEDetect.h"

extern AppPreferencesData appPrefs;
extern void saveAppPreferences();

void SettingsCallbacks::onWrite(BLECharacteristic *pCharacteristic)
{
    std::string value = pCharacteristic->getValue();
    std::istringstream ss(value);
    std::string token;

    uint8_t operation_mode = appPrefs.operation_mode;
    uint8_t cpu_speed = appPrefs.cpu_speed;
    bool led_mode = appPrefs.led_mode;
    int8_t wifi_tx_power = appPrefs.wifiTxPower;
    int8_t ble_tx_power = appPrefs.bleTxPower;

    Serial.printf("SettingsCallbacks::onWrite -> %s\n", value.c_str());

    // Read values
    if (std::getline(ss, token, '|'))
        appPrefs.only_management_frames = std::stoi(token) ? true : false;
    if (std::getline(ss, token, '|'))
        appPrefs.minimal_rssi = std::stoi(token);
    if (std::getline(ss, token, '|'))
        appPrefs.wifi_channel_dwell_time = std::stoul(token);
    if (std::getline(ss, token, '|'))
        appPrefs.ble_scan_delay = std::stoul(token);
    if (std::getline(ss, token, '|'))
        appPrefs.ignore_random_ble_addresses = std::stoi(token) ? true : false;
    if (std::getline(ss, token, '|'))
        appPrefs.ble_scan_duration = std::stoul(token);
    if (std::getline(ss, token, '|'))
        appPrefs.operation_mode = std::stoi(token);
    if (std::getline(ss, token, '|'))
        appPrefs.passive_scan = std::stoi(token) ? true : false;
    if (std::getline(ss, token, '|'))
        appPrefs.stealth_mode = std::stoi(token) ? true : false;
    if (std::getline(ss, token, '|'))
        appPrefs.autosave_interval = std::stoul(token);
    if (std::getline(ss, token, '|'))
        strncpy(appPrefs.authorized_address, token.c_str(), sizeof(appPrefs.authorized_address) - 1);
    if (std::getline(ss, token, '|'))
        appPrefs.cpu_speed = std::stoi(token);
    if (std::getline(ss, token, '|'))
        appPrefs.led_mode = std::stoi(token);
    if (std::getline(ss, token, '|'))
        appPrefs.wifiTxPower = std::stoi(token);
    if (std::getline(ss, token, '|'))
        appPrefs.bleTxPower = std::stoi(token);
    if (std::getline(ss, token, '|'))
        appPrefs.bleMTU = std::stoi(token);

    // Device name is now the last element
    if (std::getline(ss, token))
    {
        char olddevice_name[32];
        strncpy(olddevice_name, appPrefs.device_name, sizeof(olddevice_name) - 1);
        olddevice_name[sizeof(olddevice_name) - 1] = '\0';

        strncpy(appPrefs.device_name, token.c_str(), sizeof(appPrefs.device_name) - 1);
        appPrefs.device_name[sizeof(appPrefs.device_name) - 1] = '\0';

        if (strcmp(olddevice_name, appPrefs.device_name) != 0)
        {
            updateBLEDeviceName(appPrefs.device_name);
        }   
    }

    if (wifi_tx_power != appPrefs.wifiTxPower)
    {
        esp_wifi_set_max_tx_power((wifi_power_t) appPrefs.wifiTxPower);
    }

    if (ble_tx_power != appPrefs.bleTxPower)
    {
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, (esp_power_level_t) appPrefs.bleTxPower);
    }

    if (operation_mode != appPrefs.operation_mode)
    {
        switch (appPrefs.operation_mode)
        {
        case OPERATION_MODE_SCAN:
            // Deinitialize WifiDetect and BLEDetect
            WifiDetector.stop();
            BLEDetector.stop();

            // Initialize WifiScan and BLE Scan
            WifiScanner.setup();
            BLEScanner.setup();
            break;

        case OPERATION_MODE_DETECTION:
            // Deinitialize WifiScan and BLE Scan
            WifiScanner.stop();
            BLEScanner.stop();
            // Initialize WifiDetection and BLE Detect
            WifiDetector.setup();
            BLEDetector.setup();
            break;

        default: // OPERATION_MODE_OFF
            // Deinitialize WifiScan
            WifiScanner.stop();
            BLEScanner.stop();
            // Deinitialize WifiDetection
            WifiDetector.stop();
            BLEDetector.stop();
            break;
        }
    }

    saveAppPreferences();
}

void SettingsCallbacks::onRead(BLECharacteristic *pCharacteristic)
{
    // Create the settings string
    String settingsString = String(appPrefs.only_management_frames) + "|" +
                            String(appPrefs.minimal_rssi) + "|" +
                            String(appPrefs.wifi_channel_dwell_time) + "|" +
                            String(appPrefs.ble_scan_delay) + "|" +
                            String(appPrefs.ignore_random_ble_addresses) + "|" +
                            String(appPrefs.ble_scan_duration) + "|" +
                            String(appPrefs.operation_mode) + "|" +
                            String(appPrefs.passive_scan) + "|" +
                            String(appPrefs.stealth_mode) + "|" +
                            String(appPrefs.autosave_interval) + "|" +
                            String(appPrefs.authorized_address) + "|" +
                            String(appPrefs.cpu_speed) + "|" +
                            String(appPrefs.led_mode) + "|" +
                            String(appPrefs.wifiTxPower) + "|" +
                            String(appPrefs.bleTxPower) + "|" +
                            String(appPrefs.bleMTU) + "|" +
                            String(appPrefs.device_name);
    pCharacteristic->setValue(settingsString.c_str());
}

void SettingsCallbacks::updateBLEDeviceName(const char *newName)
{
    Serial.printf("Updating BLE device name to: %s\n", newName);

    // Stop advertising
    BLEDevice::stopAdvertising();

    // Change the device name
    esp_ble_gap_set_device_name(newName);

    // Restart advertising
    BLEDevice::startAdvertising();

    Serial.println("BLE device name updated and advertising restarted");
}
