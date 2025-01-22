#pragma once

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <vector>
#include <mutex>

#include "MacAddress.h"

class WifiDetectClass
{
public:
    WifiDetectClass();
    void setup();
    void start();
    void stop();
    void setChannel(int channel);
    void setFilter(bool onlyManagementFrames);
    void cleanDetectionData();

    void setupAP(const char *ssid, const char *password, int channel);
    std::vector<MacAddress> getDetectedDevices();
    std::vector<String> getDetectedNetworks();
    time_t getLastDetectionTime();
    bool isSomethingDetected();
    size_t getDetectedDevicesCount();
    size_t getDetectedNetworksCount();
    esp_event_handler_instance_t instance_any_id;

private:
    static void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type);
    void handle_rx(void *buf, wifi_promiscuous_pkt_type_t type);
    void process_management_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel);
    void process_control_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel);
    void process_data_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel);
    void parse_ssid(const uint8_t *payload, int payload_len, uint8_t subtype, char ssid[33]);
    void addDetectedNetwork(const String &ssid);
    void addDetectedDevice(const MacAddress &device);
    std::vector<MacAddress> detectedDevices;
    std::vector<String> detectedNetworks;
    time_t lastDetectionTime;

    void registerWifiEventHandlers();
    void deregisterWifiEventHandlers();

    std::mutex detectionMutex;
    static WifiDetectClass *instance;
};

extern WifiDetectClass WifiDetector;
