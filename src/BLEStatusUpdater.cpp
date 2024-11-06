#include "BLEStatusUpdater.h"

BLEStatusUpdaterClass BLEStatusUpdater;

void BLEStatusUpdaterClass::update()
{
    static String last_statusString = "";

    // External variables from BLE.cpp
    extern BLECharacteristic *pStatusCharacteristic;
    extern bool deviceConnected;

    // External variables from BLE.cpp
    extern WifiNetworkList ssidList;
    extern WifiDeviceList stationsList;
    extern BLEDeviceList bleDeviceList;

    // First 3 fields from updateListSizesCharacteristic
    size_t ssids_size = ssidList.size();
    size_t stations_size = stationsList.size();
    size_t ble_devices_size = bleDeviceList.size();

    // Next 3 fields from updateDetectedDevicesCharacteristic
    size_t detected_ssids_size = WifiDetector.getDetectedNetworksCount();
    size_t detected_stations_size = WifiDetector.getDetectedDevicesCount();
    size_t detected_ble_devices_size = BLEDetector.getDetectedDevicesCount();

    // Remaining fields
    bool alarm = BLEDetector.isSomethingDetected() | WifiDetector.isSomethingDetected();
    size_t free_heap = ESP.getFreeHeap();
    unsigned long uptime = millis() / 1000; // Convert milliseconds to seconds

    // Create comprehensive status string
    String statusString = String(ssids_size) + ":" +
                          String(stations_size) + ":" +
                          String(ble_devices_size) + ":" +
                          String(detected_ssids_size) + ":" +
                          String(detected_stations_size) + ":" +
                          String(detected_ble_devices_size) + ":" +
                          String(alarm) + ":" +
                          String(free_heap) + ":";

    String statusStringWithUptime = statusString + String(uptime);

    pStatusCharacteristic->setValue(statusStringWithUptime.c_str());
    Serial.printf("Status updated -> %s\n", statusStringWithUptime.c_str());

    if (deviceConnected)
    {
        if (last_statusString.isEmpty() || last_statusString != statusString)
        {
            Serial.println("Notifying status");
            pStatusCharacteristic->notify();
            last_statusString = statusString;
        }
    }
} 