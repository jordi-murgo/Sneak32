#include "DeviceManager.h"
#include "FlashStorage.h"
#include "AppPreferences.h"
#include <esp_log.h>

// Constructor
DeviceManager::DeviceManager() : 
    // Inicializar listas con tamaños máximos
    bleDeviceList(MAX_BLE_DEVICES),
    wifiDeviceList(MAX_STATIONS),
    wifiNetworkList(MAX_SSIDS)
{
    // Crear semáforos para protección de recursos
    bleMutex = xSemaphoreCreateMutex();
    wifiDeviceMutex = xSemaphoreCreateMutex();
    wifiNetworkMutex = xSemaphoreCreateMutex();
    flashMutex = xSemaphoreCreateMutex();
    
    if (!bleMutex || !wifiDeviceMutex || !wifiNetworkMutex || !flashMutex) {
        log_e("Error al crear semáforos para DeviceManager");
    }
}

// --- Métodos para BLEFoundDevice ---

bool DeviceManager::addBLEDevice(const BLEFoundDevice& device) {
    if (xSemaphoreTake(bleMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        log_e("No se pudo obtener bleMutex para addBLEDevice");
        return false;
    }
    
    bleDeviceList.addDevice(device);
    xSemaphoreGive(bleMutex);
    return true;
}

bool DeviceManager::updateBLEDevice(const MacAddress& address, int8_t rssi, const String& name, bool isPublic) {
    if (xSemaphoreTake(bleMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        log_e("No se pudo obtener bleMutex para updateBLEDevice");
        return false;
    }
    
    bleDeviceList.updateOrAddDevice(address, rssi, name, isPublic);
    xSemaphoreGive(bleMutex);
    return true;
}

std::vector<BLEFoundDevice, DynamicPsramAllocator<BLEFoundDevice>> DeviceManager::getBLEDeviceList() {
    if (xSemaphoreTake(bleMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        log_e("No se pudo obtener bleMutex para getBLEDeviceList");
        return std::vector<BLEFoundDevice, DynamicPsramAllocator<BLEFoundDevice>>();
    }
    
    auto list = bleDeviceList.getClonedList();
    xSemaphoreGive(bleMutex);
    return list;
}

size_t DeviceManager::getBLEDeviceCount() {
    if (xSemaphoreTake(bleMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        log_e("No se pudo obtener bleMutex para getBLEDeviceCount");
        return 0;
    }
    
    size_t count = bleDeviceList.size();
    xSemaphoreGive(bleMutex);
    return count;
}

void DeviceManager::clearBLEDevices() {
    if (xSemaphoreTake(bleMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        log_e("No se pudo obtener bleMutex para clearBLEDevices");
        return;
    }
    
    bleDeviceList.clear();
    xSemaphoreGive(bleMutex);
}

// --- Métodos para WifiDevice ---

bool DeviceManager::addWifiDevice(const WifiDevice& device) {
    if (xSemaphoreTake(wifiDeviceMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        log_e("No se pudo obtener wifiDeviceMutex para addWifiDevice");
        return false;
    }
    
    wifiDeviceList.addDevice(device);
    xSemaphoreGive(wifiDeviceMutex);
    return true;
}

bool DeviceManager::updateWifiDevice(const MacAddress& address, const MacAddress& bssid, int8_t rssi, uint8_t channel) {
    if (xSemaphoreTake(wifiDeviceMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        log_e("No se pudo obtener wifiDeviceMutex para updateWifiDevice");
        return false;
    }
    
    wifiDeviceList.updateOrAddDevice(address, bssid, rssi, channel);
    xSemaphoreGive(wifiDeviceMutex);
    return true;
}

std::vector<WifiDevice, DynamicPsramAllocator<WifiDevice>> DeviceManager::getWifiDeviceList() {
    if (xSemaphoreTake(wifiDeviceMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        log_e("No se pudo obtener wifiDeviceMutex para getWifiDeviceList");
        return std::vector<WifiDevice, DynamicPsramAllocator<WifiDevice>>();
    }
    
    auto list = wifiDeviceList.getClonedList();
    xSemaphoreGive(wifiDeviceMutex);
    return list;
}

size_t DeviceManager::getWifiDeviceCount() {
    if (xSemaphoreTake(wifiDeviceMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        log_e("No se pudo obtener wifiDeviceMutex para getWifiDeviceCount");
        return 0;
    }
    
    size_t count = wifiDeviceList.size();
    xSemaphoreGive(wifiDeviceMutex);
    return count;
}

void DeviceManager::clearWifiDevices() {
    if (xSemaphoreTake(wifiDeviceMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        log_e("No se pudo obtener wifiDeviceMutex para clearWifiDevices");
        return;
    }
    
    wifiDeviceList.clear();
    xSemaphoreGive(wifiDeviceMutex);
}

// --- Métodos para WifiNetwork ---

bool DeviceManager::addWifiNetwork(const WifiNetwork& network) {
    if (xSemaphoreTake(wifiNetworkMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        log_e("No se pudo obtener wifiNetworkMutex para addWifiNetwork");
        return false;
    }
    
    wifiNetworkList.addNetwork(network);
    xSemaphoreGive(wifiNetworkMutex);
    return true;
}

bool DeviceManager::updateWifiNetwork(const String& ssid, const MacAddress& bssid, int8_t rssi, uint8_t channel, const String& type) {
    if (xSemaphoreTake(wifiNetworkMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        log_e("No se pudo obtener wifiNetworkMutex para updateWifiNetwork");
        return false;
    }
    
    wifiNetworkList.updateOrAddNetwork(ssid, bssid, rssi, channel, type);
    xSemaphoreGive(wifiNetworkMutex);
    return true;
}

std::vector<WifiNetwork, DynamicPsramAllocator<WifiNetwork>> DeviceManager::getWifiNetworkList() {
    if (xSemaphoreTake(wifiNetworkMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        log_e("No se pudo obtener wifiNetworkMutex para getWifiNetworkList");
        return std::vector<WifiNetwork, DynamicPsramAllocator<WifiNetwork>>();
    }
    
    auto list = wifiNetworkList.getClonedList();
    xSemaphoreGive(wifiNetworkMutex);
    return list;
}

size_t DeviceManager::getWifiNetworkCount() {
    if (xSemaphoreTake(wifiNetworkMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        log_e("No se pudo obtener wifiNetworkMutex para getWifiNetworkCount");
        return 0;
    }
    
    size_t count = wifiNetworkList.size();
    xSemaphoreGive(wifiNetworkMutex);
    return count;
}

void DeviceManager::clearWifiNetworks() {
    if (xSemaphoreTake(wifiNetworkMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        log_e("No se pudo obtener wifiNetworkMutex para clearWifiNetworks");
        return;
    }
    
    wifiNetworkList.clear();
    xSemaphoreGive(wifiNetworkMutex);
}

// --- Métodos de guardado/carga ---

bool DeviceManager::saveAllToFlash() {
    if (xSemaphoreTake(flashMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        log_e("No se pudo obtener flashMutex para saveAllToFlash");
        return false;
    }
    
    bool success = true;
    try {
        // Ejecutar guardado en flash
        if (xSemaphoreTake(bleMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Usar el método sin parámetros que guardará todas las listas globales
            FlashStorage::saveAll();
            xSemaphoreGive(bleMutex);
            log_i("Datos guardados correctamente en flash");
        } else {
            log_e("No se pudo obtener bleMutex para saveAllToFlash");
            success = false;
        }
    } catch (const std::exception& e) {
        log_e("Error al guardar datos en flash: %s", e.what());
        success = false;
    }
    
    xSemaphoreGive(flashMutex);
    return success;
}

bool DeviceManager::loadAllFromFlash() {
    if (xSemaphoreTake(flashMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        log_e("No se pudo obtener flashMutex para loadAllFromFlash");
        return false;
    }
    
    bool success = true;
    try {
        // Cargar todas las listas desde flash
        FlashStorage::loadAll();
        log_i("Datos cargados correctamente desde flash");
        log_i("BLE: %zu dispositivos, WiFi: %zu dispositivos, Redes: %zu",
              bleDeviceList.size(), wifiDeviceList.size(), wifiNetworkList.size());
    } catch (const std::exception& e) {
        log_e("Error al cargar datos desde flash: %s", e.what());
        success = false;
    }
    
    xSemaphoreGive(flashMutex);
    return success;
}

// --- Métodos adicionales ---

void DeviceManager::removeIrrelevantDevices(time_t thresholdTime) {
    // Limpiar dispositivos BLE antiguos
    if (xSemaphoreTake(bleMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        size_t beforeCount = bleDeviceList.size();
        // Eliminar dispositivos con last_seen < thresholdTime
        // TODO: Implementar método interno de eliminación
        size_t afterCount = bleDeviceList.size();
        log_i("Eliminados %zu dispositivos BLE antiguos", beforeCount - afterCount);
        xSemaphoreGive(bleMutex);
    }
    
    // Limpiar dispositivos WiFi antiguos
    if (xSemaphoreTake(wifiDeviceMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        size_t beforeCount = wifiDeviceList.size();
        // TODO: Implementar método interno de eliminación
        size_t afterCount = wifiDeviceList.size();
        log_i("Eliminados %zu dispositivos WiFi antiguos", beforeCount - afterCount);
        xSemaphoreGive(wifiDeviceMutex);
    }
    
    // Limpiar redes WiFi antiguas
    if (xSemaphoreTake(wifiNetworkMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        size_t beforeCount = wifiNetworkList.size();
        // TODO: Implementar método interno de eliminación
        size_t afterCount = wifiNetworkList.size();
        log_i("Eliminadas %zu redes WiFi antiguas", beforeCount - afterCount);
        xSemaphoreGive(wifiNetworkMutex);
    }
}