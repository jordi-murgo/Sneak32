#include "BLECommands.h"
#include "WifiDeviceList.h"
#include "WifiNetworkList.h"
#include "BLEDeviceList.h"
#include "FlashStorage.h"
#include "WifiDetect.h"
#include "BLEDetect.h"
#include "BLEStatusUpdater.h"
#include <Arduino.h>
#include <SimpleCLI.h>
#include "FirmwareInfo.h"

// External variables
extern WifiDeviceList stationsList;
extern WifiNetworkList ssidList;
extern BLEDeviceList bleDeviceList;

// Callback functions declarations
void helpCallback(cmd* cmdPtr);
void restartCallback(cmd* cmdPtr);
void clearDataCallback(cmd* cmdPtr);
void saveDataCallback(cmd* cmdPtr);
void testMtuCallback(cmd* cmdPtr);
void setMtuCallback(cmd* cmdPtr);
void versionCallback(cmd* cmdPtr);
void errorCallback(cmd_error* errorPtr);

BLECharacteristic* BLECommands::pCharacteristic = nullptr;
SimpleCLI* BLECommands::pCli = nullptr;

void BLECommands::onWrite(BLECharacteristic* characteristic) {
    pCharacteristic = characteristic;
    pCli = new SimpleCLI();

    // Define commands with callbacks
    Command help = pCli->addCommand("help", helpCallback);
    help.setDescription("List available commands");
    
    Command version = pCli->addCommand("version", versionCallback);
    version.setDescription("Show firmware version");

    Command test_mtu = pCli->addSingleArgCmd("test_mtu", testMtuCallback);
    test_mtu.setDescription("Test MTU size");
    
    Command set_mtu = pCli->addSingleArgCmd("set_mtu", setMtuCallback);
    set_mtu.setDescription("Set the MTU size for data transfers");
 
    Command clear = pCli->addCommand("clear_data", clearDataCallback);
    clear.setDescription("Clear all captured data, including FlashStorage");
    
    Command save = pCli->addCommand("save_data", saveDataCallback);
    save.setDescription("Save all captured data to FlashStorage");
 
    Command restart = pCli->addCommand("restart", restartCallback);
    restart.setDescription("Restart the device");
       
    // Set error callback
    pCli->setOnError(errorCallback);

    // Parse command
    String value = String(characteristic->getValue().c_str());
    Serial.println("BLE Command received: " + value);
    pCli->parse(value.c_str());
}

// Callback implementations
void helpCallback(cmd* cmdPtr) {
    Command cmd(cmdPtr);
    String cmdList = BLECommands::pCli->getFormattedHelp();
    BLECommands::pCharacteristic->setValue(cmdList.c_str());
}

void restartCallback(cmd* cmdPtr) {
    ESP.restart();
}

void clearDataCallback(cmd* cmdPtr) {
    FlashStorage::clearAll();
    BLEStatusUpdater.update();
}

void saveDataCallback(cmd* cmdPtr) {
    FlashStorage::saveAll();
}

void testMtuCallback(cmd* cmdPtr) {
    Command cmd(cmdPtr);
    int mtuSize = cmd.getArgument(0).getValue().toInt();
    
    if (mtuSize < 20 || mtuSize > 600) {
        String response = "Error: MTU size must be between 20 and 600";
        BLECommands::pCharacteristic->setValue(response.c_str());
        Serial.println(response);
        return;
    }

    Serial.println("Test MTU command received, sent " + String(mtuSize) + " bytes");
    uint8_t *mtuBuffer = (uint8_t *)malloc(mtuSize);
    memset(mtuBuffer, 'A', mtuSize);    
    BLECommands::pCharacteristic->setValue(mtuBuffer, mtuSize);
    free(mtuBuffer);
}

void setMtuCallback(cmd* cmdPtr) {
    Command cmd(cmdPtr);
    int mtuSize = cmd.getArgument(0).getValue().toInt();

    if (mtuSize < 20 || mtuSize > 600) {
        String response = "Error: MTU size must be between 20 and 600";
        BLECommands::pCharacteristic->setValue(response.c_str());
        Serial.println(response);
        return;
    }
    
    String response = "MTU set to " + String(mtuSize);
    appPrefs.bleMTU = mtuSize;
    saveAppPreferences();
    BLECommands::pCharacteristic->setValue(response.c_str());
    Serial.println(response);
}

void versionCallback(cmd* cmdPtr) {
    String mArch = String(CONFIG_SDK_TOOLPREFIX);
    if (mArch.endsWith("-")) {
        mArch = mArch.substring(0, mArch.length() - 1);
    }

    String versionInfo = "Sneak32 " + String(AUTO_VERSION) + " (" + mArch + ")";
    BLECommands::pCharacteristic->setValue(versionInfo.c_str());
    Serial.println(versionInfo);
}

void errorCallback(cmd_error* errorPtr) {
    CommandError e(errorPtr);
    String response = "Error: " + e.toString();
    BLECommands::pCharacteristic->setValue(response.c_str());
    Serial.println(response);
}
