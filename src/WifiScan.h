#ifndef WIFI_SCAN_H
#define WIFI_SCAN_H

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>

void setupWiFi();
void startWiFiScan();
void stopWiFiScan();
void setWiFiChannel(int channel);
void setWiFiFilter(bool onlyManagementFrames);

#endif // WIFI_SCAN_H
