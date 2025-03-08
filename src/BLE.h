#ifndef BLE_H
#define BLE_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "BLEDeviceList.h"
#include "AppPreferences.h"
#include "FirmwareInfo.h"

// BLE UUIDs as strings for the 128-bit BLE UUIDs
// Main service UUID (custom UUID)
#define SNEAK32_SERVICE_UUID_STR "81af4cd7-e091-490a-99ee-caa99032ef4e"

// Standard 16-bit UUIDs converted to full 128-bit format
// These MUST match exactly what the web client expects
#define FIRMWARE_INFO_UUID_STR "0000ffe3-0000-1000-8000-00805f9b34fb"
#define SETTINGS_UUID_STR "0000ffe2-0000-1000-8000-00805f9b34fb"
#define DATA_TRANSFER_UUID_STR "0000ffe0-0000-1000-8000-00805f9b34fb"
#define COMMANDS_UUID_STR "0000ffe4-0000-1000-8000-00805f9b34fb"
#define STATUS_UUID_STR "0000ffe1-0000-1000-8000-00805f9b34fb"
#define RESPONSE_UUID_STR "0000ffe5-0000-1000-8000-00805f9b34fb"

// BLE UUIDs as 16-bit values - using these directly with BLEUUID constructor
// will automatically convert to the 128-bit form with the standard Bluetooth base UUID
// These MUST match the values in the string UUIDs above (last 2 bytes)
#define DATA_TRANSFER_UUID 0xFFE0  // Used in web client
#define STATUS_UUID 0xFFE1         // Used in web client
#define SETTINGS_UUID 0xFFE2       // Used in web client
#define FIRMWARE_INFO_UUID 0xFFE3  // Used in web client - critical for firmware info
#define COMMANDS_UUID 0xFFE4       // Used in web client
#define RESPONSE_UUID 0xFFE5       // Used in web client

#define DEVICE_APPEARANCE 192 // SmartWatch

// Function declarations
bool setupBLE();
void sendPacket(uint16_t packetNumber, boolean only_relevant_stations);
void prepareJsonData(boolean only_relevant_stations);
void doBLEScan();
void checkTransmissionTimeout();
// getFirmwareInfoString is declared in FirmwareInfo.h

// TODO, hacer algun método de xmit que informe ...de si esta en xmit o no
extern String preparedJsonData;

extern BLECharacteristic *pListSizesCharacteristic;
extern BLECharacteristic *pStatusCharacteristic;
extern BLECharacteristic *pCommandCharacteristic;
extern BLECharacteristic *pResponseCharacteristic;
extern BLECharacteristic *pFirmwareInfoCharacteristic;
extern BLECharacteristic *pDataTransferCharacteristic;
extern BLECharacteristic *pSettingsCharacteristic;

// Callback classes declarations
class MyServerCallbacks;
class ListSizesCallbacks;
class SendDataOverBLECallbacks;
class SettingsCallbacks;
class MyAdvertisedDeviceCallbacks;
class BLECommands;

#endif // BLE_H
