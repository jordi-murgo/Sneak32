#pragma once

#ifdef ENABLE_DISPLAY

#include <vector>
#include <string>
#include <lvgl.h>

#include "DisplayTypes.h"

class DisplayUI
{
public:
    DisplayUI() = default;
    void create(const DisplaySummaryInfo &summary);
    void update(const DisplaySummaryInfo &summary,
                const std::vector<WifiNetworkView> &networks,
                const std::vector<WifiDeviceView> &stations,
                const std::vector<BleDeviceView> &bleDevices);

private:
    void createLayout(const DisplaySummaryInfo &summary);
    void updateSummary(const DisplaySummaryInfo &summary);
    void updateWifiTable(const std::vector<WifiNetworkView> &networks);
    void updateStationTable(const std::vector<WifiDeviceView> &stations);
    void updateBleTable(const std::vector<BleDeviceView> &devices);

    lv_obj_t *createStatCard(lv_obj_t *parent, const char *title, lv_color_t accentColor);
    lv_obj_t *createTableSection(lv_obj_t *parent, const char *title, uint8_t columnCount);
    void configureTableHeader(lv_obj_t *table, const std::vector<std::string> &headers);

    static std::string formatCount(size_t value);
    static std::string formatRssi(int rssi);
    static std::string formatTimes(uint32_t times);
    static std::string formatChannel(int channel);
    static std::string formatDuration(uint32_t millis);
    static std::string formatBleName(const BleDeviceView &device);

    bool created_ = false;
    lv_obj_t *screen_ = nullptr;
    lv_obj_t *modeBadge_ = nullptr;
    lv_obj_t *deviceNameLabel_ = nullptr;
    lv_obj_t *wifiCountLabel_ = nullptr;
    lv_obj_t *stationCountLabel_ = nullptr;
    lv_obj_t *bleCountLabel_ = nullptr;
    lv_obj_t *lastUpdateLabel_ = nullptr;
    lv_obj_t *channelLabel_ = nullptr;
    lv_obj_t *detectionLabel_ = nullptr;
    lv_obj_t *wifiTable_ = nullptr;
    lv_obj_t *stationTable_ = nullptr;
    lv_obj_t *bleTable_ = nullptr;
};

#endif // ENABLE_DISPLAY
