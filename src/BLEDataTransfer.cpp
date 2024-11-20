#include "BLEDataTransfer.h"
#include <BLEDevice.h>
#include <cmath>
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

// Nuevos tipos de solicitud
#define REQUEST_SSID_LIST "ssid_list"
#define REQUEST_CLIENT_LIST "client_list"
#define REQUEST_BLE_LIST "ble_list"

// Variables for JSON preparation and sending
uint16_t totalPackets = 0;
unsigned long lastPacketRequestTime = 0;
String currentRequestType;

// Replace the static MTU definition with a dynamic one
#define MAX_PACKET_SIZE (appPrefs.bleMTU - PACKET_HEADER_SIZE)

void SendDataOverBLECallbacks::onWrite(BLECharacteristic *pCharacteristic)
{
    std::string value = pCharacteristic->getValue();
    
    if (value.length() > 0)
    {
        // Debug: Print raw received value
        Serial.printf("Received BLE value: %s (length: %d)\n", value.c_str(), value.length());

        // Check if it's a 4-character hex value (potential packet number)
        if (value.length() == 4 && std::all_of(value.begin(), value.end(), ::isxdigit)) 
        {
            uint16_t requestedPacket = strtoul(value.substr(0, 4).c_str(), NULL, 16);
            
            // Debug: Log parsed packet number
            Serial.printf("Parsed packet number: %d, Current request type: %s\n", 
                          requestedPacket, currentRequestType.c_str());

            // Ensure we have a valid current request type before sending
            if (!currentRequestType.isEmpty() && 
                (currentRequestType == REQUEST_SSID_LIST || 
                 currentRequestType == REQUEST_CLIENT_LIST || 
                 currentRequestType == REQUEST_BLE_LIST))
            {
                sendPacket(requestedPacket, currentRequestType);
                lastPacketRequestTime = millis();
            }
            else 
            {
                Serial.printf("ERROR: No valid current request type for packet %d\n", requestedPacket);
                pCharacteristic->setValue("Error: No active request");
            }
        }
        else 
        {
            String requestType = String(value.c_str());
            currentRequestType = requestType;
            
            if (requestType == REQUEST_SSID_LIST || 
                requestType == REQUEST_CLIENT_LIST || 
                requestType == REQUEST_BLE_LIST)
            {
                sendPacket(0, requestType);
                lastPacketRequestTime = millis();
            }
            else
            {
                pCharacteristic->setValue("Error: Invalid request type");
                Serial.println("Invalid request type: " + requestType);
            }
        }
    }
}

String generateJsonPacket(size_t packetNumber, const String& requestType)
{
    size_t itemsPerPacket = getItemsPerPacket(requestType);
    size_t startIndex = (packetNumber - 1) * itemsPerPacket;
    String jsonData;

    // Calculate current timestamp
    time_t current_time = base_time + (millis() / 1000);

    if (requestType == REQUEST_CLIENT_LIST)
    {
        std::vector<WifiDevice> clonedList = stationsList.getClonedList();
        size_t endIndex = std::min(startIndex + itemsPerPacket, clonedList.size());
        
        if (packetNumber == 1) {
            jsonData = "{\"stations\":[";
        } else {
            jsonData = "";
        }

        for (size_t i = startIndex; i < endIndex; ++i)
        {
            if (i > 0) jsonData += ",";
            jsonData += generateStationJson(clonedList[i]);
        }

        if (packetNumber == totalPackets) {
            jsonData += "],\"timestamp\":" + String(current_time) + "}";
        }
    }
    else if (requestType == REQUEST_SSID_LIST)
    {
        std::vector<WifiNetwork> clonedList = ssidList.getClonedList();
        size_t endIndex = std::min(startIndex + itemsPerPacket, clonedList.size());
        
        if (packetNumber == 1) {
            jsonData = "{\"ssids\":[";
        } else {
            jsonData = "";
        }

        for (size_t i = startIndex; i < endIndex; ++i)
        {
            if (i > 0) jsonData += ",";
            jsonData += generateSsidJson(clonedList[i]);
        }

        if (packetNumber == totalPackets) {
            jsonData += "],\"timestamp\":" + String(current_time) + "}";
        }
    }
    else if (requestType == REQUEST_BLE_LIST)
    {
        std::vector<BLEFoundDevice> clonedList = bleDeviceList.getClonedList();
        size_t endIndex = std::min(startIndex + itemsPerPacket, clonedList.size());
        
        if (packetNumber == 1) {
            jsonData = "{\"ble_devices\":[";
        } else {
            jsonData = "";
        }

        for (size_t i = startIndex; i < endIndex; ++i)
        {
            if (i > 0) jsonData += ",";
            jsonData += generateBleDeviceJson(clonedList[i]);
        }

        if (packetNumber == totalPackets) {
            jsonData += "],\"timestamp\":" + String(current_time) + "}";
        }
    }

    return jsonData;
}

