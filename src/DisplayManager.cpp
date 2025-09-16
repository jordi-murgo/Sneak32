#include "DisplayManager.h"

#ifdef ENABLE_DISPLAY
#include "WifiNetworkList.h"
#include "BLEDeviceList.h"
#include "WifiDeviceList.h"

// Static member initialization
DisplayManager* DisplayManager::instance = nullptr;
TFT_eSPI* DisplayManager::tft_instance = nullptr;

DisplayManager::DisplayManager() : 
    initialized(false),
    current_mode(MODE_OVERVIEW),
    display_buffer1(nullptr),
    display_buffer2(nullptr),
    display(nullptr),
    touch_input(nullptr),
    main_screen(nullptr),
    wifi_networks_screen(nullptr),
    wifi_devices_screen(nullptr),
    ble_devices_screen(nullptr),
    status_screen(nullptr),
    wifi_networks_list(nullptr),
    wifi_devices_list(nullptr),
    ble_devices_list(nullptr),
    status_label(nullptr),
    scanning_status_label(nullptr),
    nav_tabs(nullptr)
{
    instance = this;
}

DisplayManager::~DisplayManager() {
    if (display_buffer1) {
        free(display_buffer1);
    }
    if (display_buffer2) {
        free(display_buffer2);
    }
    instance = nullptr;
    tft_instance = nullptr;
}

bool DisplayManager::begin() {
    Serial.println("Initializing display...");
    
    // Initialize LVGL
    lv_init();
    
    // Initialize TFT
    tft.init();
    tft.setRotation(1); // Landscape mode
    tft_instance = &tft;
    
    // Allocate display buffers
    display_buffer1 = (lv_color_t*)malloc(BUFFER_SIZE * sizeof(lv_color_t));
    display_buffer2 = (lv_color_t*)malloc(BUFFER_SIZE * sizeof(lv_color_t));
    
    if (!display_buffer1 || !display_buffer2) {
        Serial.println("Failed to allocate display buffers");
        return false;
    }
    
    // Initialize display buffer
    lv_disp_draw_buf_init(&display_buf, display_buffer1, display_buffer2, BUFFER_SIZE);
    
    // Initialize display driver
    lv_disp_drv_init(&display_driver);
    display_driver.hor_res = LCD_WIDTH;
    display_driver.ver_res = LCD_HEIGHT;
    display_driver.flush_cb = display_flush_cb;
    display_driver.draw_buf = &display_buf;
    display = lv_disp_drv_register(&display_driver);
    
    // Initialize touch driver (if available)
    lv_indev_drv_init(&touch_driver);
    touch_driver.type = LV_INDEV_TYPE_POINTER;
    touch_driver.read_cb = touch_read_cb;
    touch_input = lv_indev_drv_register(&touch_driver);
    
    // Create UI screens
    createMainScreen();
    createWifiNetworksScreen();
    createWifiDevicesScreen();
    createBLEDevicesScreen();
    createStatusScreen();
    createNavigationTabs();
    
    // Load the main screen
    lv_scr_load(main_screen);
    
    initialized = true;
    Serial.println("Display initialized successfully");
    return true;
}

void DisplayManager::update() {
    if (!initialized) return;
    
    lv_timer_handler(); // Handle LVGL tasks
}

void DisplayManager::createMainScreen() {
    main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_screen, lv_color_black(), 0);
    
    // Title label
    lv_obj_t* title = lv_label_create(main_screen);
    lv_label_set_text(title, "Sneak32 - WiFi & BLE Scanner");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    // Status area
    scanning_status_label = lv_label_create(main_screen);
    lv_label_set_text(scanning_status_label, "Status: Ready");
    lv_obj_set_style_text_color(scanning_status_label, lv_color_hex(0x00FF00), 0);
    lv_obj_align(scanning_status_label, LV_ALIGN_TOP_MID, 0, 60);
    
    // Create overview stats
    lv_obj_t* stats_container = lv_obj_create(main_screen);
    lv_obj_set_size(stats_container, LCD_WIDTH - 40, 200);
    lv_obj_align(stats_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(stats_container, lv_color_hex(0x2F2F2F), 0);
    lv_obj_set_style_border_width(stats_container, 2, 0);
    lv_obj_set_style_border_color(stats_container, lv_color_hex(0x555555), 0);
    
    // WiFi Networks count
    lv_obj_t* wifi_count_label = lv_label_create(stats_container);
    lv_label_set_text(wifi_count_label, "WiFi Networks: 0");
    lv_obj_set_style_text_color(wifi_count_label, lv_color_white(), 0);
    lv_obj_align(wifi_count_label, LV_ALIGN_TOP_LEFT, 20, 20);
    
    // WiFi Devices count
    lv_obj_t* wifi_devices_count_label = lv_label_create(stats_container);
    lv_label_set_text(wifi_devices_count_label, "WiFi Devices: 0");
    lv_obj_set_style_text_color(wifi_devices_count_label, lv_color_white(), 0);
    lv_obj_align(wifi_devices_count_label, LV_ALIGN_TOP_LEFT, 20, 60);
    
    // BLE Devices count
    lv_obj_t* ble_count_label = lv_label_create(stats_container);
    lv_label_set_text(ble_count_label, "BLE Devices: 0");
    lv_obj_set_style_text_color(ble_count_label, lv_color_white(), 0);
    lv_obj_align(ble_count_label, LV_ALIGN_TOP_LEFT, 20, 100);
}

