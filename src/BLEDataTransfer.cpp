#include "BLEDataTransfer.h"
#include <BLEDevice.h>
#include <cmath>
#include <arpa/inet.h>
#include <algorithm>
#include "WifiDeviceList.h"
#include "WifiNetworkList.h"
#include "BLEDeviceList.h"
#include "AppPreferences.h"

// External variables
extern BLEDeviceList bleDeviceList;
extern WifiDeviceList stationsList;
extern WifiNetworkList ssidList;
extern AppPreferencesData appPrefs;
extern time_t base_time;

// Global variables
extern BLECharacteristic *pTxCharacteristic;
extern bool deviceConnected;

// Constants
#define PACKET_START_MARKER "START:"
#define PACKET_END_MARKER "END"
#define PACKET_DELAY 100
#define PACKET_HEADER_SIZE 4
#define PACKET_TIMEOUT 5000
#define TRANSMISSION_TIMEOUT 10000

// Variables for data preparation and sending
uint16_t totalPackets = 0;
unsigned long lastPacketRequestTime = 0;
String currentRequestType;

// Replace the static MTU definition with a dynamic one
#define MAX_PACKET_SIZE (appPrefs.bleMTU - PACKET_HEADER_SIZE)

// Helper functions for network byte order
void writeUint16(uint8_t *buffer, uint16_t value, size_t &offset)
{
    uint16_t netValue = htons(value);
    memcpy(buffer + offset, &netValue, sizeof(uint16_t));
    offset += sizeof(uint16_t);
}

void writeUint32(uint8_t *buffer, uint32_t value, size_t &offset)
{
    uint32_t netValue = htonl(value);
    memcpy(buffer + offset, &netValue, sizeof(uint32_t));
    offset += sizeof(uint32_t);
}

void writeUint64(uint8_t *buffer, uint64_t value, size_t &offset)
{
    // Convert to network byte order (big-endian)
    uint64_t netValue = ((uint64_t)htonl(value & 0xFFFFFFFF) << 32) | htonl(value >> 32);
    memcpy(buffer + offset, &netValue, sizeof(uint64_t));
    offset += sizeof(uint64_t);
}

void writeInt8(uint8_t *buffer, int8_t value, size_t &offset)
{
    buffer[offset++] = value;
}

void writeFixedString(uint8_t *buffer, const String &str, size_t maxLength, size_t &offset)
{
    size_t strLen = str.length();
    memset(buffer + offset, 0, maxLength); // Fill with zeros
    if (strLen > 0)
    {
        memcpy(buffer + offset, str.c_str(), std::min(strLen, maxLength));
    }
    offset += maxLength;
}

void writeMacAddress(uint8_t *buffer, const MacAddress &addr, size_t &offset)
{
    memcpy(buffer + offset, addr.getBytes(), MAC_ADDR_SIZE);
    offset += MAC_ADDR_SIZE;
}

void SendDataOverBLECallbacks::onWrite(BLECharacteristic *pCharacteristic)
{
    if (!pCharacteristic) {
        log_e("Invalid characteristic in onWrite");
        return;
    }

    std::string value = pCharacteristic->getValue();

    if (value.length() > 0)
    {
        log_i("Received BLE value: %s (length: %d)", value.c_str(), value.length());

        if (value.length() == 4 && std::all_of(value.begin(), value.end(), ::isxdigit))
        {
            try {
                uint16_t requestedPacket = strtoul(value.substr(0, 4).c_str(), NULL, 16);
                log_d("Parsed packet number: %d, Current request type: %s",
                    requestedPacket, currentRequestType.c_str());

                if (!currentRequestType.isEmpty())
                {
                    sendPacket(requestedPacket, currentRequestType);
                    lastPacketRequestTime = millis();
                }
                else
                {
                    log_e("No valid current request type for packet %d", requestedPacket);
                    pCharacteristic->setValue("Error: No active request");
                }
            } catch (const std::exception& e) {
                log_e("Exception parsing packet number: %s", e.what());
                pCharacteristic->setValue("Error: Invalid packet number format");
            }
        }
        else
        {
            String requestType = String(value.c_str());
            if (requestType == REQUEST_SSID_LIST ||
                requestType == REQUEST_CLIENT_LIST ||
                requestType == REQUEST_BLE_LIST)
            {
                currentRequestType = requestType;
                sendPacket(0, requestType);
                lastPacketRequestTime = millis();
            }
            else
            {
                pCharacteristic->setValue("Error: Invalid request type");
                log_w("Invalid request type: %s", requestType.c_str());
            }
        }
    }
}

