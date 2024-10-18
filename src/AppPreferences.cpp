#include "AppPreferences.h"
#include <Arduino.h>

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
    Serial.printf(" - loop_delay: %u ms\n", appPrefs.loop_delay);
    Serial.printf(" - only_mgmt: %s\n", appPrefs.only_management_frames ? "true" : "false");
    Serial.printf(" - ble_scan_period: %u ms\n", appPrefs.ble_scan_period);
    Serial.printf(" - ble_scan_duration: %u s\n", appPrefs.ble_scan_duration);
    Serial.printf(" - ignore_random_ble: %s\n", appPrefs.ignore_random_ble_addresses ? "true" : "false");
}

void loadAppPreferences() {
    Serial.println("Loading App Preferences");
    preferences.begin("wifi_monitor", false);

    // Generate default device name
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    String chipModel = ESP.getChipModel();
    char default_name[32] = {0};
    snprintf(default_name, sizeof(default_name), "Sneak%s (%02X%02X)", 
             chipModel.c_str() + 3, mac[4], mac[5]);
    String defaultName = String(default_name);
    String savedName = preferences.getString("device_name", defaultName);

    strncpy(appPrefs.device_name, savedName.c_str(), sizeof(appPrefs.device_name) - 1);
    appPrefs.device_name[sizeof(appPrefs.device_name) - 1] = '\0';

    appPrefs.operation_mode = preferences.getInt("op_mode", 1); // 0-OFF, 1-SCAN_MODE, 2-DETECTION_MODE

    appPrefs.minimal_rssi = preferences.getInt("min_rssi", -85);

    appPrefs.only_management_frames = preferences.getBool("only_mgmt", true);
    appPrefs.loop_delay = preferences.getUInt("loop_delay", 1000);

    appPrefs.ble_scan_period = preferences.getUInt("ble_scan_period", 30000);
    appPrefs.ignore_random_ble_addresses = preferences.getBool("ignore_random", false);
    appPrefs.ble_scan_duration = preferences.getUInt("ble_scan_dur", 15);
    appPrefs.autosave_interval = preferences.getUInt("autosave_interval", 60);
    appPrefs.passive_scan = preferences.getBool("passive_scan", false);
    appPrefs.stealth_mode = preferences.getBool("stealth_mode", false);
    preferences.end();

    Serial.println("App Preferences loaded");
    printPreferences();
}

void saveAppPreferences() {
    Serial.println("Saving App Preferences");
    preferences.begin("wifi_monitor", false);

    preferences.putString("device_name", appPrefs.device_name);
    preferences.putInt("op_mode", appPrefs.operation_mode);
    preferences.putInt("min_rssi", appPrefs.minimal_rssi);
    preferences.putBool("only_mgmt", appPrefs.only_management_frames);
    preferences.putUInt("loop_delay", appPrefs.loop_delay);
    preferences.putUInt("ble_scan_period", appPrefs.ble_scan_period);
    preferences.putBool("ignore_random", appPrefs.ignore_random_ble_addresses);
    preferences.putUInt("ble_scan_dur", appPrefs.ble_scan_duration);
    preferences.putUInt("autosave_interval", appPrefs.autosave_interval);
    preferences.putBool("passive_scan", appPrefs.passive_scan);
    preferences.putBool("stealth_mode", appPrefs.stealth_mode);
    preferences.end();

    Serial.println("App Preferences saved");
    printPreferences();

}
