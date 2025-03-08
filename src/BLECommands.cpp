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
#include "AppPreferences.h"

// Forward declaration of the system info function from main.cpp
void printSystemInfo();

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
void psCallback(cmd* cmdPtr);

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
    
    Command ps = pCli->addCommand("ps", psCallback);
    ps.setDescription("Display system task and memory info (similar to Unix ps)");
       
    // Set error callback
    pCli->setOnError(errorCallback);

    // Parse command
    String value = String(characteristic->getValue().c_str());
    log_i("BLE Command received: %s", value.c_str());
    pCli->parse(value.c_str());
}

void BLECommands::respond(String text) {
    if (pCharacteristic != nullptr) {
        // Check text length to avoid buffer overflows
        if (text.length() > 512) {
            log_w("BLE Response truncated from %d to 512 bytes", text.length());
            text = text.substring(0, 512);
        }
        pCharacteristic->setValue(text.c_str());
        log_i("BLE Response: %s", text.c_str());
    } else {
        log_e("Cannot send BLE response, characteristic is null");
    }
}

void BLECommands::respond(uint8_t *value, size_t length) {
    if (pCharacteristic != nullptr) {
        // Check length to avoid buffer overflows
        if (length > 512) {
            log_w("BLE Response truncated from %d to 512 bytes", length);
            length = 512;
        }
        pCharacteristic->setValue(value, length);
        // Only log a portion of the response if it's very large
        if (length > 100) {
            log_i("BLE Response: %s... (truncated, total length: %d)", String((char*)value, 100).c_str(), length);
        } else {
            log_i("BLE Response: %s", String((char*)value, length).c_str());
        }
    } else {
        log_e("Cannot send BLE response, characteristic is null");
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
    log_i("Save data command received");
    FlashStorage::saveAll();
    BLECommands::respond("Data saved");
}

void saveWifiNetworksCallback(cmd* cmdPtr) {
    log_i("Save WiFi networks command received");
    FlashStorage::saveWifiNetworks();
    BLECommands::respond("WiFi networks saved");
}   

void saveWifiDevicesCallback(cmd* cmdPtr) {
    log_i("Save WiFi devices command received");
    FlashStorage::saveWifiDevices();
    BLECommands::respond("WiFi devices saved");
}

void saveBleDevicesCallback(cmd* cmdPtr) {
    log_i("Save BLE devices command received");
    FlashStorage::saveBLEDevices();
    BLECommands::respond("BLE devices saved");
}

void testMtuCallback(cmd* cmdPtr) {
    Command cmd(cmdPtr);
    int mtuSize = cmd.getArgument(0).getValue().toInt();
    
    if (mtuSize < 20 || mtuSize > 512) {
        BLECommands::respond("Error: MTU size must be between 20 and 512");
        return;
    }

    log_i("Test MTU command received, sent %d bytes", mtuSize);
    
    // Use PSRAM if available for large buffers
    uint8_t *mtuBuffer = nullptr;
    
    #if defined(CONFIG_SPIRAM_SUPPORT)
    if (mtuSize > 256) {
        mtuBuffer = (uint8_t *)ps_malloc(mtuSize + 1);  // +1 for null terminator
    } else {
        mtuBuffer = (uint8_t *)malloc(mtuSize + 1);  // +1 for null terminator
    }
    #else
    mtuBuffer = (uint8_t *)malloc(mtuSize + 1);  // +1 for null terminator
    #endif
    
    if (mtuBuffer == nullptr) {
        log_e("Failed to allocate memory for MTU test");
        BLECommands::respond("Error: Memory allocation failed");
        return;
    }
    
    // Fill with a pattern that's less likely to cause issues
    for (int i = 0; i < mtuSize; i++) {
        mtuBuffer[i] = 'A' + (i % 26);  // Cycle through alphabet
    }
    mtuBuffer[mtuSize] = '\0';  // Null terminate
    
    BLECommands::respond(mtuBuffer, mtuSize);
    free(mtuBuffer);
}

void setMtuCallback(cmd* cmdPtr) {
    Command cmd(cmdPtr);
    int mtuSize = cmd.getArgument(0).getValue().toInt();

    if (mtuSize < 20 || mtuSize > 512) {
        BLECommands::respond("Error: MTU size must be between 20 and 512");
        return;
    }

    // Update the MTU size in the app preferences
    appPrefs.bleMTU = mtuSize;
    
    // Save preferences in a safe way
    try {
        saveAppPreferences();
        
        // Set the MTU size in the BLE device
        BLEDevice::setMTU(mtuSize);
        
        BLECommands::respond("MTU set to " + String(mtuSize));
    } catch (const std::exception& e) {
        log_e("Exception in setMtuCallback: %s", e.what());
        BLECommands::respond("Error setting MTU: " + String(e.what()));
    } catch (...) {
        log_e("Unknown exception in setMtuCallback");
        BLECommands::respond("Unknown error setting MTU");
    }
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

// Callback for the ps command - shows system task info
void psCallback(cmd* cmdPtr) {
    Command cmd(cmdPtr);
    BLECommands::respond("Printing task information to serial console...");
    
    // Call the system info function to print to serial
    log_i("=========== PS COMMAND EXECUTED ===========");
    printSystemInfo();
    log_i("=========== END OF PS COMMAND ============");
}
