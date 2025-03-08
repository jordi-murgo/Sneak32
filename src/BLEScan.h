#pragma once

// Incluimos las bibliotecas BLE necesarias para la funcionalidad completa
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "AppPreferences.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <mutex>

// Declaración de la función utilizada por otros módulos
void printHexDump(const uint8_t *data, size_t length);

// Forward declarations
class BLEScanClass;

class BLEScanAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
    BLEScanAdvertisedDeviceCallbacks(BLEScanClass* scan);
    // Restauramos el override para la correcta herencia
    void onResult(BLEAdvertisedDevice advertisedDevice) override;
private:
    BLEScanClass* scan;
};

class BLEScanClass {
public:
    BLEScanClass();
    ~BLEScanClass();

    void setup();
    bool start();
    void stop();
    void scan_loop();
    std::mutex mtx;

private:
    // Static task function for FreeRTOS task creation
    static void scanTask(void* parameter);
    
    TaskHandle_t scanTaskHandle = nullptr;
    // Restauramos los punteros a objetos BLE para la implementación completa
    BLEScan* pBLEScan = nullptr;
    BLEScanAdvertisedDeviceCallbacks* callbacks = nullptr;
    bool isScanning = false;

    friend class BLEScanAdvertisedDeviceCallbacks;
};

extern BLEScanClass BLEScanner;
