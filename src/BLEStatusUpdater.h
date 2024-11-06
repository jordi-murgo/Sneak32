#pragma once

#include <Arduino.h>
#include "WifiDetect.h"
#include "BLEDetect.h"
#include "WifiDeviceList.h"
#include "WifiNetworkList.h"
#include "BLEDeviceList.h"


class BLEStatusUpdaterClass {

public:
    void update();
};

extern BLEStatusUpdaterClass BLEStatusUpdater;