void checkTransmissionTimeout()
{
    unsigned long currentTime = millis();
    if (!deviceConnected || (currentTime - lastPacketRequestTime > TRANSMISSION_TIMEOUT))
    {
        if (!currentRequestType.isEmpty()) {
            log_w("Transmission timeout: Resetting current request type");
            currentRequestType = "";
        }
    }
}

size_t getItemsPerPacket(const String &requestType)
{
    size_t recordSize;

    if (requestType == REQUEST_BLE_LIST)
    {
        recordSize = BLE_DEVICE_RECORD_SIZE;
    }
    else if (requestType == REQUEST_CLIENT_LIST)
    {
        recordSize = WIFI_DEVICE_RECORD_SIZE;
    }
    else
    {
        recordSize = WIFI_NETWORK_RECORD_SIZE;
    }

    // Calculate effective MTU size for BLE notifications
    // Some BLE stacks limit notifications to 20 bytes or MTU-3
    size_t effectiveMtu = std::min<size_t>(appPrefs.bleMTU, 253); // 253 is typical max notify size
    
    // Ensure we don't try to send more than what fits in a single notification
    size_t maxItemsPerPacket = (effectiveMtu - PACKET_HEADER_SIZE) / recordSize;
    
    // Always send at least one item, but no more than what fits
    return std::max<size_t>(1, maxItemsPerPacket);
}

uint16_t calculateTotalPackets(const String &requestType)
{
    size_t itemsPerPacket = getItemsPerPacket(requestType);
    size_t totalItems;

    if (requestType == REQUEST_CLIENT_LIST)
    {
        totalItems = stationsList.getClonedList().size();
    }
    else if (requestType == REQUEST_SSID_LIST)
    {
        totalItems = ssidList.getClonedList().size();
    }
    else if (requestType == REQUEST_BLE_LIST)
    {
        totalItems = bleDeviceList.getClonedList().size();
    }
    else
    {
        return 0;
    }

    return (totalItems + itemsPerPacket - 1) / itemsPerPacket;
}

