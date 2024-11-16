#ifndef FLASH_STORAGE_H
#define FLASH_STORAGE_H

#include <Arduino.h>
#include <Preferences.h>
#include "WifiDeviceList.h"
#include "BLEDeviceList.h"
#include "WifiNetworkList.h"

struct WifiDeviceStruct {
    uint8_t address[6];
    uint8_t bssid[6];
    int8_t rssi;
    uint8_t channel;
    time_t last_seen;
    uint32_t times_seen;
};

struct BLEDeviceStruct {
    uint8_t address[6];
    int8_t rssi;
    char name[32];
    bool isPublic;
    time_t last_seen;
    uint32_t times_seen;
};

struct WifiNetworkStruct {
    char ssid[32];
    uint8_t address[6];
    int8_t rssi;
    uint8_t channel;
    char type[16];
    time_t last_seen;
    uint32_t times_seen;
};

class FlashStorage {
public:
    static void saveWifiDevices();
    static void saveBLEDevices();
    static void saveWifiNetworks();

    static void loadWifiDevices();
    static void loadBLEDevices();
    static void loadWifiNetworks();
    static void loadAll();
    static void saveAll();
    static void clearAll();

private:
    static const char* NAMESPACE;
    static const char* WIFI_DEVICES_KEY;
    static const char* BLE_DEVICES_KEY;
    static const char* WIFI_NETWORKS_KEY;

    static Preferences preferences;
};

#endif // FLASH_STORAGE_H
