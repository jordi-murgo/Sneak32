#pragma once

#ifdef ENABLE_DISPLAY

#include <cstdint>
#include <string>

struct WifiNetworkView
{
    std::string ssid;
    int rssi = 0;
    int channel = 0;
    std::string type;
    uint32_t timesSeen = 0;
};

struct WifiDeviceView
{
    std::string mac;
    int rssi = 0;
    int channel = 0;
    uint32_t timesSeen = 0;
};

struct BleDeviceView
{
    std::string name;
    std::string address;
    bool isPublic = false;
    int rssi = 0;
    uint32_t timesSeen = 0;
};

struct DisplaySummaryInfo
{
    size_t wifiNetworkCount = 0;
    size_t wifiDeviceCount = 0;
    size_t bleDeviceCount = 0;
    uint8_t operationMode = 0;
    bool passiveScan = false;
    bool stealthMode = false;
    bool detectionAlarm = false;
    bool deviceConnected = false;
    int currentChannel = -1;
    std::string detectionTarget;
    std::string deviceName;
    uint32_t lastRefreshMillis = 0;
};

#endif // ENABLE_DISPLAY
