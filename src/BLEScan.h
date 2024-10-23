#pragma once

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "AppPreferences.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <mutex>

class BLEScanClass;  // Forward declaration

class BLEScanAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
    BLEScanAdvertisedDeviceCallbacks(BLEScanClass* scan);
    void onResult(BLEAdvertisedDevice advertisedDevice) override;
private:
    BLEScanClass* scan;
};

class BLEScanClass {
public:
    void setup();
    void start();
    void stop();
    std::mutex mtx;

private:
    void scan_loop();
    
    TaskHandle_t scanTaskHandle = nullptr;
    BLEScan* pBLEScan = nullptr;
    BLEScanAdvertisedDeviceCallbacks* callbacks = nullptr;
    bool isScanning = false;

    friend class BLEScanAdvertisedDeviceCallbacks;
};

extern BLEScanClass BLEScanner;
