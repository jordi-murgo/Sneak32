/**
 * @file BLEAdvertisingManager.cpp
 * @brief Implementation of BLE advertising manager
 */

#include "BLEAdvertisingManager.h"
#include "BLEDeviceWrapper.h"
#include "AppPreferences.h"
#include "Logging.h"
#include <string>

// Static member initialization
BLEAdvertising* BLEAdvertisingManager::pAdvertising = nullptr;
BLEAdvertisementData BLEAdvertisingManager::advData;
uint8_t BLEAdvertisingManager::advertisingMode = 0; // Default to ADV_TYPE_IND
bool BLEAdvertisingManager::isInitialized = false;
bool BLEAdvertisingManager::isAdvertising = false;
uint8_t BLEAdvertisingManager::gapEventBuffer[32] = {0};
SemaphoreHandle_t BLEAdvertisingManager::advertisingSemaphore = nullptr;
TaskHandle_t BLEAdvertisingManager::advertisingTaskHandle = nullptr;

void BLEAdvertisingManager::handleGAPEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    if (!param) {
        log_e("GAP event parameter is null");
        return;
    }

    // Usar buffer estático para datos del evento
    memset(gapEventBuffer, 0, sizeof(gapEventBuffer));
    
    switch (event) {
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                log_i("Advertising started successfully");
                isAdvertising = true;
            } else {
                log_e("Advertising start failed: %d", param->adv_start_cmpl.status);
                isAdvertising = false;
            }
            break;
            
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                log_i("Advertising stopped successfully");
                isAdvertising = false;
            } else {
                log_e("Advertising stop failed: %d", param->adv_stop_cmpl.status);
            }
            break;
            
        default:
            break;
    }
}

void BLEAdvertisingManager::registerGAPEventHandler() {
    esp_err_t ret = esp_ble_gap_register_callback(handleGAPEvent);
    if (ret != ESP_OK) {
        log_e("Failed to register GAP event handler: %d", ret);
    } else {
        log_i("GAP event handler registered successfully");
    }
}

/**
 * @brief Setup the BLE advertising
 * 
 * This method initializes the BLE advertising object and configures it for normal mode.
 * It includes robust error handling and initialization checks.
 */
void BLEAdvertisingManager::advertisingTask(void* parameter) {
    const int MAX_RETRIES = 3;
    const TickType_t RETRY_DELAY = pdMS_TO_TICKS(1000); // 1 second delay between retries
    
    for (int retry = 0; retry < MAX_RETRIES; retry++) {
        log_i("Advertising task attempt %d/%d", retry + 1, MAX_RETRIES);
        
        try {
            if (pAdvertising == nullptr) {
                log_i("Getting BLE advertising object");
                pAdvertising = BLEDeviceWrapper::getAdvertising();
            }
            
            if (pAdvertising != nullptr) {
                // Configure and start advertising
                pAdvertising->stop();
                
                // Disable scan response to avoid heap corruption
                pAdvertising->setScanResponse(false);
                
                // Use minimal configuration
                BLEAdvertisementData minimalData;
                std::string deviceName = BLEDeviceWrapper::getInitializedName();
                if (!deviceName.empty()) {
                    minimalData.setName(deviceName);
                } else {
                    minimalData.setName("Sneak32");
                }
                
                // Set only essential flags
                minimalData.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
                
                // Add the main service UUID so web clients can find it
                log_i("Adding main service UUID to advertising data: %s", SNEAK32_SERVICE_UUID_STR);
                BLEUUID serviceUUID(SNEAK32_SERVICE_UUID_STR);
                minimalData.setCompleteServices(serviceUUID);
                
                // Set advertisement data
                pAdvertising->setAdvertisementData(minimalData);
                
                // If we got here without exceptions, break the retry loop
                log_i("Advertising configured successfully");
                break;
            }
            
        } catch (const std::exception& e) {
            log_e("Exception in advertising task: %s", e.what());
        } catch (...) {
            log_e("Unknown exception in advertising task");
        }
        
        // Wait before retry
        vTaskDelay(RETRY_DELAY);
    }
    
    // Signal completion
    xSemaphoreGive(advertisingSemaphore);
    vTaskDelete(NULL);
}

