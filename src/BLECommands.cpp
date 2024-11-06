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
        else if (value == "test_mtu")
        {
            Serial.println("Test MTU command received");
            // Prepare a testMTU Buffer
            uint8_t *mtuBuffer = (uint8_t *)malloc(600);
            memset(mtuBuffer, 'A', 600);    
            pCharacteristic->setValue(mtuBuffer, 600);
            pCharacteristic->notify();
            free(mtuBuffer);
        }
        else if (value.substr(0, 7) == "set_mtu")
        {
            // Parse MTU size from the command
            int mtuSize = std::stoi(value.substr(8));
            
            String response = "MTU set to " + String(mtuSize);
            
            // Update the MTU in app preferences
            appPrefs.bleMTU = mtuSize;
            
            // Save the preference
            saveAppPreferences();
            
            // Respond with confirmation
            pCharacteristic->setValue(String(response).c_str());

            Serial.println(response);
        } else {
            Serial.println("Unknown command: " + String(value.c_str()));
        }
    }
}