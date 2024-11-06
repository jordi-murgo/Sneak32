#include "BLECommands.h"
#include "WifiDeviceList.h"
#include "WifiNetworkList.h"
#include "BLEDeviceList.h"
#include "FlashStorage.h"
#include "WifiDetect.h"
#include "BLEDetect.h"
#include "BLEStatusUpdater.h"
#include <Arduino.h>

// External variables
extern WifiDeviceList stationsList;
extern WifiNetworkList ssidList;
extern BLEDeviceList bleDeviceList;

void CommandsCallbacks::onWrite(BLECharacteristic *pCharacteristic)
{
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0)
    {
        Serial.println("Received command: " + String(value.c_str()));

        if (value == "restart")
        {
            ESP.restart();
        }
        else if (value == "clear_data")
        {
            FlashStorage::clearAll();
            BLEStatusUpdater.update();
        }
        else if (value == "save_data")
        {
            FlashStorage::saveAll();
        }
    }
}

