#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>
#include "MacAddress.h"
#include "DynamicPsramAllocator.h"
#include "WifiDeviceList.h"
#include "WifiNetworkList.h"
#include "BLEDeviceList.h"
#include "TaskManager.h" // Para definiciones de MAX_*

/**
 * @brief Clase que gestiona centralizadamente todas las listas de dispositivos.
 * 
 * Esta clase es la encargada de manejar las listas de dispositivos BLE, WiFi y redes WiFi.
 * Implementa semáforos para acceso seguro desde múltiples tareas y optimiza el uso de PSRAM.
 */
class DeviceManager {
public:
    static DeviceManager& getInstance() {
        static DeviceManager instance;
        return instance;
    }

    // Métodos para BLEFoundDevice
    bool addBLEDevice(const BLEFoundDevice& device);
    bool updateBLEDevice(const MacAddress& address, int8_t rssi, const String& name, bool isPublic);
    std::vector<BLEFoundDevice, DynamicPsramAllocator<BLEFoundDevice>> getBLEDeviceList();
    size_t getBLEDeviceCount();
    void clearBLEDevices();
    
    // Métodos para WifiDevice
    bool addWifiDevice(const WifiDevice& device);
    bool updateWifiDevice(const MacAddress& address, const MacAddress& bssid, int8_t rssi, uint8_t channel);
    std::vector<WifiDevice, DynamicPsramAllocator<WifiDevice>> getWifiDeviceList();
    size_t getWifiDeviceCount();
    void clearWifiDevices();
    
    // Métodos para WifiNetwork
    bool addWifiNetwork(const WifiNetwork& network);
    bool updateWifiNetwork(const String& ssid, const MacAddress& bssid, int8_t rssi, uint8_t channel, const String& type);
    std::vector<WifiNetwork, DynamicPsramAllocator<WifiNetwork>> getWifiNetworkList();
    size_t getWifiNetworkCount();
    void clearWifiNetworks();
    
    // Métodos de guardado/carga
    bool saveAllToFlash();
    bool loadAllFromFlash();
    
    // Purga de dispositivos antiguos
    void removeIrrelevantDevices(time_t thresholdTime);

private:
    DeviceManager();
    ~DeviceManager() = default;
    
    // Constructor de copia y asignación eliminados (singleton)
    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;
    
    // Listas de dispositivos con memoria en PSRAM
    BLEDeviceList bleDeviceList;
    WifiDeviceList wifiDeviceList;
    WifiNetworkList wifiNetworkList;
    
    // Semáforos para acceso concurrente
    SemaphoreHandle_t bleMutex;
    SemaphoreHandle_t wifiDeviceMutex;
    SemaphoreHandle_t wifiNetworkMutex;
    SemaphoreHandle_t flashMutex;
};