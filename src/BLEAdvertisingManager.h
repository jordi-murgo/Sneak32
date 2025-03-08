/**
 * @file BLEAdvertisingManager.h
 * @brief Manages BLE advertising functionality
 */

#ifndef BLE_ADVERTISING_MANAGER_H
#define BLE_ADVERTISING_MANAGER_H

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>
#include "AppPreferences.h"
#include "BLE.h" // Include BLE.h to use the UUIDs defined there
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Service UUIDs for advertising (using the ones from BLE.h)
// Using UUIDs from BLE.h for consistency with the web client
// These are local UUIDs only used for advertising if needed
#define ADV_SCANNER_SERVICE_UUID 0xFFF1
#define ADV_COMMAND_UUID 0xFFF2
#define ADV_RESPONSE_UUID 0xFFF3

extern AppPreferencesData appPrefs;

/**
 * @brief Manager class for BLE advertising
 * 
 * This class handles all aspects of BLE advertising, including
 * initialization, configuration, and state management.
 */
class BLEAdvertisingManager {
public:
    /**
     * @brief Initialize the BLE advertising
     * 
     * Sets up the advertising object and configures it based on preferences
     */
    static void setup();
    
    /**
     * @brief Start BLE advertising
     */
    static void start();
    
    /**
     * @brief Stop BLE advertising
     */
    static void stop();

    /**
     * @brief Configure stealth mode for advertising
     * 
     * Sets up directed advertising to a specific authorized device
     */
    static void configureStealthMode();
    
    /**
     * @brief Configure normal mode for advertising
     * 
     * Sets up general discoverable advertising without starting it
     */
    static void configureNormalMode();

    /**
     * @brief Start advertising with current configuration
     * 
     * @return true if advertising started successfully, false otherwise
     */
    static bool startAdvertising();

private:
    /**
     * @brief The BLEAdvertising object
     */
    static BLEAdvertising* pAdvertising;

    /**
     * @brief Advertisement data object
     */
    static BLEAdvertisementData advData;

    /**
     * @brief Flag to track if the advertising has been initialized
     */
    static bool isInitialized;

    /**
     * @brief Current advertising mode
     */
    static uint8_t advertisingMode;

    /**
     * @brief Static buffer for GAP event data
     */
    static uint8_t gapEventBuffer[32];

    /**
     * @brief Flag to track if advertising is active
     */
    static bool isAdvertising;

    /**
     * @brief Handle GAP event safely
     * 
     * @param event GAP event to handle
     * @param param Event parameters
     */
    static void handleGAPEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param);

    /**
     * @brief Register GAP event handler
     */
    static void registerGAPEventHandler();

    /**
     * @brief Semaphore for advertising synchronization
     */
    static SemaphoreHandle_t advertisingSemaphore;

    /**
     * @brief Task handle for advertising task
     */
    static TaskHandle_t advertisingTaskHandle;

    /**
     * @brief FreeRTOS task for handling advertising
     */
    static void advertisingTask(void* parameter);


    
    /**
     * @brief Get advertisement data
     * 
     * @return BLEAdvertisementData object with device name and service UUID
     */
    static BLEAdvertisementData getAdvertisementData();
    
    /**
     * @brief Create a minimal scan response data object
     * 
     * @return BLEAdvertisementData object with minimal data
     */
    static BLEAdvertisementData createMinimalScanResponseData();
    
    /**
     * @brief Disable scan response data
     * 
     * This method disables scan response data as a last resort.
     */
    static void disableScanResponseData();
};

#endif // BLE_ADVERTISING_MANAGER_H
