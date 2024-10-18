#pragma once

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "AppPreferences.h"

class BLEScanClass {
public:
    void setup();
    void start();
    void stop();
    void setFilter(bool onlyManagementFrames);

private:
    class BLEScanAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
        void onResult(BLEAdvertisedDevice advertisedDevice) override;
    };
    void scan_loop();
    
    TaskHandle_t scanTaskHandle = nullptr;
    BLEScan* pBLEScan = nullptr;
    BLEScanAdvertisedDeviceCallbacks* callbacks = nullptr;
    bool isScanning = false;
};

extern BLEScanClass BLEScanner;
