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
        else if (value.substr(0, 8) == "test_mtu")
        {
            try {
                int mtuSize = std::stoi(value.substr(9));
                
                if (mtuSize < 20 || mtuSize > 600) {
                    String response = "Error: MTU size must be between 20 and 600";
                    pCharacteristic->setValue(response.c_str());
                    Serial.println(response);
                    return;
                }

                Serial.println("Test MTU command received, sent " + String(mtuSize) + " bytes");
                uint8_t *mtuBuffer = (uint8_t *)malloc(mtuSize);
                memset(mtuBuffer, 'A', mtuSize);    
                pCharacteristic->setValue(mtuBuffer, mtuSize);
                pCharacteristic->notify();
                free(mtuBuffer);
            } catch (const std::invalid_argument& e) {
                String response = "Error: Invalid MTU value format";
                pCharacteristic->setValue(response.c_str());
                Serial.println(response);
            }
        }
        else if (value.substr(0, 7) == "set_mtu")
        {
            try {
                int mtuSize = std::stoi(value.substr(8));

                if (mtuSize < 20 || mtuSize > 600) {
                    String response = "Error: MTU size must be between 20 and 600";
                    pCharacteristic->setValue(response.c_str());
                    Serial.println(response);
                    return;
                }
                
                String response = "MTU set to " + String(mtuSize);
                appPrefs.bleMTU = mtuSize;
                saveAppPreferences();
                pCharacteristic->setValue(String(response).c_str());
                Serial.println(response);
            } catch (const std::invalid_argument& e) {
                String response = "Error: Invalid MTU value format";
                pCharacteristic->setValue(response.c_str());
                Serial.println(response);
            }
        } else {
            String response = "Error: Unknown command " + String(value.c_str());
            pCharacteristic->setValue(response.c_str());
            Serial.println(response);
        }
    }
}