#include "BLEStatusUpdater.h"

BLEStatusUpdaterClass BLEStatusUpdater;

void BLEStatusUpdaterClass::update()
{
    static String last_statusString = "";

    // External variables from BLE.cpp
    extern BLECharacteristic *pStatusCharacteristic;
    extern bool deviceConnected;

    // PROGRAMACIÓN DEFENSIVA: Verificar que el puntero a la característica no sea nulo antes de usarlo
    if (pStatusCharacteristic == nullptr) {
        log_w("BLEStatusUpdater::update() - pStatusCharacteristic es nullptr, no se puede actualizar el estado");
        return;
    }

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

    // PROGRAMACIÓN DEFENSIVA: Volvemos a verificar el puntero antes de usarlo
    // (podría haber cambiado durante la ejecución de este método)
    if (pStatusCharacteristic != nullptr) {
        try {
            pStatusCharacteristic->setValue(statusStringWithUptime.c_str());
            log_d("Status updated -> %s", statusStringWithUptime.c_str());
            
            if (deviceConnected)
            {
                if (last_statusString.isEmpty() || last_statusString != statusString)
                {
                    log_d("Notifying status");
                    pStatusCharacteristic->notify();
                    last_statusString = statusString;
                }
            }
        } catch (const std::exception& e) {
            log_e("Excepción capturada al intentar actualizar la característica BLE: %s", e.what());
        } catch (...) {
            log_e("Excepción desconocida capturada al intentar actualizar la característica BLE");
        }
    } else {
        log_w("BLEStatusUpdater::update() - pStatusCharacteristic se volvió nullptr durante la ejecución");
    }
} 