#pragma once

#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include "AppPreferences.h"

class BLEAdvertisingManager {
public:
    static void setup();
    static void start();
    static void stop();
    static void updateAdvertisingData();
    static void configureStealthMode();
    static void configureNormalMode();

private:
    static uint8_t advertisingMode;
    static BLEAdvertising* pAdvertising;

    static BLEAdvertisementData getAdvertisementData();
};
