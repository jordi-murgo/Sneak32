#ifndef APP_PREFERENCES_H
#define APP_PREFERENCES_H

#include <Preferences.h>
#include <Arduino.h>

// Estructura para almacenar las preferencias
struct AppPreferencesData {
    char device_name[32];
    int8_t operation_mode;             // 0-OFF, 1-SCAN_MODE, 2-DETECTION_MODE
    // General
    int8_t minimal_rssi;
    // Wifi
    uint8_t only_management_frames;
    uint32_t loop_delay;
    // BLE
    uint32_t ble_scan_period;
    bool ignore_random_ble_addresses;  
    uint32_t ble_scan_duration;        
};

const int8_t OPERATION_MODE_OFF = 0;
const int8_t OPERATION_MODE_SCAN = 1;
const int8_t OPERATION_MODE_DETECTION = 2;

// Declaraciones de funciones
void initAppPreferences();
void loadAppPreferences();
void saveAppPreferences();

// Variables externas
extern AppPreferencesData appPrefs;
extern Preferences preferences;

#endif // APP_PREFERENCES_H