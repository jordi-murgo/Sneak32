#ifndef APP_PREFERENCES_H
#define APP_PREFERENCES_H

#include <Preferences.h>
#include <Arduino.h>

// Estructura para almacenar las preferencias
struct AppPreferencesData {
    // General
    char device_name[32];
    uint8_t operation_mode;             // 0-OFF, 1-SCAN_MODE, 2-DETECTION_MODE
    uint32_t autosave_interval;
    int8_t minimal_rssi;
    bool passive_scan;
    bool stealth_mode;
    // Wifi
    bool only_management_frames;
    uint32_t loop_delay;
    // BLE
    uint32_t ble_scan_period;
    bool ignore_random_ble_addresses;  
    uint32_t ble_scan_duration;   
    char authorized_address[18];
};

const int8_t OPERATION_MODE_OFF = 0;
const int8_t OPERATION_MODE_SCAN = 1;
const int8_t OPERATION_MODE_DETECTION = 2;

// Namespace y Keys para NVS
namespace Keys {
    const char* const NAMESPACE = "wifi_monitor";
    const char* const DEVICE_NAME = "device_name";
    const char* const OP_MODE = "op_mode";
    const char* const MIN_RSSI = "min_rssi";
    const char* const ONLY_MGMT = "only_mgmt";
    const char* const LOOP_DELAY = "loop_delay";
    const char* const BLE_SCAN_PERIOD = "ble_period";
    const char* const IGNORE_RANDOM = "ignore_rand";
    const char* const BLE_SCAN_DUR = "ble_dur";
    const char* const AUTOSAVE_INT = "autosave_int";
    const char* const PASSIVE_SCAN = "passive";
    const char* const STEALTH_MODE = "stealth";
    const char* const AUTH_ADDR = "auth_addr";
}

// Declaraciones de funciones
void initAppPreferences();
void loadAppPreferences();
void saveAppPreferences();

// Variables externas
extern AppPreferencesData appPrefs;
extern Preferences preferences;

#endif // APP_PREFERENCES_H