void sendPacket(uint16_t packetNumber, const String &requestType)
{
    if (pTxCharacteristic == nullptr || !deviceConnected)
    {
        log_e("Cannot send packet - invalid state");
        return;
    }

    try
    {
        if (packetNumber == 0)
        {
            // Before calculating total packets, check if the MTU has been set correctly
            if (appPrefs.bleMTU < 100) {
                log_w("BLE MTU value is low (%d), using default value", appPrefs.bleMTU);
                // Use a safer value to avoid truncation issues
                appPrefs.bleMTU = 512;
            }
            
            totalPackets = calculateTotalPackets(requestType);
            char packetsHeader[5];
            snprintf(packetsHeader, sizeof(packetsHeader), "%04X", totalPackets);
            String startMarker = String(PACKET_START_MARKER) + String(packetsHeader);
            pTxCharacteristic->setValue(startMarker.c_str());
            pTxCharacteristic->notify();
            log_i("Sent start marker: %s (MTU: %d, Max packet size: %d)", 
                  startMarker.c_str(), appPrefs.bleMTU, MAX_PACKET_SIZE);
        }
        else if (packetNumber <= totalPackets)
        {
            size_t itemsPerPacket = getItemsPerPacket(requestType);
            size_t startIndex = (packetNumber - 1) * itemsPerPacket;
            size_t length;
            uint8_t *buffer = nullptr;

            if (requestType == REQUEST_SSID_LIST)
            {
                std::vector<WifiNetwork, DynamicPsramAllocator<WifiNetwork>> networks = ssidList.getClonedList();
                size_t endIndex = std::min(startIndex + itemsPerPacket, networks.size());
                length = (endIndex - startIndex) * WIFI_NETWORK_RECORD_SIZE;
                buffer = new uint8_t[length];
                size_t offset = 0;

                for (size_t i = startIndex; i < endIndex; i++)
                {
                    writeMacAddress(buffer, networks[i].address, offset);
                    writeFixedString(buffer, networks[i].ssid, SSID_SIZE, offset);
                    writeInt8(buffer, networks[i].rssi, offset);
                    writeInt8(buffer, networks[i].channel, offset);
                    writeFixedString(buffer, networks[i].type, TYPE_SIZE, offset);
                    writeUint64(buffer, networks[i].last_seen, offset);
                    writeUint32(buffer, networks[i].times_seen, offset);
                }
            }
            else if (requestType == REQUEST_CLIENT_LIST)
            {
                std::vector<WifiDevice, DynamicPsramAllocator<WifiDevice>> devices = stationsList.getClonedList();
                size_t endIndex = std::min(startIndex + itemsPerPacket, devices.size());
                length = (endIndex - startIndex) * WIFI_DEVICE_RECORD_SIZE;
                buffer = new uint8_t[length];
                size_t offset = 0;

                for (size_t i = startIndex; i < endIndex; i++)
                {
                    writeMacAddress(buffer, devices[i].address, offset);
                    writeMacAddress(buffer, devices[i].bssid, offset);
                    writeInt8(buffer, devices[i].rssi, offset);
                    writeInt8(buffer, devices[i].channel, offset);
                    writeUint64(buffer, devices[i].last_seen, offset);
                    writeUint32(buffer, devices[i].times_seen, offset);
                }
            }
            else if (requestType == REQUEST_BLE_LIST)
            {
                std::vector<BLEFoundDevice, DynamicPsramAllocator<BLEFoundDevice>> devices = bleDeviceList.getClonedList();
                size_t endIndex = std::min(startIndex + itemsPerPacket, devices.size());
                length = (endIndex - startIndex) * BLE_DEVICE_RECORD_SIZE;
                buffer = new uint8_t[length];
                size_t offset = 0;

                for (size_t i = startIndex; i < endIndex; i++)
                {
                    writeMacAddress(buffer, devices[i].address, offset);
                    writeFixedString(buffer, devices[i].name, NAME_SIZE, offset);
                    writeInt8(buffer, devices[i].rssi, offset);
                    writeUint64(buffer, devices[i].last_seen, offset);
                    writeInt8(buffer, devices[i].isPublic ? 1 : 0, offset);
                    writeUint32(buffer, devices[i].times_seen, offset);
                }
            }

            if (buffer != nullptr)
            {
                // Add packet number header
                uint8_t header[PACKET_HEADER_SIZE];
                snprintf((char *)header, PACKET_HEADER_SIZE + 1, "%04X", packetNumber);

                // Combine header and data
                uint8_t *fullPacket = new uint8_t[PACKET_HEADER_SIZE + length];
                memcpy(fullPacket, header, PACKET_HEADER_SIZE);
                memcpy(fullPacket + PACKET_HEADER_SIZE, buffer, length);

                size_t totalLength = PACKET_HEADER_SIZE + length;
                if (totalLength > appPrefs.bleMTU) {
                    log_w("Packet size (%d) exceeds MTU (%d) - data may be truncated", 
                           totalLength, appPrefs.bleMTU);
                }
                
                pTxCharacteristic->setValue(fullPacket, totalLength);
                pTxCharacteristic->notify();
                
                delete[] fullPacket;
                delete[] buffer;

                log_d("Sent packet %d", packetNumber);

                if (packetNumber == totalPackets)
                {
                    delay(PACKET_DELAY);
                    time_t now = millis() / 1000 + base_time;
                    String endMarker = String(PACKET_END_MARKER) + ":" + String(now);
                    pTxCharacteristic->setValue(endMarker.c_str());
                    pTxCharacteristic->notify();
                    log_i("Sent end marker with timestamp: %s", endMarker.c_str());
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        log_e("EXCEPTION in sendPacket: %s", e.what());
    }
    catch (...)
    {
        log_e("UNKNOWN EXCEPTION in sendPacket");
    }
}
