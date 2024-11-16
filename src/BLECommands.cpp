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
void saveWifiNetworksCallback(cmd* cmdPtr);
void saveWifiDevicesCallback(cmd* cmdPtr);
void saveBleDevicesCallback(cmd* cmdPtr);

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
 
    Command saveWifiNetworks = pCli->addCommand("save_wifi_networks", saveWifiNetworksCallback);
    saveWifiNetworks.setDescription("Save WiFi networks to FlashStorage");

    Command saveWifiDevices = pCli->addCommand("save_wifi_devices", saveWifiDevicesCallback);
    saveWifiDevices.setDescription("Save WiFi devices to FlashStorage");

    Command saveBleDevices = pCli->addCommand("save_ble_devices", saveBleDevicesCallback);
    saveBleDevices.setDescription("Save BLE devices to FlashStorage");

    Command restart = pCli->addCommand("restart", restartCallback);
    restart.setDescription("Restart the device");
       
    // Set error callback
    pCli->setOnError(errorCallback);

    // Parse command
    String value = String(characteristic->getValue().c_str());
    Serial.println("BLE Command received: " + value);
    pCli->parse(value.c_str());
}

void BLECommands::respond(String text) {
    if (pCharacteristic != nullptr) {
        pCharacteristic->setValue(text.c_str());
        Serial.println("BLE Response: " + text);
    } else {
        Serial.println("ERROR: Cannot send BLE response, characteristic is null");
    }
}

void BLECommands::respond(uint8_t *value, size_t length) {
    if (pCharacteristic != nullptr) {
        pCharacteristic->setValue(value, length);
        Serial.println("BLE Response: " + String(value, length));
    } else {
        Serial.println("ERROR: Cannot send BLE response, characteristic is null");
    }
}

// Callback implementations
void helpCallback(cmd* cmdPtr) {
    Command cmd(cmdPtr);
    BLECommands::respond(BLECommands::getFormattedHelp());
}

void restartCallback(cmd* cmdPtr) {
    BLECommands::respond("Restarting...");
    ESP.restart();
}

void clearDataCallback(cmd* cmdPtr) {
    FlashStorage::clearAll();
    BLECommands::respond("Data cleared");
    BLEStatusUpdater.update();
}

void saveDataCallback(cmd* cmdPtr) {
    Serial.println("Save data command received");
    FlashStorage::saveAll();
    BLECommands::respond("Data saved");
}

void saveWifiNetworksCallback(cmd* cmdPtr) {
    Serial.println("Save WiFi networks command received");
    FlashStorage::saveWifiNetworks();
    BLECommands::respond("WiFi networks saved");
}   

void saveWifiDevicesCallback(cmd* cmdPtr) {
    Serial.println("Save WiFi devices command received");
    FlashStorage::saveWifiDevices();
    BLECommands::respond("WiFi devices saved");
}

void saveBleDevicesCallback(cmd* cmdPtr) {
    Serial.println("Save BLE devices command received");
    FlashStorage::saveBLEDevices();
    BLECommands::respond("BLE devices saved");
}

void testMtuCallback(cmd* cmdPtr) {
    Command cmd(cmdPtr);
    int mtuSize = cmd.getArgument(0).getValue().toInt();
    
    if (mtuSize < 20 || mtuSize > 600) {
        BLECommands::respond("Error: MTU size must be between 20 and 600");
        return;
    }

    Serial.println("Test MTU command received, sent " + String(mtuSize) + " bytes");
    uint8_t *mtuBuffer = (uint8_t *)malloc(mtuSize);
    memset(mtuBuffer, 'A', mtuSize);    
    BLECommands::respond(mtuBuffer, mtuSize);
    free(mtuBuffer);
}

void setMtuCallback(cmd* cmdPtr) {
    Command cmd(cmdPtr);
    int mtuSize = cmd.getArgument(0).getValue().toInt();

    if (mtuSize < 20 || mtuSize > 600) {
        BLECommands::respond("Error: MTU size must be between 20 and 600");
        return;
    }
    
    String response = "MTU set to " + String(mtuSize);
    appPrefs.bleMTU = mtuSize;
    saveAppPreferences();
    BLECommands::respond(response);
}

void versionCallback(cmd* cmdPtr) {
    String mArch = String(CONFIG_SDK_TOOLPREFIX);
    if (mArch.endsWith("-")) {
        mArch = mArch.substring(0, mArch.length() - 1);
    }

    String versionInfo = "Sneak32 " + String(AUTO_VERSION) + " (" + mArch + ")";
    BLECommands::respond(versionInfo);
}

void errorCallback(cmd_error* errorPtr) {
    CommandError e(errorPtr);
    BLECommands::respond("Error: " + e.toString());
}

String BLECommands::getFormattedHelp() {
    if (BLECommands::pCli == nullptr) {
        return "Error: CLI not initialized";
    }
    return BLECommands::pCli->getFormattedHelp();
}
