#include "AppPreferences.h"
#include <Arduino.h>

// For TX Power Default Values
#include <WiFi.h>
#include <esp_bt.h>


Preferences preferences;
AppPreferencesData appPrefs;

void printPreferences() {
    log_i("Current preferences:");
    
    String operationModeName = "Unknown";
    switch (appPrefs.operation_mode) {
        case 0: operationModeName = "OFF"; break;
        case 1: operationModeName = "CAPTURE"; break;
        case 2: operationModeName = "DETECTION"; break;
    }

    log_i(" - device_name: %s", appPrefs.device_name);
    log_i(" - operation_mode: %s", operationModeName.c_str());
    log_i(" - autosave_interval: %u min", appPrefs.autosave_interval);
    log_i(" - minimal_rssi: %d dBm", appPrefs.minimal_rssi);
    log_i(" - passive_scan: %s", appPrefs.passive_scan ? "true" : "false");
    log_i(" - stealth_mode: %s", appPrefs.stealth_mode ? "true" : "false");
    log_i(" - authorized_address: %s", appPrefs.authorized_address);
    log_i(" - cpu_speed: %u", appPrefs.cpu_speed);
    log_i(" - led_mode: %u", appPrefs.led_mode);
    log_i(" - wifi_channel_dwell_time: %u ms", appPrefs.wifi_channel_dwell_time);
    log_i(" - only_mgmt: %s", appPrefs.only_management_frames ? "true" : "false");
    log_i(" - wifi_tx_power: %u", appPrefs.wifiTxPower);
    log_i(" - ignore_local_wifi_addresses: %s", appPrefs.ignore_local_wifi_addresses ? "true" : "false");
    log_i(" - ble_tx_power: %u", appPrefs.bleTxPower);
    log_i(" - ble_scan_delay: %u s", appPrefs.ble_scan_delay);
    log_i(" - ble_scan_duration: %u s", appPrefs.ble_scan_duration);
    log_i(" - ignore_random_ble: %s", appPrefs.ignore_random_ble_addresses ? "true" : "false");
    log_i(" - ble_mtu: %u", appPrefs.bleMTU);
}

void loadAppPreferences() {
    log_i("Loading App Preferences");
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

    appPrefs.only_management_frames = preferences.getBool(Keys::ONLY_MGMT, false);
    appPrefs.wifi_channel_dwell_time = preferences.getUInt(Keys::WIFI_CHANNEL_DWELL_TIME, 10000);
    appPrefs.ignore_local_wifi_addresses = preferences.getBool(Keys::IGNORE_LOCAL, true);

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

    log_i("App Preferences loaded");
    printPreferences();
}

void saveAppPreferences() {
    log_i("Saving App Preferences");
    preferences.begin(Keys::NAMESPACE, false);
    preferences.putString(Keys::DEVICE_NAME, appPrefs.device_name);
    preferences.putInt(Keys::OP_MODE, appPrefs.operation_mode);
    preferences.putInt(Keys::MIN_RSSI, appPrefs.minimal_rssi);
    preferences.putBool(Keys::ONLY_MGMT, appPrefs.only_management_frames);
    preferences.putBool(Keys::IGNORE_LOCAL, appPrefs.ignore_local_wifi_addresses);
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

    log_i("App Preferences saved");
    printPreferences();
}