void BLEAdvertisingManager::setup()
{
    log_i("Setting up BLE advertising");
    
    // Check if BLEDevice is initialized
    if (!BLEDeviceWrapper::isInitialized()) {
        log_e("BLEDevice not initialized, cannot setup advertising");
        return;
    }
    
    // If already initialized, don't re-initialize
    if (isInitialized && pAdvertising != nullptr) {
        log_i("BLE advertising already initialized, skipping setup");
        return;
    }
    
    // Register GAP event handler first
    registerGAPEventHandler();
    
    // Create semaphore if it doesn't exist
    if (advertisingSemaphore == nullptr) {
        advertisingSemaphore = xSemaphoreCreateBinary();
        if (advertisingSemaphore == nullptr) {
            log_e("Failed to create advertising semaphore");
            return;
        }
    }
    
    // Create advertising task
    // Increase stack size to 8192 bytes for BLE operations
    BaseType_t result = xTaskCreatePinnedToCore(
        advertisingTask,
        "BLEAdv",
        8192,  // Increased from 4096 to 8192
        NULL,
        1,
        &advertisingTaskHandle,
        0  // Run on Core 0
    );
    
    if (result != pdPASS) {
        log_e("Failed to create advertising task");
        return;
    }
    
    // Wait for advertising task to complete with increased timeout
    if (xSemaphoreTake(advertisingSemaphore, pdMS_TO_TICKS(10000)) != pdTRUE) {
        log_e("Timeout waiting for advertising task");
        if (advertisingTaskHandle != NULL) {
            vTaskDelete(advertisingTaskHandle);
            advertisingTaskHandle = NULL;
        }
        return;
    }
    
    isInitialized = true;
    log_i("BLE advertising setup complete");
}

/**
 * @brief Create a minimal scan response data object
 * 
 * This method creates a minimal scan response data object with just the device name.
 * 
 * @return BLEAdvertisementData object with minimal data
 */
BLEAdvertisementData BLEAdvertisingManager::createMinimalScanResponseData() {
    BLEAdvertisementData scanRespData;
    
    // Return empty scan response data to avoid heap corruption
    log_i("Creating empty scan response data to avoid heap corruption");
    
    return scanRespData;
}

/**
 * @brief Configure the BLE advertising for normal mode
 * 
 * This method configures the BLE advertising parameters for normal operation.
 * This implementation uses the absolute minimum configuration needed for functionality
 * to avoid any potential issues or memory errors.
 */
void BLEAdvertisingManager::configureNormalMode()
{
    log_v(">> configureNormalMode");
    
    try {
        // Obtener el objeto de advertising
        BLEAdvertising* pAdvertising = BLEDeviceWrapper::getAdvertising();
        if (pAdvertising == nullptr) {
            log_e("Failed to get advertising object");
            return;
        }
        
        // Detener cualquier advertising existente
        log_i("Stopping any existing advertising");
        try {
            pAdvertising->stop();
        } catch (...) {
            log_w("Exception during advertising stop, continuing anyway");
        }
        
        // Añadir un delay para estabilizar
        delay(100);
        
        // Verificar que el objeto sigue siendo válido
        if (pAdvertising == nullptr) {
            log_e("Advertising object became null");
            return;
        }
        
        // Configurar los datos de advertising
        BLEAdvertisementData advertisementData;
        
        // Configurar el nombre del dispositivo
        std::string deviceName = BLEDeviceWrapper::getInitializedName();
        if (!deviceName.empty()) {
            advertisementData.setName(deviceName);
        } else {
            advertisementData.setName("Sneak32");
        }
        
        // Añadir el UUID del servicio principal para que los clientes web puedan encontrarlo
        log_i("Adding main service UUID to advertising data: %s", SNEAK32_SERVICE_UUID_STR);
        BLEUUID serviceUUID(SNEAK32_SERVICE_UUID_STR);
        advertisementData.setCompleteServices(serviceUUID);
        
        // Configurar los datos de advertising
        log_i("Setting advertisement data");
        try {
            // Disable scan response to avoid heap corruption
            pAdvertising->setScanResponse(false);
            
            // Set advertisement data only
            pAdvertising->setAdvertisementData(advertisementData);
            
            log_i("BLE advertising configured successfully");
        } catch (const std::exception& e) {
            log_e("Exception during advertising configuration: %s", e.what());
            return;
        } catch (...) {
            log_e("Unknown exception during advertising configuration");
            return;
        }
    } catch (...) {
        log_e("Exception in configureNormalMode");
    }
    
    log_v("<< configureNormalMode");
}

