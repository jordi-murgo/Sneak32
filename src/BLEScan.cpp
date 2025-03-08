/**
 * @file BLEScan.cpp
 * @brief Implementation of BLE scanning functionality with defensive programming
 */

#include "BLEScan.h"
#include "BLEDeviceList.h"
#include "MacAddress.h"
#include "AppPreferences.h"
// Se restauran las referencias a la librería BLE
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

extern BLEDeviceList bleDeviceList;
extern AppPreferencesData appPrefs;

BLEScanClass BLEScanner;
// Constructor con inicialización segura
BLEScanClass::BLEScanClass() {
    // Inicialización segura de todos los punteros
    pBLEScan = nullptr;
    callbacks = nullptr;
    scanTaskHandle = nullptr;
    isScanning = false;
}

// Destructor con liberación segura
BLEScanClass::~BLEScanClass() {
    // Asegurarse de liberar recursos
    stop();
    
    // Limpieza segura del callback
    if (callbacks != nullptr) {
        delete callbacks;
        callbacks = nullptr;
    }
}

// Constructor del callback que almacena una referencia al scanner
BLEScanAdvertisedDeviceCallbacks::BLEScanAdvertisedDeviceCallbacks(BLEScanClass* scan) : scan(scan) {}

// Setup - Inicializa el scanner BLE
void BLEScanClass::setup() {
    log_i("Setting up BLE Scanner");
    
    // Verificar si BLE está inicializado
    if (!BLEDevice::getInitialized()) {
        log_e("BLE no está inicializado, no se puede configurar el scanner");
        return;
    }
    
    pBLEScan = BLEDevice::getScan();
    if (pBLEScan == nullptr) {
        log_e("No se pudo obtener el scanner BLE");
        return;
    }
    
    callbacks = new BLEScanAdvertisedDeviceCallbacks(this);
    start();
    
    log_i("BLE Scanner setup complete");
}

// Start - Inicia el scanner BLE
bool BLEScanClass::start() {
    log_i("Starting BLE Scan task");
    
    if (pBLEScan == nullptr) {
        log_e("El scanner BLE no está inicializado");
        return false;
    }
    
    pBLEScan->setAdvertisedDeviceCallbacks(callbacks);
    pBLEScan->setActiveScan(true);
    
    if (!isScanning) {
        isScanning = true;
        xTaskCreatePinnedToCore(
            [](void* parameter) {
                static_cast<BLEScanClass*>(parameter)->scan_loop();
            },
            "BLE_Scan_Task",
            4096,
            this,
            1,
            &scanTaskHandle,
            0
        );
    }
    
    log_i("BLE Scan task started");
    return true;
}

// Stop - Detiene el scanner BLE
void BLEScanClass::stop() {
    log_i("Stopping BLE Scan task");
    
    if (pBLEScan != nullptr) {
        pBLEScan->stop();
        pBLEScan->setAdvertisedDeviceCallbacks(nullptr);
    }
    
    if (isScanning) {
        isScanning = false;
        if (scanTaskHandle != nullptr) {
            vTaskDelete(scanTaskHandle);
            scanTaskHandle = nullptr;
        }
        if (pBLEScan != nullptr) {
            pBLEScan->stop();
        }
    }
    
    log_i("BLE Scan task stopped");
}

// Scan Loop - Bucle principal del scanner BLE
void BLEScanClass::scan_loop() {
    log_i("ScanLoop started");
    
    while (true) {
        // Esperar entre escaneos según la configuración
        delay(appPrefs.ble_scan_delay * 1000);
        
        log_i("Starting BLE Scan");
        
        // Configurar parámetros de escaneo
        if (pBLEScan != nullptr) {
            pBLEScan->setInterval(100);
            pBLEScan->setWindow(90);
            pBLEScan->setActiveScan(!appPrefs.passive_scan);
            
            // Realizar el escaneo
            BLEScanResults foundDevices = pBLEScan->start(appPrefs.ble_scan_duration, false);
            
            log_i("BLE Scan complete. %d devices found.", foundDevices.getCount());
        } else {
            log_e("pBLEScan is null in scan_loop");
        }
    }
}

/**
 * Handles BLE scan results
 * @param advertisedDevice The discovered BLE device
 */
void BLEScanAdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice advertisedDevice) {
    try {
        // Verificar el RSSI mínimo configurado
        int rssi = advertisedDevice.getRSSI();
        if (rssi >= appPrefs.minimal_rssi) {
            // Extraer información del dispositivo fuera de la sección crítica
            std::string addressStr = advertisedDevice.getAddress().toString();
            std::string nameStr = advertisedDevice.haveName() ? advertisedDevice.getName() : "";
            
            // Copiar la dirección BLE
            esp_bd_addr_t bleaddr;
            memcpy(bleaddr, advertisedDevice.getAddress().getNative(), sizeof(esp_bd_addr_t));
            
            // Determinar el tipo de dirección
            boolean isPublic = false;
            String addressType;
            
            switch (advertisedDevice.getAddressType()) {
                case BLE_ADDR_TYPE_PUBLIC:
                    addressType = "public";
                    isPublic = true;
                    break;
                case BLE_ADDR_TYPE_RANDOM:
                    addressType = "random";
                    break;
                case BLE_ADDR_TYPE_RPA_PUBLIC:
                    addressType = "RPA-public";
                    isPublic = true;
                    break;
                case BLE_ADDR_TYPE_RPA_RANDOM:
                    addressType = "RPA-random";
                    break;
                default:
                    addressType = "unknown";
                    break;
            }
            
            // Construir una sola línea de información con los datos esenciales
            String deviceInfo = "BLE Device: ";
            deviceInfo += addressStr.c_str();
            deviceInfo += " (";
            deviceInfo += addressType;
            deviceInfo += "), RSSI: ";
            deviceInfo += rssi;
            
            if (!nameStr.empty()) {
                deviceInfo += ", Name: '";
                deviceInfo += nameStr.c_str();
                deviceInfo += "'";
            }
            
            // Servicios importantes (sin mostrar todos)
            if (advertisedDevice.haveServiceUUID()) {
                deviceInfo += ", Service UUID: ";
                deviceInfo += advertisedDevice.getServiceUUID().toString().c_str();
            }
            
            // Imprimir la información del dispositivo en una sola línea de log
            log_i("%s", deviceInfo.c_str());
            
            // Descartar direcciones inválidas
            if (addressStr == "00:00:00:00:00:00") {
                // Esta es una dirección inválida, ignorarla
                return;
            }
            
            // Verificar configuración para direcciones aleatorias
            if (!isPublic && appPrefs.ignore_random_ble_addresses) {
                // Esta es una dirección BLE aleatoria, ignorarla
                log_i("Ignoring random BLE address due to configuration");
                return;
            }
            
            // Usar lock_guard para bloqueo seguro ante excepciones
            {
                std::lock_guard<std::mutex> lock(scan->mtx);
                if (!appPrefs.ignore_random_ble_addresses || isPublic) {
                    bool added = bleDeviceList.updateOrAddDevice(MacAddress(bleaddr), rssi, String(nameStr.c_str()), isPublic);
                    if (bleDeviceList.size() % 10 == 0) {
                        log_i("Total devices: %d", bleDeviceList.size());
                    }
                }
            }
        }
    } catch (const std::exception &e) {
        log_e("Exception in onResult: %s", e.what());
    } catch (...) {
        log_e("Unknown exception in onResult");
    }
}
