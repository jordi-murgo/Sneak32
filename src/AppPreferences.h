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
    uint32_t wifi_channel_dwell_time;
    uint8_t wifiTxPower;
    bool ignore_local_wifi_addresses;
    // BLE
    uint32_t ble_scan_delay;
    bool ignore_random_ble_addresses;  
    uint32_t ble_scan_duration;   
    char authorized_address[18];
    uint8_t bleTxPower;

    // CPU
    uint8_t cpu_speed;
    // LED
    uint8_t led_mode;

    uint16_t bleMTU;
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
    const char* const WIFI_CHANNEL_DWELL_TIME = "wifi_dwell";
    const char* const BLE_SCAN_DELAY = "ble_delay";
    const char* const IGNORE_RANDOM = "ignore_rand";
    const char* const IGNORE_LOCAL = "ignore_local";
    const char* const BLE_SCAN_DUR = "ble_dur";
    const char* const AUTOSAVE_INT = "autosave_int";
    const char* const PASSIVE_SCAN = "passive";
    const char* const STEALTH_MODE = "stealth";
    const char* const AUTH_ADDR = "auth_addr";
    const char* const CPU_SPEED = "cpu_speed";
    const char* const LED_MODE = "led_mode";
    const char* const WIFI_TX_POWER = "wifi_tx_power";
    const char* const BLE_TX_POWER = "ble_tx_power";
    const char* const BLE_MTU = "ble_mtu";
}

// Declaraciones de funciones
void initAppPreferences();
void loadAppPreferences();
void saveAppPreferences();

// Variables externas
extern AppPreferencesData appPrefs;
extern Preferences preferences;

#endif // APP_PREFERENCES_H