bool BLEAdvertisingManager::startAdvertising()
{
    log_v(">> startAdvertising");
    
    // Verificar que el objeto de advertising existe
    BLEAdvertising* pAdvertising = BLEDeviceWrapper::getAdvertising();
    if (pAdvertising == nullptr) {
        log_e("Failed to get advertising object");
        return false;
    }
    
    // Añadir un delay para estabilizar
    delay(100);
    
    try {
        log_i("Starting BLE advertising");
        
        // Verificar que el objeto sigue siendo válido
        if (pAdvertising != nullptr) {
            // Ensure scan response is disabled to prevent heap corruption
            try {
                pAdvertising->setScanResponse(false);
                delay(50);
            } catch (...) {
                log_e("Exception disabling scan response");
            }
            
            // Set minimal advertising parameters to reduce memory usage
            try {
                // Set minimal interval (slower advertising = less CPU/memory usage)
                pAdvertising->setMinInterval(0x100); // 160ms
                pAdvertising->setMaxInterval(0x200); // 320ms
                
                // Set minimal power to reduce power consumption
                esp_err_t err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_N12);
                if (err != ESP_OK) {
                    log_w("Failed to set BLE TX power: %d", err);
                }
                
                // Reduce advertising data complexity
                BLEAdvertisementData minimalData;
                std::string deviceName = BLEDeviceWrapper::getInitializedName();
                if (!deviceName.empty()) {
                    minimalData.setName(deviceName);
                } else {
                    minimalData.setName("Sneak32");
                }
                
                // Set only essential flags
                minimalData.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
                
                // Set advertisement data
                pAdvertising->setAdvertisementData(minimalData);
                
                delay(50); // Add delay after setting data
            } catch (...) {
                log_e("Exception setting advertising parameters");
            }
            
            // Usar un bloque try-catch para capturar cualquier excepción durante el start
            try {
                // We can't access m_advParams directly as it's private
                // Just use the standard method with additional safety
                pAdvertising->start();
                
                // Add a delay after starting advertising to allow system to stabilize
                delay(200);
                
                // Set a flag to indicate advertising is active
                isAdvertising = true;
                
                log_i("BLE advertising started successfully");
                return true;
            } catch (const std::exception& e) {
                log_e("Exception during advertising start: %s", e.what());
                return false;
            } catch (...) {
                log_e("Unknown exception during advertising start");
                return false;
            }
        } else {
            log_e("Advertising object became null");
            return false;
        }
    } catch (...) {
        log_e("Exception in startAdvertising");
        return false;
    }
    
    log_v("<< startAdvertising");
    return false;
}

/**
 * @brief Disable scan response data
 * 
 * This method disables scan response data as a last resort.
 */
void BLEAdvertisingManager::disableScanResponseData() {
    log_i("Disabling scan response data");
    
    if (pAdvertising == nullptr) {
        log_e("pAdvertising is null in disableScanResponseData");
        return;
    }
    
    try {
        pAdvertising->setScanResponse(false);
        log_i("Scan response disabled successfully");
    } catch (const std::exception& e) {
        log_e("Exception in disableScanResponseData: %s", e.what());
    } catch (...) {
        log_e("Unknown exception in disableScanResponseData");
    }
}

