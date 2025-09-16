#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>

#ifdef ENABLE_DISPLAY
#include <lvgl.h>
#include <TFT_eSPI.h> // Hardware-specific library for displays
#endif

// Forward declarations
class WifiNetworkList;
class BLEDeviceList;
class WifiDeviceList;

class DisplayManager {
public:
    DisplayManager();
    ~DisplayManager();
    
    bool begin();
    void update();
    void updateWifiNetworks(const WifiNetworkList& networks);
    void updateBLEDevices(const BLEDeviceList& devices);
    void updateWifiDevices(const WifiDeviceList& devices);
    void setStatusMessage(const char* message);
    void showScanningStatus(bool wifi_scanning, bool ble_scanning);
    
    // Display modes
    enum DisplayMode {
        MODE_OVERVIEW,
        MODE_WIFI_NETWORKS,
        MODE_WIFI_DEVICES,
        MODE_BLE_DEVICES,
        MODE_STATUS
    };
    
    void setDisplayMode(DisplayMode mode);
    DisplayMode getCurrentMode() const { return current_mode; }
    
    // Static callback functions for LVGL
    static void display_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
    static void touch_read_cb(lv_indev_drv_t *indev, lv_indev_data_t *data);
    
private:
#ifdef ENABLE_DISPLAY
    // LVGL display and input device drivers
    lv_disp_drv_t display_driver;
    lv_indev_drv_t touch_driver;
    lv_disp_t* display;
    lv_indev_t* touch_input;
    
    // Display buffer
    static const size_t BUFFER_SIZE = (LCD_WIDTH * LCD_HEIGHT / 10);
    lv_color_t* display_buffer1;
    lv_color_t* display_buffer2;
    lv_disp_draw_buf_t display_buf;
    
    // TFT instance
    TFT_eSPI tft;
    
    // UI objects
    lv_obj_t* main_screen;
    lv_obj_t* wifi_networks_screen;
    lv_obj_t* wifi_devices_screen;
    lv_obj_t* ble_devices_screen;
    lv_obj_t* status_screen;
    
    // Lists and labels
    lv_obj_t* wifi_networks_list;
    lv_obj_t* wifi_devices_list;
    lv_obj_t* ble_devices_list;
    lv_obj_t* status_label;
    lv_obj_t* scanning_status_label;
    
    // Navigation
    lv_obj_t* nav_tabs;
    
    DisplayMode current_mode;
    
    // Private methods
    void createMainScreen();
    void createWifiNetworksScreen();
    void createWifiDevicesScreen();
    void createBLEDevicesScreen();
    void createStatusScreen();
    void createNavigationTabs();
    
    void switchToScreen(lv_obj_t* screen);
    
    // Static instances for callbacks
    static DisplayManager* instance;
    static TFT_eSPI* tft_instance;
#endif
    
    bool initialized;
};

#endif // DISPLAY_MANAGER_H