#ifndef BLE_DATA_TRANSFER_H
#define BLE_DATA_TRANSFER_H

#include <BLECharacteristic.h>
#include <Arduino.h>
#include "WifiDeviceList.h"
#include "WifiNetworkList.h"
#include "BLEDeviceList.h"

// Fixed sizes for binary records
#define MAC_ADDR_SIZE 6
#define SSID_SIZE 32
#define TYPE_SIZE 16
#define NAME_SIZE 32
#define TIMESTAMP_SIZE 8
#define COUNTER_SIZE 4
#define RSSI_SIZE 1
#define CHANNEL_SIZE 1
#define IS_PUBLIC_SIZE 1

// Record sizes
#define WIFI_NETWORK_RECORD_SIZE (MAC_ADDR_SIZE + SSID_SIZE + RSSI_SIZE + CHANNEL_SIZE + TYPE_SIZE + TIMESTAMP_SIZE + COUNTER_SIZE)
#define WIFI_DEVICE_RECORD_SIZE (MAC_ADDR_SIZE + MAC_ADDR_SIZE + RSSI_SIZE + CHANNEL_SIZE + TIMESTAMP_SIZE + COUNTER_SIZE)
#define BLE_DEVICE_RECORD_SIZE (MAC_ADDR_SIZE + NAME_SIZE + RSSI_SIZE + TIMESTAMP_SIZE + IS_PUBLIC_SIZE + COUNTER_SIZE)

// Request types
#define REQUEST_SSID_LIST "ssid_list"
#define REQUEST_CLIENT_LIST "client_list"
#define REQUEST_BLE_LIST "ble_list"

class SendDataOverBLECallbacks : public BLECharacteristicCallbacks
{
public:
    void onWrite(BLECharacteristic *pCharacteristic) override;
};

void sendPacket(uint16_t packetNumber, const String& requestType);
void checkTransmissionTimeout();
uint16_t calculateTotalPackets(const String& requestType);
size_t getItemsPerPacket(const String& requestType);

// Helper functions for network byte order
void writeUint16(uint8_t* buffer, uint16_t value, size_t& offset);
void writeUint32(uint8_t* buffer, uint32_t value, size_t& offset);
void writeUint64(uint8_t* buffer, uint64_t value, size_t& offset);
void writeInt8(uint8_t* buffer, int8_t value, size_t& offset);
void writeFixedString(uint8_t* buffer, const String& str, size_t maxLength, size_t& offset);
void writeMacAddress(uint8_t* buffer, const MacAddress& addr, size_t& offset);

#endif // BLE_DATA_TRANSFER_H