/**
 * @brief Start BLE advertising
 * 
 * This method starts the BLE advertising process.
 */
void BLEAdvertisingManager::start()
{
    log_i("Starting BLE advertising");
    
    // Check if initialized
    if (!isInitialized || pAdvertising == nullptr) {
        log_e("Cannot start advertising: not initialized or pAdvertising is null");
        
        // Try to initialize
        setup();
        
        // Check again after setup attempt
        if (!isInitialized || pAdvertising == nullptr) {
            log_e("Failed to initialize advertising in start method");
            return;
        }
    }
    
    try {
        // Verify BLEDevice is still initialized
        if (!BLEDeviceWrapper::isInitialized()) {
            log_e("BLEDevice not initialized before starting advertising");
            return;
        }
        
        // Double-check advertising object
        if (pAdvertising == nullptr) {
            log_e("pAdvertising is null before starting");
            
            // Try to recover
            delay(100); // Give a bit of time before recovery attempt
            pAdvertising = BLEDeviceWrapper::getAdvertising();
            if (pAdvertising == nullptr) {
                log_e("Failed to recover pAdvertising before starting");
                return;
            }
            log_i("Successfully recovered pAdvertising before starting");
            
            // Re-configure since we have a new advertising object
            configureNormalMode();
        }
        
        // Add delay before starting
        delay(100);
        
        // Start advertising with error handling
        log_i("Starting BLE advertising");
        try {
            // Use standard method to start advertising
            pAdvertising->start();
            log_i("BLE advertising started successfully");
        } catch (const std::exception& e) {
            log_e("Exception when calling start advertising: %s", e.what());
            
            // Try to recover by reconfiguring and waiting longer
            log_i("Attempting to reconfigure advertising before retrying start");
            delay(200); // Longer delay for more stability
            configureNormalMode();
            delay(200); // Wait after configuration
            
            // Try again with even more basic approach
            try {
                // Try standard approach again
                pAdvertising->start();
                log_i("BLE advertising started successfully on second attempt");
            } catch (...) {
                log_e("Failed to start advertising even after reconfiguration");
                
                // Last resort - try with empty data
                try {
                    BLEAdvertisementData minimalData;
                    minimalData.setFlags(ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
                    pAdvertising->setAdvertisementData(minimalData);
                    pAdvertising->setScanResponseData(minimalData);
                    pAdvertising->start();
                    log_i("Started advertising with last-resort minimal data");
                } catch (...) {
                    log_e("All advertising start attempts failed");
                }
            }
        }
    } catch (const std::exception& e) {
        log_e("Exception in BLEAdvertisingManager::start: %s", e.what());
    } catch (...) {
        log_e("Unknown exception in BLEAdvertisingManager::start");
    }
}

/**
 * @brief Stop BLE advertising
 * 
 * This method stops the BLE advertising process.
 */
void BLEAdvertisingManager::stop()
{
    log_i("Stopping BLE advertising");
    
    // Check if initialized
    if (!isInitialized || pAdvertising == nullptr) {
        log_e("Cannot stop advertising: not initialized or pAdvertising is null");
        return;
    }
    
    try {
        // Verify BLEDevice is still initialized
        if (!BLEDeviceWrapper::isInitialized()) {
            log_e("BLEDevice not initialized before stopping advertising");
            return;
        }
        
        // Double-check advertising object
        if (pAdvertising == nullptr) {
            log_e("pAdvertising is null before stopping");
            return;
        }
        
        // Stop advertising with error handling
        log_i("Stopping BLE advertising");
        try {
            pAdvertising->stop();
            log_i("BLE advertising stopped successfully");
        } catch (const std::exception& e) {
            log_e("Exception when calling pAdvertising->stop(): %s", e.what());
            
            // Try to recover by getting a new advertising object
            log_i("Attempting to recover advertising object before retrying stop");
            pAdvertising = BLEDeviceWrapper::getAdvertising();
            
            if (pAdvertising != nullptr) {
                try {
                    pAdvertising->stop();
                    log_i("BLE advertising stopped successfully after recovery");
                } catch (...) {
                    log_e("Failed to stop advertising even after recovery");
                }
            }
        }
    } catch (const std::exception& e) {
        log_e("Exception in BLEAdvertisingManager::stop: %s", e.what());
    } catch (...) {
        log_e("Unknown exception in BLEAdvertisingManager::stop");
    }
}

/**
 * @brief Configure stealth mode for advertising
 * 
 * Sets up directed advertising to a specific authorized device
 */
void BLEAdvertisingManager::configureStealthMode()
{
    log_i("Configuring BLE advertising for stealth mode");
    
    // Safety check
    if (pAdvertising == nullptr) {
        log_e("pAdvertising is null in configureStealthMode");
        return;
    }
    
    try {
        // Set advertising mode to directed advertising (high duty cycle)
        advertisingMode = 1; // ESP_BLE_ADV_TYPE_DIRECT_IND_HIGH
        pAdvertising->setAdvertisementType((esp_ble_adv_type_t)advertisingMode);
        
        // Set scan filter for stealth mode
        pAdvertising->setScanFilter(true, false);
        
        // Set advertisement data with minimal information
        BLEAdvertisementData minimalData;
        minimalData.setFlags(0x06); // Limited discovery mode + BLE only
        pAdvertising->setAdvertisementData(minimalData);
        
        log_i("Stealth mode configured successfully");
    } catch (const std::exception& e) {
        log_e("Exception in configureStealthMode: %s", e.what());
    } catch (...) {
        log_e("Unknown exception in configureStealthMode");
    }
}

/**
 * @brief Get advertisement data
 * 
 * @return BLEAdvertisementData object with device name and service UUID
 */
BLEAdvertisementData BLEAdvertisingManager::getAdvertisementData()
{
    BLEAdvertisementData advertisementData;
    
    try {
        // Set device name
        std::string deviceName;
        try {
            deviceName = BLEDeviceWrapper::getInitializedName();
        } catch (...) {
            log_e("Exception getting initialized name");
        }
        
        if (deviceName.empty()) {
            deviceName = "Sneak32";
            log_i("Using default device name: %s", deviceName.c_str());
        }
        
        // Set name with error handling
        try {
            advertisementData.setName(deviceName);
        } catch (...) {
            log_e("Exception setting device name");
        }
        
        // Set service UUID - using the string UUID
        try {
            BLEUUID serviceUUID(SNEAK32_SERVICE_UUID_STR);
            advertisementData.setCompleteServices(serviceUUID);
        } catch (...) {
            log_e("Exception setting service UUID");
            
            // Try with a simple 16-bit UUID as fallback
            try {
                BLEUUID fallbackUUID((uint16_t)ADV_SCANNER_SERVICE_UUID);
                advertisementData.setCompleteServices(fallbackUUID);
                log_i("Used fallback 16-bit UUID for services");
            } catch (...) {
                log_e("Exception setting fallback service UUID");
            }
        }
        
        // Set flags
        try {
            advertisementData.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
        } catch (...) {
            log_e("Exception setting flags");
        }
        
        // Set manufacturer data - keep this minimal
        try {
            uint8_t manufacturerData[] = {0x01, 0x02};  // Simplified manufacturer data
            advertisementData.setManufacturerData(std::string((char*)manufacturerData, sizeof(manufacturerData)));
        } catch (...) {
            log_e("Exception setting manufacturer data");
        }
    } catch (const std::exception& e) {
        log_e("Exception in getAdvertisementData: %s", e.what());
    } catch (...) {
        log_e("Unknown exception in getAdvertisementData");
    }
    
    return advertisementData;
}
