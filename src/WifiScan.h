#pragma once

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>

class WifiScanClass {
public:
    WifiScanClass();
    void setup();
    void start();
    void stop();
    void setChannel(int channel);
    void setFilter(bool onlyManagementFrames);

private:
    static void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type);
    void handle_rx(void *buf, wifi_promiscuous_pkt_type_t type);
    void process_management_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel);
    void process_control_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel);
    void process_data_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel);
    void parse_ssid(const uint8_t *payload, int payload_len, uint8_t subtype, char ssid[33]);
    static WifiScanClass* instance;
};

extern WifiScanClass WifiScanner;