size_t getItemsPerPacket(const String& requestType)
{
    if (requestType == REQUEST_BLE_LIST)
    {
        return 2; // Adjust based on typical BLE device JSON size
    }
    
    // Dynamically calculate based on MTU and average item size
    size_t averageItemSize = (requestType == REQUEST_CLIENT_LIST) ? 120 : 155;
    size_t overhead = PACKET_HEADER_SIZE + 50; // Increased overhead for JSON array
    
    return std::max(1, static_cast<int>((MAX_PACKET_SIZE - overhead) / averageItemSize));
}

uint16_t calculateTotalPackets(const String& requestType)
{
    size_t itemsPerPacket = getItemsPerPacket(requestType);
    size_t totalItems;

    if (requestType == REQUEST_CLIENT_LIST) {
        totalItems = stationsList.getClonedList().size();
    } else if (requestType == REQUEST_SSID_LIST) {
        totalItems = ssidList.getClonedList().size();
    } else if (requestType == REQUEST_BLE_LIST) {
        totalItems = bleDeviceList.getClonedList().size();
    } else {
        return 0;
    }

    return (totalItems + itemsPerPacket - 1) / itemsPerPacket;
}

void sendPacket(uint16_t packetNumber, const String& requestType)
{
    // Debug: Log entry point
    Serial.printf("sendPacket: Start - PacketNumber: %d, RequestType: %s\n", packetNumber, requestType.c_str());

    // Validate pointers and connection
    if (pTxCharacteristic == nullptr) {
        Serial.println("ERROR: pTxCharacteristic is NULL");
        return;
    }

    if (!deviceConnected) {
        Serial.println("ERROR: Device not connected");
        return;
    }

    // Debug: Check request type validity
    if (requestType != REQUEST_SSID_LIST && 
        requestType != REQUEST_CLIENT_LIST && 
        requestType != REQUEST_BLE_LIST) {
        Serial.printf("ERROR: Invalid request type: %s\n", requestType.c_str());
        return;
    }

    try {
        if (packetNumber == 0)
        {
            // Calculate total packets with error handling
            totalPackets = calculateTotalPackets(requestType);
            Serial.printf("Total Packets: %d\n", totalPackets);

            char packetsHeader[5];
            snprintf(packetsHeader, sizeof(packetsHeader), "%04X", totalPackets);

            String startMarker = String(PACKET_START_MARKER) + String(packetsHeader);
            
            // Debug: Log start marker details
            Serial.printf("Start Marker: %s, Length: %d\n", startMarker.c_str(), startMarker.length());

            pTxCharacteristic->setValue(startMarker.c_str());
            pTxCharacteristic->notify();
            Serial.println("Sent " + startMarker);
        }
        else if (packetNumber <= totalPackets)
        {
            // Debug: Log packet generation attempt
            Serial.printf("Generating JSON for Packet %d\n", packetNumber);

            // Generate JSON data for this packet on-the-fly
            String packetData = generateJsonPacket(packetNumber, requestType);
            
            // Debug: Log generated packet data
            Serial.printf("Packet Data Length: %d\n", packetData.length());

            // Add packet number header
            char packetHeader[PACKET_HEADER_SIZE + 1];
            snprintf(packetHeader, sizeof(packetHeader), "%04X", packetNumber);

            // Combine header and data, ensuring we don't exceed MTU
            String fullPacket = String(packetHeader) + packetData;
            
            // Debug: Log full packet details
            Serial.printf("Full Packet Length: %d, MTU: %d\n", fullPacket.length(), appPrefs.bleMTU);

            if (fullPacket.length() > appPrefs.bleMTU)
            {
                Serial.printf("WARNING: Packet %d exceeds MTU (%d > %d)\n",
                    packetNumber, fullPacket.length(), appPrefs.bleMTU);
                fullPacket = fullPacket.substring(0, appPrefs.bleMTU);
            }

            // Debug: Attempt to set value and notify
            Serial.println("Attempting to set characteristic value");
            pTxCharacteristic->setValue(fullPacket.c_str());
            
            Serial.println("Attempting to notify");
            pTxCharacteristic->notify();
            
            Serial.printf("Sent packet %d\n", packetNumber);

            if (packetNumber == totalPackets)
            {
                delay(PACKET_DELAY);
                pTxCharacteristic->setValue(PACKET_END_MARKER);
                pTxCharacteristic->notify();
                Serial.println("Sent " + String(PACKET_END_MARKER));
            }
        }
        else 
        {
            Serial.printf("ERROR: Packet number %d exceeds total packets %d\n", packetNumber, totalPackets);
        }
    }
    catch (const std::exception& e) 
    {
        Serial.printf("EXCEPTION in sendPacket: %s\n", e.what());
    }
    catch (...) 
    {
        Serial.println("UNKNOWN EXCEPTION in sendPacket");
    }

    // Debug: Log exit point
    Serial.println("sendPacket: End");
}

