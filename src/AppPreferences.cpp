#include "AppPreferences.h"
#include <Arduino.h>

// For TX Power Default Values
#include <WiFi.h>
#include <esp_bt.h>


Preferences preferences;
AppPreferencesData appPrefs;

void printPreferences() {

    String operationModeName = "Unknown";
    switch (appPrefs.operation_mode) {
        case 0: operationModeName = "OFF"; break;
        case 1: operationModeName = "CAPTURE"; break;
        case 2: operationModeName = "DETECTION"; break;
    }

    Serial.printf(" - device_name: %s\n", appPrefs.device_name);
    Serial.printf(" - operation_mode: %s\n", operationModeName.c_str());
    Serial.printf(" - autosave_interval: %u min\n", appPrefs.autosave_interval);
    Serial.printf(" - minimal_rssi: %d dBm\n", appPrefs.minimal_rssi);
    Serial.printf(" - passive_scan: %s\n", appPrefs.passive_scan ? "true" : "false");
    Serial.printf(" - stealth_mode: %s\n", appPrefs.stealth_mode ? "true" : "false");
    Serial.printf(" - authorized_address: %s\n", appPrefs.authorized_address);
    Serial.printf(" - cpu_speed: %u\n", appPrefs.cpu_speed);
    Serial.printf(" - led_mode: %u\n", appPrefs.led_mode);

    Serial.printf(" - wifi_channel_dwell_time: %u ms\n", appPrefs.wifi_channel_dwell_time);
    Serial.printf(" - only_mgmt: %s\n", appPrefs.only_management_frames ? "true" : "false");
    Serial.printf(" - wifi_tx_power: %u\n", appPrefs.wifiTxPower);

    Serial.printf(" - ble_tx_power: %u\n", appPrefs.bleTxPower);
    Serial.printf(" - ble_scan_delay: %u s\n", appPrefs.ble_scan_delay);
    Serial.printf(" - ble_scan_duration: %u s\n", appPrefs.ble_scan_duration);
    Serial.printf(" - ignore_random_ble: %s\n", appPrefs.ignore_random_ble_addresses ? "true" : "false");
    Serial.printf(" - ble_mtu: %u\n", appPrefs.bleMTU);
}

void loadAppPreferences() {
    Serial.println("Loading App Preferences");
    preferences.begin(Keys::NAMESPACE, false);

    // Generate default device name
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    String chipModel = ESP.getChipModel();
    char default_name[32] = {0};
    snprintf(default_name, sizeof(default_name), "Sneak%s (%02X%02X)", 
             chipModel.c_str() + 3, mac[4], mac[5]);
    String defaultName = String(default_name);
    String savedName = preferences.getString(Keys::DEVICE_NAME, defaultName);

    strncpy(appPrefs.device_name, savedName.c_str(), sizeof(appPrefs.device_name) - 1);
    appPrefs.device_name[sizeof(appPrefs.device_name) - 1] = '\0';

    appPrefs.operation_mode = preferences.getInt(Keys::OP_MODE, 1); // 0-OFF, 1-SCAN_MODE, 2-DETECTION_MODE

    appPrefs.minimal_rssi = preferences.getInt(Keys::MIN_RSSI, -85);

    appPrefs.only_management_frames = preferences.getBool(Keys::ONLY_MGMT, true);
    appPrefs.wifi_channel_dwell_time = preferences.getUInt(Keys::WIFI_CHANNEL_DWELL_TIME, 1000);

    appPrefs.ble_scan_delay = preferences.getUInt(Keys::BLE_SCAN_DELAY, 30);
    appPrefs.ignore_random_ble_addresses = preferences.getBool(Keys::IGNORE_RANDOM, true);
    appPrefs.ble_scan_duration = preferences.getUInt(Keys::BLE_SCAN_DUR, 15);
    appPrefs.autosave_interval = preferences.getUInt(Keys::AUTOSAVE_INT, 60);
    appPrefs.passive_scan = preferences.getBool(Keys::PASSIVE_SCAN, false);
    appPrefs.stealth_mode = preferences.getBool(Keys::STEALTH_MODE, false);

    String savedAddress = preferences.getString(Keys::AUTH_ADDR, "");
    strncpy(appPrefs.authorized_address, savedAddress.c_str(), sizeof(appPrefs.authorized_address) - 1);
    appPrefs.authorized_address[sizeof(appPrefs.authorized_address) - 1] = '\0';

    appPrefs.cpu_speed = preferences.getUInt(Keys::CPU_SPEED, 80);
    appPrefs.led_mode = preferences.getBool(Keys::LED_MODE, true);
    appPrefs.wifiTxPower = preferences.getInt(Keys::WIFI_TX_POWER, WIFI_POWER_8_5dBm);
    appPrefs.bleTxPower = preferences.getInt(Keys::BLE_TX_POWER, ESP_PWR_LVL_P9);

    appPrefs.bleMTU = preferences.getUInt(Keys::BLE_MTU, 256);
    
    preferences.end();

    Serial.println("App Preferences loaded");
    printPreferences();
}

void saveAppPreferences() {
    Serial.println("Saving App Preferences");
    preferences.begin(Keys::NAMESPACE, false);
    preferences.putString(Keys::DEVICE_NAME, appPrefs.device_name);
    preferences.putInt(Keys::OP_MODE, appPrefs.operation_mode);
    preferences.putInt(Keys::MIN_RSSI, appPrefs.minimal_rssi);
    preferences.putBool(Keys::ONLY_MGMT, appPrefs.only_management_frames);
    preferences.putUInt(Keys::WIFI_CHANNEL_DWELL_TIME, appPrefs.wifi_channel_dwell_time);
    preferences.putUInt(Keys::BLE_SCAN_DELAY, appPrefs.ble_scan_delay);
    preferences.putBool(Keys::IGNORE_RANDOM, appPrefs.ignore_random_ble_addresses);
    preferences.putUInt(Keys::BLE_SCAN_DUR, appPrefs.ble_scan_duration);
    preferences.putUInt(Keys::AUTOSAVE_INT, appPrefs.autosave_interval);
    preferences.putBool(Keys::PASSIVE_SCAN, appPrefs.passive_scan);
    preferences.putBool(Keys::STEALTH_MODE, appPrefs.stealth_mode);
    preferences.putString(Keys::AUTH_ADDR, appPrefs.authorized_address);
    preferences.putUInt(Keys::CPU_SPEED, appPrefs.cpu_speed);
    preferences.putBool(Keys::LED_MODE, appPrefs.led_mode);
    preferences.putInt(Keys::WIFI_TX_POWER, appPrefs.wifiTxPower);
    preferences.putInt(Keys::BLE_TX_POWER, appPrefs.bleTxPower);
    preferences.putUInt(Keys::BLE_MTU, appPrefs.bleMTU);
    preferences.end();

    Serial.println("App Preferences saved");
    printPreferences();

}
