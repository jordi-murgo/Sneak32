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
    Serial.printf(" - min_rssi: %d dBm\n", appPrefs.minimal_rssi);
    Serial.printf(" - operation_mode: %s\n", operationModeName.c_str());
    Serial.printf(" - loop_delay: %u ms\n", appPrefs.loop_delay);
    Serial.printf(" - only_mgmt: %s\n", appPrefs.only_management_frames ? "true" : "false");
    Serial.printf(" - ble_scan_period: %u ms\n", appPrefs.ble_scan_period);
    Serial.printf(" - ignore_random_ble: %s\n", appPrefs.ignore_random_ble_addresses ? "true" : "false");
    Serial.printf(" - ble_scan_duration: %u s\n", appPrefs.ble_scan_duration);
}

void loadAppPreferences() {
    Serial.println("Loading App Preferences");
    preferences.begin("wifi_monitor", false);

    // Default spyduino name if not set (fist time)
    uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_BT);
    char default_name[32] = {0};
    snprintf(default_name, sizeof(default_name), "Spyduino (%02X%02X)", mac[4], mac[5]);
    String defaultName = String(default_name);
    String savedName = preferences.getString("device_name", defaultName);

    strncpy(appPrefs.device_name, savedName.c_str(), sizeof(appPrefs.device_name) - 1);
    appPrefs.device_name[sizeof(appPrefs.device_name) - 1] = '\0';

    appPrefs.operation_mode = preferences.getInt("op_mode", 1);
    appPrefs.minimal_rssi = preferences.getInt("min_rssi", -80);

    appPrefs.only_management_frames = preferences.getBool("only_mgmt", true);
    appPrefs.loop_delay = preferences.getUInt("loop_delay", 1000);

    appPrefs.ble_scan_period = preferences.getUInt("ble_scan_period", 30000);
    appPrefs.ignore_random_ble_addresses = preferences.getBool("ignore_random", false);
    appPrefs.ble_scan_duration = preferences.getUInt("ble_scan_dur", 15);

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
    preferences.end();

    Serial.println("App Preferences saved");
    printPreferences();

}