void checkTransmissionTimeout()
{
    unsigned long currentTime = millis();
    if ((currentTime - lastPacketRequestTime > TRANSMISSION_TIMEOUT) || !deviceConnected)
    {
        // Reset current request type if transmission has timed out
        currentRequestType = "";
        Serial.println("Transmission timeout: Resetting current request type");
    }
}

String generateStationJson(const WifiDevice& device)
{
    char station_json[256];
    snprintf(station_json, sizeof(station_json),
             "{\"mac\":\"%s\",\"bssid\":\"%s\",\"rssi\":%d,\"channel\":%d,\"last_seen\":%d,\"times_seen\":%u}",
             device.address.toString().c_str(),
             device.bssid.toString().c_str(),
             device.rssi,
             device.channel,
             device.last_seen,
             device.times_seen);
    return String(station_json);
}

String generateSsidJson(const WifiNetwork& network) {
    char ssid_json[256];
    snprintf(ssid_json, sizeof(ssid_json),
             "{\"ssid\":\"%s\",\"mac\":\"%s\",\"rssi\":%d,\"channel\":%d,\"type\":\"%s\",\"last_seen\":%d,\"times_seen\":%u}",
             network.ssid.c_str(),
             network.address.toString().c_str(),
             network.rssi,
             network.channel,
             network.type.c_str(),
             network.last_seen,
             network.times_seen);
    return String(ssid_json);
}

String generateBleDeviceJson(const BLEFoundDevice& device) {
    char ble_json[256];
    snprintf(ble_json, sizeof(ble_json),
             "{\"mac\":\"%s\",\"name\":\"%s\",\"rssi\":%d,\"last_seen\":%d,\"is_public\":\"%s\",\"times_seen\":%u}",
             device.address.toString().c_str(),
             device.name.c_str(),
             device.rssi,
             device.last_seen,
             device.isPublic ? "true" : "false",
             device.times_seen);
    return String(ble_json);
}
