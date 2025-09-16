#include "DisplayManager.h"

#ifdef ENABLE_DISPLAY

#include <algorithm>
#include <cstring>
#include <utility>

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <lvgl.h>

#include "ESP_Panel_Library.h"

namespace
{
constexpr const char *kTag = "DisplayManager";
std::string makeSafeString(const char *value)
{
    if (!value)
    {
        return {};
    }
    return std::string(value, strnlen(value, 64));
}

} // namespace

DisplayManager::DisplayManager() = default;

void DisplayManager::begin()
{
    if (initialized_)
    {
        return;
    }

    if (!board_.init())
    {
        ESP_LOGE(kTag, "Failed to initialise display board");
        return;
    }

    if (!board_.begin())
    {
        ESP_LOGE(kTag, "Failed to start display board");
        return;
    }

    lcd_ = board_.getLCD();
    touch_ = board_.getTouch();

    if (!lcd_)
    {
        ESP_LOGE(kTag, "LCD driver not available");
        return;
    }

    lv_init();

    const int width = std::max(1, lcd_->getFrameWidth());
    const int height = std::max(1, lcd_->getFrameHeight());

    bufferSizeBytes_ = width * kBufferLines * sizeof(lv_color_t);
    drawBuffer1_ = static_cast<lv_color_t *>(heap_caps_malloc(bufferSizeBytes_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!drawBuffer1_)
    {
        drawBuffer1_ = static_cast<lv_color_t *>(heap_caps_malloc(bufferSizeBytes_, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    drawBuffer2_ = static_cast<lv_color_t *>(heap_caps_malloc(bufferSizeBytes_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (!drawBuffer1_)
    {
        ESP_LOGE(kTag, "Failed to allocate LVGL draw buffer");
        return;
    }

    display_ = lv_display_create(width, height);
    lv_display_set_flush_cb(display_, flushCallback);
    lv_display_set_user_data(display_, this);
    lv_display_set_buffers(display_, drawBuffer1_, drawBuffer2_, bufferSizeBytes_, LV_DISPLAY_RENDER_MODE_PARTIAL);

    if (touch_)
    {
        inputDevice_ = lv_indev_create();
        lv_indev_set_type(inputDevice_, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(inputDevice_, touchReadCallback);
        lv_indev_set_user_data(inputDevice_, this);
    }

    mutex_ = xSemaphoreCreateRecursiveMutex();
    if (!mutex_)
    {
        ESP_LOGE(kTag, "Failed to create LVGL mutex");
        return;
    }

    esp_timer_create_args_t timerArgs = {
        .callback = &DisplayManager::tickCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lv_tick"
    };
    if (esp_timer_create(&timerArgs, &tickTimer_) != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to create LVGL tick timer");
        return;
    }
    esp_timer_start_periodic(tickTimer_, kTickPeriodMs * 1000ULL);

    BaseType_t res = xTaskCreatePinnedToCore(lvglTask, "lvgl", 4096, this, 2, &taskHandle_, 1);
    if (res != pdPASS)
    {
        ESP_LOGE(kTag, "Failed to create LVGL task");
        return;
    }

    initialized_ = true;
}

void DisplayManager::loop()
{
    // No-op: LVGL runs in its own task
}

void DisplayManager::setDeviceConnected(bool connected)
{
    deviceConnected_ = connected;
}

void DisplayManager::refresh(const WifiNetworkList &networks,
                             const WifiDeviceList &stations,
                             const BLEDeviceList &bleDevices,
                             const AppPreferencesData &prefs,
                             int currentChannel,
                             bool detectionAlarm,
                             const std::string &detectionTarget)
{
    if (!initialized_)
    {
        return;
    }

    const uint32_t now = millis();
    if (now - lastRefreshMs_ < kRefreshIntervalMs && lastRefreshMs_ != 0)
    {
        return;
    }
    lastRefreshMs_ = now;


    constexpr std::size_t kMaxRows = 8;
    auto wifiNetworks = networks.getClonedList();
    auto wifiStations = stations.getClonedList();
    auto bleList = bleDevices.getClonedList();

    std::vector<WifiNetworkView> wifiView;
    wifiView.reserve(wifiNetworks.size());
    for (const auto &network : wifiNetworks)
    {
        WifiNetworkView view;
        view.ssid = network.ssid.c_str();
        view.rssi = network.rssi;
        view.channel = network.channel;
        view.type = network.type.c_str();
        view.timesSeen = network.times_seen;
        wifiView.push_back(std::move(view));
    }
    std::sort(wifiView.begin(), wifiView.end(), [](const WifiNetworkView &a, const WifiNetworkView &b) {
        if (a.rssi != b.rssi)
        {
            return a.rssi > b.rssi;
        }
        return a.timesSeen > b.timesSeen;
    });
    if (wifiView.size() > kMaxRows)
    {
        wifiView.resize(kMaxRows);
    }

    std::vector<WifiDeviceView> stationView;
    stationView.reserve(wifiStations.size());
    for (const auto &station : wifiStations)
    {
        WifiDeviceView view;
        view.mac = station.address.toString();
        view.rssi = station.rssi;
        view.channel = station.channel;
        view.timesSeen = station.times_seen;
        stationView.push_back(std::move(view));
    }
    std::sort(stationView.begin(), stationView.end(), [](const WifiDeviceView &a, const WifiDeviceView &b) {
        if (a.rssi != b.rssi)
        {
            return a.rssi > b.rssi;
        }
        return a.timesSeen > b.timesSeen;
    });
    if (stationView.size() > kMaxRows)
    {
        stationView.resize(kMaxRows);
    }

    std::vector<BleDeviceView> bleView;
    bleView.reserve(bleList.size());
    for (const auto &device : bleList)
    {
        BleDeviceView view;
        view.name = device.name.c_str();
        view.address = device.address.toString();
        view.isPublic = device.isPublic;
        view.rssi = device.rssi;
        view.timesSeen = device.times_seen;
        bleView.push_back(std::move(view));
    }
    std::sort(bleView.begin(), bleView.end(), [](const BleDeviceView &a, const BleDeviceView &b) {
        if (a.rssi != b.rssi)
        {
            return a.rssi > b.rssi;
        }
        return a.timesSeen > b.timesSeen;
    });
    if (bleView.size() > kMaxRows)
    {
        bleView.resize(kMaxRows);
    }

    DisplaySummaryInfo summary;
    summary.wifiNetworkCount = wifiNetworks.size();
    summary.wifiDeviceCount = wifiStations.size();
    summary.bleDeviceCount = bleList.size();
    summary.operationMode = prefs.operation_mode;
    summary.passiveScan = prefs.passive_scan;
    summary.stealthMode = prefs.stealth_mode;
    summary.deviceConnected = deviceConnected_;
    summary.detectionAlarm = detectionAlarm;
    summary.currentChannel = currentChannel;
    summary.detectionTarget = detectionTarget;
    summary.deviceName = makeSafeString(prefs.device_name);
    summary.lastRefreshMillis = now;

    updateUI(summary, std::move(wifiView), std::move(stationView), std::move(bleView));
}

void DisplayManager::updateUI(const DisplaySummaryInfo &summary,
                              std::vector<WifiNetworkView> networks,
                              std::vector<WifiDeviceView> stations,
                              std::vector<BleDeviceView> bleDevices)
{
    if (!mutex_)
    {
        return;
    }

    if (xSemaphoreTakeRecursive(mutex_, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        ui_.update(summary, networks, stations, bleDevices);
        xSemaphoreGiveRecursive(mutex_);
    }
}

void DisplayManager::lvglTask(void *param)
{
    auto *manager = static_cast<DisplayManager *>(param);
    while (true)
    {
        if (manager->mutex_ && xSemaphoreTakeRecursive(manager->mutex_, portMAX_DELAY) == pdTRUE)
        {
            lv_timer_handler();
            xSemaphoreGiveRecursive(manager->mutex_);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void DisplayManager::tickCallback(void *param)
{
    (void)param;
    lv_tick_inc(kTickPeriodMs);
}

void DisplayManager::flushCallback(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap)
{
    auto *manager = static_cast<DisplayManager *>(lv_display_get_user_data(disp));
    if (!manager || !manager->lcd_)
    {
        lv_disp_flush_ready(disp);
        return;
    }

    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;
    manager->lcd_->drawBitmap(area->x1, area->y1, w, h, pxMap);
    lv_disp_flush_ready(disp);
}

void DisplayManager::touchReadCallback(lv_indev_t *indev, lv_indev_data_t *data)
{
    auto *manager = static_cast<DisplayManager *>(lv_indev_get_user_data(indev));
    if (!manager || !manager->touch_)
    {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    esp_panel::drivers::TouchPoint point;
    int count = manager->touch_->readPoints(&point, 1, 0);
    if (count > 0)
    {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = point.x;
        data->point.y = point.y;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

DisplayManager displayManager;

#endif // ENABLE_DISPLAY
