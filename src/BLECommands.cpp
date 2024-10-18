#include "BLECommands.h"
#include "WifiDeviceList.h"
#include "WifiNetworkList.h"
#include "BLEDeviceList.h"
#include "FlashStorage.h"
#include "WifiDetect.h"
#include "BLEDetect.h"
#include <Arduino.h>

// External variables
extern WifiDeviceList stationsList;
extern WifiNetworkList ssidList;
extern BLEDeviceList bleDeviceList;

// Function declaration
void updateListSizesCharacteristic();

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
            // Clear all collected data
            stationsList.clear();
            ssidList.clear();
            bleDeviceList.clear();
            updateListSizesCharacteristic();
        }
        else if (value == "clear_flash_data")
        {
            // Clear all saved data
            stationsList.clear();
            ssidList.clear();
            bleDeviceList.clear();
            FlashStorage::saveAll();
            updateListSizesCharacteristic();
        }
        else if (value == "save_data")
        {
            FlashStorage::saveAll();
        }
        else if (value == "save_relevant_data")
        {
            stationsList.remove_irrelevant_stations();
            ssidList.remove_irrelevant_networks();
            bleDeviceList.remove_irrelevant_devices();
            FlashStorage::saveAll();
            updateListSizesCharacteristic();
        } else if (value == "clean_detection_data") {
            WifiDetector.cleanDetectionData();
            BLEDetector.cleanDetectionData();
            updateListSizesCharacteristic();
        }
    }
}

