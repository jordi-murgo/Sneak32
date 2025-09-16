#pragma once

#ifdef ENABLE_DISPLAY

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_timer.h>
#include <vector>

#include "DisplayUI.h"
#include "WifiNetworkList.h"
#include "WifiDeviceList.h"
#include "BLEDeviceList.h"
#include "AppPreferences.h"

class DisplayManager
{
public:
    DisplayManager();
    void begin();
    void loop();
    void refresh(const WifiNetworkList &networks,
                 const WifiDeviceList &stations,
                 const BLEDeviceList &bleDevices,
                 const AppPreferencesData &prefs,
                 int currentChannel,
                 bool detectionAlarm,
                 const std::string &detectionTarget);

    void setDeviceConnected(bool connected);

private:
    static constexpr uint32_t kTickPeriodMs = 5;
    static constexpr uint32_t kRefreshIntervalMs = 1500;
    static constexpr uint32_t kBufferLines = 60;

    static void lvglTask(void *param);
    static void tickCallback(void *param);
    static void flushCallback(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap);
    static void touchReadCallback(lv_indev_t *indev, lv_indev_data_t *data);

    void updateUI(const DisplaySummaryInfo &summary,
                  std::vector<WifiNetworkView> networks,
                  std::vector<WifiDeviceView> stations,
                  std::vector<BleDeviceView> bleDevices);

    bool initialized_ = false;
    bool deviceConnected_ = false;
    uint32_t lastRefreshMs_ = 0;

    esp_panel::board::Board board_;
    esp_panel::drivers::LCD *lcd_ = nullptr;
    esp_panel::drivers::Touch *touch_ = nullptr;
    lv_display_t *display_ = nullptr;
    lv_indev_t *inputDevice_ = nullptr;

    lv_color_t *drawBuffer1_ = nullptr;
    lv_color_t *drawBuffer2_ = nullptr;
    size_t bufferSizeBytes_ = 0;

    SemaphoreHandle_t mutex_ = nullptr;
    TaskHandle_t taskHandle_ = nullptr;
    esp_timer_handle_t tickTimer_ = nullptr;

    DisplayUI ui_;
};

extern DisplayManager displayManager;

#endif // ENABLE_DISPLAY
