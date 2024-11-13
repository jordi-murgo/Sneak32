#include "BLEDataTransfer.h"
#include <BLEDevice.h>
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
String preparedJsonData;
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
        String requestType = String(value.c_str());
        currentRequestType = requestType;
        
        if (requestType == REQUEST_SSID_LIST || 
            requestType == REQUEST_CLIENT_LIST || 
            requestType == REQUEST_BLE_LIST)
        {
            sendPacket(0, requestType);
            lastPacketRequestTime = millis();
        }
        else if (value.length() == 4)
        {
            uint16_t requestedPacket = strtoul(value.substr(0, 4).c_str(), NULL, 16);
            sendPacket(requestedPacket, currentRequestType);
            lastPacketRequestTime = millis();
        }
        else
        {
            pCharacteristic->setValue("Error: Invalid request type or packet number");
            Serial.println("Invalid request type or packet number");
        }
    }
}

String generateJsonString(const String& requestType)
{
    String jsonString = "{";

    if (requestType == REQUEST_CLIENT_LIST)
    {
        auto stationsClonedList = stationsList.getClonedList();
        jsonString += "\"stations\":[";
        for (size_t i = 0; i < stationsClonedList.size(); ++i)
        {
            if (i > 0) jsonString += ",";
            char station_json[256];
            snprintf(station_json, sizeof(station_json),
                     "{\"mac\":\"%s\",\"bssid\":\"%s\",\"rssi\":%d,\"channel\":%d,\"last_seen\":%d,\"times_seen\":%u}",
                     stationsClonedList[i].address.toString().c_str(),
                     stationsClonedList[i].bssid.toString().c_str(),
                     stationsClonedList[i].rssi,
                     stationsClonedList[i].channel,
                     stationsClonedList[i].last_seen,
                     stationsClonedList[i].times_seen);
            jsonString += String(station_json);
        }
        jsonString += "]";
    }
    else if (requestType == REQUEST_SSID_LIST)
    {
        auto ssidsClonedList = ssidList.getClonedList();
        jsonString += "\"ssids\":[";
        for (size_t i = 0; i < ssidsClonedList.size(); ++i)
        {
            if (i > 0) jsonString += ",";
            char ssid_json[256];
            snprintf(ssid_json, sizeof(ssid_json),
                     "{\"ssid\":\"%s\",\"mac\":\"%s\",\"rssi\":%d,\"channel\":%d,\"type\":\"%s\",\"last_seen\":%d,\"times_seen\":%u}",
                     ssidsClonedList[i].ssid.c_str(),
                     ssidsClonedList[i].address.toString().c_str(),
                     ssidsClonedList[i].rssi,
                     ssidsClonedList[i].channel,
                     ssidsClonedList[i].type.c_str(),
                     ssidsClonedList[i].last_seen,
                     ssidsClonedList[i].times_seen);
            jsonString += ssid_json;
        }
        jsonString += "]";
    }
    else if (requestType == REQUEST_BLE_LIST)
    {
        auto bleDevicesClonedList = bleDeviceList.getClonedList();
        jsonString += "\"ble_devices\":[";
        for (size_t i = 0; i < bleDevicesClonedList.size(); ++i)
        {
            if (i > 0) jsonString += ",";
            char ble_json[256];
            snprintf(ble_json, sizeof(ble_json),
                     "{\"mac\":\"%s\",\"name\":\"%s\",\"rssi\":%d,\"last_seen\":%d,\"is_public\":\"%s\",\"times_seen\":%u}",
                     bleDevicesClonedList[i].address.toString().c_str(),
                     bleDevicesClonedList[i].name.c_str(),
                     bleDevicesClonedList[i].rssi,
                     bleDevicesClonedList[i].last_seen,
                     bleDevicesClonedList[i].isPublic ? "true" : "false",
                     bleDevicesClonedList[i].times_seen);
            jsonString += ble_json;
        }
        jsonString += "]";
    }

    // Añadir timestamp y metadata común
    jsonString += ",\"timestamp\":" + String((millis() / 1000) + base_time);
    jsonString += ",\"free_heap\":" + String(ESP.getFreeHeap());
    jsonString += "}";

    return jsonString;
}

void prepareJsonData(const String& requestType)
{
    Serial.printf("Preparing JSON data for request type: %s\n", requestType.c_str());
    preparedJsonData = generateJsonString(requestType);
}

void sendPacket(uint16_t packetNumber, const String& requestType)
{
    if (pTxCharacteristic != nullptr && deviceConnected)
    {
        if (packetNumber == 0)
        {
            prepareJsonData(requestType);
            totalPackets = 1 + preparedJsonData.length() / MAX_PACKET_SIZE;

            char packetsHeader[5];
            snprintf(packetsHeader, sizeof(packetsHeader), "%04X", totalPackets);

            String startMarker = PACKET_START_MARKER + String(packetsHeader);
            pTxCharacteristic->setValue(startMarker.c_str());
            pTxCharacteristic->notify();
            Serial.println("Sent " + startMarker);
        }
        else if (packetNumber <= totalPackets)
        {
            size_t startIndex = (packetNumber - 1) * MAX_PACKET_SIZE;
            size_t endIndex = min(startIndex + MAX_PACKET_SIZE, preparedJsonData.length());
            String chunk = preparedJsonData.substring(startIndex, endIndex);

            char packetHeader[PACKET_HEADER_SIZE + 1];
            snprintf(packetHeader, sizeof(packetHeader), "%04X", packetNumber);
            String packetData = String(packetHeader) + chunk;

            pTxCharacteristic->setValue(packetData.c_str());
            pTxCharacteristic->notify();
            Serial.println("Sent packet " + String(packetNumber));

            if (packetNumber == totalPackets)
            {
                preparedJsonData.clear();
                delay(PACKET_DELAY);
                pTxCharacteristic->setValue(PACKET_END_MARKER);
                pTxCharacteristic->notify();
                Serial.println("Sent " + String(PACKET_END_MARKER));
            }
        }
    }
}

void checkTransmissionTimeout()
{
    if (millis() - lastPacketRequestTime > TRANSMISSION_TIMEOUT || !deviceConnected)
    {
        preparedJsonData.clear();
    }
}