void DisplayManager::createWifiNetworksScreen() {
    wifi_networks_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(wifi_networks_screen, lv_color_black(), 0);
    
    // Title
    lv_obj_t* title = lv_label_create(wifi_networks_screen);
    lv_label_set_text(title, "WiFi Networks");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Create list
    wifi_networks_list = lv_list_create(wifi_networks_screen);
    lv_obj_set_size(wifi_networks_list, LCD_WIDTH - 20, LCD_HEIGHT - 80);
    lv_obj_align(wifi_networks_list, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(wifi_networks_list, lv_color_hex(0x1F1F1F), 0);
}

void DisplayManager::createWifiDevicesScreen() {
    wifi_devices_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(wifi_devices_screen, lv_color_black(), 0);
    
    // Title
    lv_obj_t* title = lv_label_create(wifi_devices_screen);
    lv_label_set_text(title, "WiFi Devices");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Create list
    wifi_devices_list = lv_list_create(wifi_devices_screen);
    lv_obj_set_size(wifi_devices_list, LCD_WIDTH - 20, LCD_HEIGHT - 80);
    lv_obj_align(wifi_devices_list, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(wifi_devices_list, lv_color_hex(0x1F1F1F), 0);
}

void DisplayManager::createBLEDevicesScreen() {
    ble_devices_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ble_devices_screen, lv_color_black(), 0);
    
    // Title
    lv_obj_t* title = lv_label_create(ble_devices_screen);
    lv_label_set_text(title, "BLE Devices");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Create list
    ble_devices_list = lv_list_create(ble_devices_screen);
    lv_obj_set_size(ble_devices_list, LCD_WIDTH - 20, LCD_HEIGHT - 80);
    lv_obj_align(ble_devices_list, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(ble_devices_list, lv_color_hex(0x1F1F1F), 0);
}

void DisplayManager::createStatusScreen() {
    status_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(status_screen, lv_color_black(), 0);
    
    // Title
    lv_obj_t* title = lv_label_create(status_screen);
    lv_label_set_text(title, "System Status");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Status text area
    status_label = lv_label_create(status_screen);
    lv_label_set_text(status_label, "System ready...");
    lv_obj_set_style_text_color(status_label, lv_color_white(), 0);
    lv_obj_set_size(status_label, LCD_WIDTH - 40, LCD_HEIGHT - 100);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 10);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
}

void DisplayManager::createNavigationTabs() {
    // Create tab view for navigation
    nav_tabs = lv_tabview_create(main_screen, LV_DIR_BOTTOM, 50);
    lv_obj_set_size(nav_tabs, LCD_WIDTH, LCD_HEIGHT - 100);
    lv_obj_align(nav_tabs, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    // Add tabs (they will be visible at the bottom)
    lv_obj_t* tab1 = lv_tabview_add_tab(nav_tabs, "Overview");
    lv_obj_t* tab2 = lv_tabview_add_tab(nav_tabs, "WiFi Nets");
    lv_obj_t* tab3 = lv_tabview_add_tab(nav_tabs, "WiFi Devs");
    lv_obj_t* tab4 = lv_tabview_add_tab(nav_tabs, "BLE");
    lv_obj_t* tab5 = lv_tabview_add_tab(nav_tabs, "Status");
}

void DisplayManager::setDisplayMode(DisplayMode mode) {
    if (!initialized) return;
    
    current_mode = mode;
    
    switch (mode) {
        case MODE_OVERVIEW:
            switchToScreen(main_screen);
            break;
        case MODE_WIFI_NETWORKS:
            switchToScreen(wifi_networks_screen);
            break;
        case MODE_WIFI_DEVICES:
            switchToScreen(wifi_devices_screen);
            break;
        case MODE_BLE_DEVICES:
            switchToScreen(ble_devices_screen);
            break;
        case MODE_STATUS:
            switchToScreen(status_screen);
            break;
    }
}

void DisplayManager::switchToScreen(lv_obj_t* screen) {
    if (screen) {
        lv_scr_load(screen);
    }
}

void DisplayManager::updateWifiNetworks(const WifiNetworkList& networks) {
    if (!initialized || !wifi_networks_list) return;
    
    // Clear existing items
    lv_obj_clean(wifi_networks_list);
    
    // Add new items
    auto network_list = networks.getClonedList();
    for (const auto& network : network_list) {
        char item_text[128];
        snprintf(item_text, sizeof(item_text), "%s (Ch:%d, %ddBm)", 
                 network.ssid.c_str(), network.channel, network.rssi);
        
        lv_obj_t* list_item = lv_list_add_btn(wifi_networks_list, LV_SYMBOL_WIFI, item_text);
        lv_obj_set_style_text_color(list_item, lv_color_white(), 0);
    }
}

void DisplayManager::updateBLEDevices(const BLEDeviceList& devices) {
    if (!initialized || !ble_devices_list) return;
    
    // Clear existing items
    lv_obj_clean(ble_devices_list);
    
    // Add new items
    auto device_list = devices.getClonedList();
    for (const auto& device : device_list) {
        char item_text[128];
        const char* name = device.name.empty() ? "Unknown" : device.name.c_str();
        snprintf(item_text, sizeof(item_text), "%s (%s, %ddBm)", 
                 name, device.address.c_str(), device.rssi);
        
        lv_obj_t* list_item = lv_list_add_btn(ble_devices_list, LV_SYMBOL_BLUETOOTH, item_text);
        lv_obj_set_style_text_color(list_item, lv_color_white(), 0);
    }
}

void DisplayManager::updateWifiDevices(const WifiDeviceList& devices) {
    if (!initialized || !wifi_devices_list) return;
    
    // Clear existing items
    lv_obj_clean(wifi_devices_list);
    
    // Add new items
    auto device_list = devices.getClonedList();
    for (const auto& device : device_list) {
        char item_text[128];
        snprintf(item_text, sizeof(item_text), "%s (Ch:%d, %ddBm)", 
                 device.mac_address.c_str(), device.channel, device.rssi);
        
        lv_obj_t* list_item = lv_list_add_btn(wifi_devices_list, LV_SYMBOL_SETTINGS, item_text);
        lv_obj_set_style_text_color(list_item, lv_color_white(), 0);
    }
}

void DisplayManager::setStatusMessage(const char* message) {
    if (!initialized || !status_label) return;
    
    lv_label_set_text(status_label, message);
}

void DisplayManager::showScanningStatus(bool wifi_scanning, bool ble_scanning) {
    if (!initialized || !scanning_status_label) return;
    
    char status_text[64];
    if (wifi_scanning && ble_scanning) {
        snprintf(status_text, sizeof(status_text), "Status: WiFi & BLE Scanning");
    } else if (wifi_scanning) {
        snprintf(status_text, sizeof(status_text), "Status: WiFi Scanning");
    } else if (ble_scanning) {
        snprintf(status_text, sizeof(status_text), "Status: BLE Scanning");
    } else {
        snprintf(status_text, sizeof(status_text), "Status: Ready");
    }
    
    lv_label_set_text(scanning_status_label, status_text);
}

// Static callback functions
void DisplayManager::display_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    if (!tft_instance) {
        lv_disp_flush_ready(disp);
        return;
    }
    
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    tft_instance->startWrite();
    tft_instance->setAddrWindow(area->x1, area->y1, w, h);
    tft_instance->pushColors((uint16_t*)&color_p->full, w * h, true);
    tft_instance->endWrite();
    
    lv_disp_flush_ready(disp);
}

void DisplayManager::touch_read_cb(lv_indev_drv_t *indev, lv_indev_data_t *data) {
    // TODO: Implement touch reading if touch screen is available
    // For now, just indicate no touch
    data->state = LV_INDEV_STATE_REL;
}

#else // ENABLE_DISPLAY not defined

// Stub implementation when display is disabled
DisplayManager::DisplayManager() : initialized(false), current_mode(MODE_OVERVIEW) {}
DisplayManager::~DisplayManager() {}
bool DisplayManager::begin() { return false; }
void DisplayManager::update() {}
void DisplayManager::updateWifiNetworks(const WifiNetworkList& networks) {}
void DisplayManager::updateBLEDevices(const BLEDeviceList& devices) {}
void DisplayManager::updateWifiDevices(const WifiDeviceList& devices) {}
void DisplayManager::setStatusMessage(const char* message) {}
void DisplayManager::showScanningStatus(bool wifi_scanning, bool ble_scanning) {}
void DisplayManager::setDisplayMode(DisplayMode mode) {}

#endif // ENABLE_DISPLAY