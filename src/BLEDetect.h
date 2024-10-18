#pragma once

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "AppPreferences.h"
#include "MACAddress.h"
#include <vector>
#include <mutex>

class BLEDetectClass {
public:
    BLEDetectClass(); // Default constructor
    void setup();
    void start();
    void stop();

    std::vector<MacAddress> getDetectedDevices();
    time_t getLastDetectionTime();
    bool isSomethingDetected();
    void cleanDetectionData();
    size_t getDetectedDevicesCount();

private:
    class BLEDetectAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
        private:
            BLEDetectClass* parent;
        public:
            BLEDetectAdvertisedDeviceCallbacks(BLEDetectClass* parent) : parent(parent) {}
            void onResult(BLEAdvertisedDevice advertisedDevice) override;
    };

    void detect_loop();
    
    TaskHandle_t detectTaskHandle = nullptr;
    BLEScan* pBLEScan = nullptr;
    BLEDetectAdvertisedDeviceCallbacks* callbacks = nullptr;
    bool isDetecting = false;

    time_t lastDetectionTime;
    std::vector<MacAddress> detectedDevices;
    std::mutex detectedDevicesMutex;
};

extern BLEDetectClass BLEDetector;
