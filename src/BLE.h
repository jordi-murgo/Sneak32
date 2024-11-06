#ifndef BLE_H
#define BLE_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "BLEDeviceList.h"
#include "AppPreferences.h"

// BLE UUIDs
#define SCANNER_SERVICE_UUID "81af4cd7-e091-490a-99ee-caa99032ef4e"
#define SCANNER_DATATRANSFER_UUID 0xFFE0
#define STATUS_UUID 0xFFE1
#define SETTINGS_UUID 0xFFE2
#define FIRMWARE_INFO_UUID 0xFFE3
#define COMMANDS_UUID 0xFFE4

#define DEVICE_APPEARANCE 192 // SmartWatch

// Function declarations
void setupBLE();
void sendPacket(uint16_t packetNumber, boolean only_relevant_stations);
void prepareJsonData(boolean only_relevant_stations);
void doBLEScan();
void checkTransmissionTimeout();
String getFirmwareInfo();

// TODO, hacer algun método de xmit que informe ...de si esta en xmit o no
extern String preparedJsonData;

extern BLECharacteristic *pListSizesCharacteristic;

// Callback classes declarations
class MyServerCallbacks;
class ListSizesCallbacks;
class SendDataOverBLECallbacks;
class SettingsCallbacks;
class MyAdvertisedDeviceCallbacks;
class CommandsCallbacks;

#endif // BLE_H
