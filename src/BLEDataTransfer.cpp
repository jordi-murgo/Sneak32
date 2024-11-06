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

// Variables for JSON preparation and sending
String preparedJsonData;
uint16_t totalPackets = 0;
unsigned long lastPacketRequestTime = 0;

// Replace the static MTU definition with a dynamic one
#define MAX_PACKET_SIZE (appPrefs.bleMTU - PACKET_HEADER_SIZE)

void SendDataOverBLECallbacks::onWrite(BLECharacteristic *pCharacteristic)
{
    bool only_relevant_stations = false;
    std::string value = pCharacteristic->getValue();

    if (value.length() >= 4)
    {
        uint16_t requestedPacket = strtoul(value.substr(0, 4).c_str(), NULL, 16);
        Serial.printf("Received request for packet %d\n", requestedPacket);
        if (value.length() >= 5)
        {
            only_relevant_stations = true;
        }
        sendPacket(requestedPacket, only_relevant_stations);
        lastPacketRequestTime = millis();
    }
}

String generateJsonString(boolean only_relevant_stations = false)
{
    String jsonString = "";

    auto stationsClonedList = stationsList.getClonedList();
    auto ssidsClonedList = ssidList.getClonedList();
    auto bleDevicesClonedList = bleDeviceList.getClonedList();

    jsonString = "{";

    // Add stations array
    jsonString += "\"stations\":[";
    uint32_t total_seens = 0;
    for (const auto &device : stationsClonedList)
    {
        total_seens += device.times_seen;
    }
    uint32_t min_seens = static_cast<uint32_t>(round(total_seens / static_cast<double>(stationsClonedList.size()) / 3.0));

    int stationItems = 0;
    for (size_t i = 0; i < stationsClonedList.size(); ++i)
    {
        if (only_relevant_stations && (stationsClonedList[i].times_seen < min_seens || stationsClonedList[i].rssi < appPrefs.minimal_rssi))
        {
            continue;
        }

        if (stationItems++ > 0)
            jsonString += ",";

        char station_json[256];
        snprintf(station_json, sizeof(station_json),
                 "{\"mac\":\"%s\",\"rssi\":%d,\"channel\":%d,\"last_seen\":%d,\"times_seen\":%u}",
                 stationsClonedList[i].address.toString().c_str(),
                 stationsClonedList[i].rssi,
                 stationsClonedList[i].channel,
                 stationsClonedList[i].last_seen,
                 stationsClonedList[i].times_seen);

        jsonString += String(station_json);
    }
    jsonString += "],";

    // Add SSIDs array
    jsonString += "\"ssids\":[";
    for (size_t i = 0; i < ssidsClonedList.size(); ++i)
    {
        if (only_relevant_stations && ssidsClonedList[i].rssi < appPrefs.minimal_rssi)
        {
            continue;
        }

        if (i > 0)
            jsonString += ",";
        char ssid_json[256];
        snprintf(ssid_json, sizeof(ssid_json),
                 "{\"ssid\":\"%s\",\"rssi\":%d,\"channel\":%d,\"type\":\"%s\",\"last_seen\":%d,\"times_seen\":%u}",
                 ssidsClonedList[i].ssid.c_str(),
                 ssidsClonedList[i].rssi,
                 ssidsClonedList[i].channel,
                 ssidsClonedList[i].type.c_str(),
                 ssidsClonedList[i].last_seen,
                 ssidsClonedList[i].times_seen);

        jsonString += ssid_json;
    }
    jsonString += "],";

    // Add BLE devices array
    jsonString += "\"ble_devices\":[";
    for (size_t i = 0; i < bleDevicesClonedList.size(); ++i)
    {
        if (only_relevant_stations && bleDevicesClonedList[i].rssi < appPrefs.minimal_rssi)
        {
            continue;
        }

        if (i > 0)
            jsonString += ",";
        char ble_json[256];
        snprintf(ble_json, sizeof(ble_json),
                 "{\"address\":\"%s\",\"name\":\"%s\",\"rssi\":%d,\"last_seen\":%d,\"is_public\":\"%s\",\"times_seen\":%u}",
                 bleDevicesClonedList[i].address.toString().c_str(),
                 bleDevicesClonedList[i].name.c_str(),
                 bleDevicesClonedList[i].rssi,
                 bleDevicesClonedList[i].last_seen,
                 bleDevicesClonedList[i].isPublic ? "true" : "false",
                 bleDevicesClonedList[i].times_seen);
        jsonString += ble_json;
    }
    jsonString += "],";

    // Add timestamp, counts, and free heap
    jsonString += "\"timestamp\":" + String((millis() / 1000) + base_time) + ",";
    jsonString += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    jsonString += "\"min_seens\":" + String(min_seens) + ",";
    jsonString += "\"minimal_rssi\":" + String(appPrefs.minimal_rssi) + ",";
    jsonString += "\"stations_count\":" + String(stationItems) + ",";
    jsonString += "\"ssids_count\":" + String(ssidsClonedList.size()) + ",";
    jsonString += "\"ble_devices_count\":" + String(bleDevicesClonedList.size());

    jsonString += "}";

    return jsonString;
}

void prepareJsonData(boolean only_relevant_stations)
{
    Serial.printf("Preparing JSON data with only_relevant_stations: %d\n", only_relevant_stations);
    preparedJsonData = generateJsonString(only_relevant_stations);
}

void sendPacket(uint16_t packetNumber, boolean only_relevant_stations)
{
    if (pTxCharacteristic != nullptr && deviceConnected)
    {
        if (packetNumber == 0)
        {
            prepareJsonData(only_relevant_stations);
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
