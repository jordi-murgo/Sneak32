/**
 * @file BLEDeviceWrapper.h
 * @brief Wrapper for BLEDevice with additional functionality
 */

#ifndef BLE_DEVICE_WRAPPER_H
#define BLE_DEVICE_WRAPPER_H

#include <BLEDevice.h>
#include "Logging.h"
#include "esp_rom_sys.h"

/**
 * @brief Wrapper class for BLEDevice with additional functionality
 *
 * This class extends the standard BLEDevice with additional methods
 * for tracking initialization state and other features.
 */
class BLEDeviceWrapper
{
public:
    /**
     * @brief Initialize the BLE device
     *
     * @param deviceName Name of the device
     */
    static bool init(const std::string &deviceName)
    {
        static portMUX_TYPE initMux = portMUX_INITIALIZER_UNLOCKED;

        try
        {
            // Quick check with minimal critical section
            {
                portENTER_CRITICAL(&initMux);
                if (initialized)
                {
                    portEXIT_CRITICAL(&initMux);
                    log_w("BLE already initialized with name: %s", initializedName.c_str());
                    return true;
                }
                portEXIT_CRITICAL(&initMux);
            }

            log_i("[BLE] Starting initialization with name: %s", deviceName.c_str());

            // Feed watchdog and give system time
            delay(50);  // Increased from 10ms to 50ms

            // Initialize BLE with empty name first to reduce complexity
            log_d("[BLE] Calling BLEDevice::init with empty name");
            BLEDevice::init("");
            delay(100);  // Increased from 10ms to 100ms

            // Verify initialization
            if (!BLEDevice::getInitialized())
            {
                log_e("BLE initialization failed");
                return false;
            }

            // Set name after successful init - reinitialize with the actual name
            log_d("[BLE] Setting device name: %s", deviceName.c_str());
            BLEDevice::deinit(false);
            delay(50);
            BLEDevice::init(deviceName);
            delay(100);  // Increased from 10ms to 100ms

            // Update state with minimal critical section
            portENTER_CRITICAL(&initMux);
            initialized = true;
            initializedName = deviceName;
            portEXIT_CRITICAL(&initMux);

            log_i("BLE initialized successfully");
            return true;
        }
        catch (const std::exception &e)
        {
            log_e("Exception during BLE initialization: %s", e.what());
            return false;
        }
        catch (...)
        {
            log_e("Unknown exception during BLE initialization");
            return false;
        }
    }

    /**
     * @brief Deinitialize the BLE device
     *
     * @param release Whether to release memory
     */
    static void deinit(bool release = false)
    {
        static portMUX_TYPE deinitMux = portMUX_INITIALIZER_UNLOCKED;
        portENTER_CRITICAL(&deinitMux);

        try
        {
            BLEDevice::deinit(release);
            initialized = false;
            initializedName = "";
        }
        catch (const std::exception &e)
        {
            log_e("Exception during BLE deinitialization: %s", e.what());
        }
        catch (...)
        {
            log_e("Unknown exception during BLE deinitialization");
        }

        portEXIT_CRITICAL(&deinitMux);
    }

    /**
     * @brief Check if the BLE device is initialized
     *
     * @return true if initialized, false otherwise
     */
    static bool isInitialized()
    {
        return initialized;
    }

    /**
     * @brief Get the initialized name
     *
     * @return std::string name of the initialized device
     */
    static std::string getInitializedName()
    {
        try
        {
            if (!initialized)
            {
                log_w("BLEDevice not initialized, returning empty name");
                return "";
            }
            return initializedName;
        }
        catch (...)
        {
            log_e("Exception in getInitializedName");
            return "";
        }
    }

    /**
     * @brief Get the BLE advertising object
     *
     * This method gets the BLEAdvertising instance with multiple safety checks,
     * error handling, and a retry mechanism for improved reliability.
     *
     * @return BLEAdvertising* The BLEAdvertising instance or nullptr if not available
     */
    static BLEAdvertising *getAdvertising()
    {
        log_i("Getting BLE advertising object with enhanced safety");

        // Verificar si BLE está inicializado
        if (!isInitialized())
        {
            log_e("Cannot get advertising object: BLEDevice is not initialized");
            return nullptr;
        }

        BLEAdvertising *pAdvertising = nullptr;

        // Make multiple attempts to get the advertising object
        const int maxAttempts = 3;
        for (int attempt = 1; attempt <= maxAttempts; attempt++)
        {
            try
            {
                // Simple check - just get the advertising object
                log_i("Using BLEDevice::getAdvertising() on attempt %d", attempt);
                
                // Verificar que BLEDevice sigue inicializado
                if (!BLEDevice::getInitialized()) {
                    log_e("BLEDevice no longer initialized");
                    
                    // Try to reinitialize with a simple approach
                    if (attempt < maxAttempts) {
                        log_i("Attempting to reinitialize BLE");
                        try {
                            // Use a simple initialization with minimal operations
                            BLEDevice::init("Sneak32");
                            delay(200);
                        } catch (...) {
                            log_e("Failed to reinitialize BLE");
                        }
                    }
                    
                    delay(100);
                    continue;
                }
                
                // Obtener el objeto de advertising con protección
                try {
                    // Use a more direct approach to get the advertising object
                    // This avoids potential issues with the standard getAdvertising method
                    if (BLEDevice::getInitialized()) {
                        pAdvertising = BLEDevice::getAdvertising();
                        
                        // Verify the advertising object is valid
                        if (pAdvertising != nullptr) {
                            log_i("Successfully retrieved advertising object on attempt %d", attempt);
                            return pAdvertising;
                        } else {
                            log_e("Advertising object is null after retrieval");
                        }
                    } else {
                        log_e("BLEDevice not initialized during advertising object retrieval");
                    }
                } catch (const std::exception &e) {
                    log_e("Exception during BLEDevice::getAdvertising() call: %s", e.what());
                } catch (...) {
                    log_e("Unknown exception during BLEDevice::getAdvertising() call");
                }
                
                // If we got here, something went wrong
                if (attempt < maxAttempts) {
                    // Add a delay before the next attempt
                    delay(200 * attempt);
                    
                    // Try to clean up and reinitialize
                    try {
                        log_i("Attempting cleanup before next advertising retrieval attempt");
                        // Force a memory cleanup
                        ESP.getMinFreeHeap();
                        delay(100);
                    } catch (...) {
                        log_e("Exception during cleanup");
                    }
                }
            }
            catch (const std::exception &e)
            {
                log_e("Exception getting advertising object (attempt %d): %s", attempt, e.what());
            }
            catch (...)
            {
                log_e("Unknown exception getting advertising object (attempt %d)", attempt);
            }

            // Introduce delay between attempts
            if (attempt < maxAttempts)
            {
                log_i("Delaying before attempt %d", attempt + 1);
                delay(300 * attempt); // Increase delay with each attempt
            }
        }

        log_e("Failed to get advertising object after %d attempts", maxAttempts);
        return nullptr;
    }

    /**
     * @brief Get the BLE scan object
     *
     * @return Pointer to BLEScan object
     */
    static BLEScan *getScan()
    {
        if (!initialized)
        {
            log_e("BLEDevice not initialized, cannot get scan");
            return nullptr;
        }
        return BLEDevice::getScan();
    }

    /**
     * @brief Create a BLE server
     *
     * @return Pointer to BLEServer object
     */
    static BLEServer *createServer()
    {
        if (!initialized)
        {
            log_e("BLEDevice not initialized, cannot create server");
            return nullptr;
        }
        return BLEDevice::createServer();
    }

    /**
     * @brief Add a device to the whitelist
     *
     * @param address BLE address to add
     */
    static void whiteListAdd(BLEAddress address)
    {
        if (!initialized)
        {
            log_e("BLEDevice not initialized, cannot add to whitelist");
            return;
        }
        BLEDevice::whiteListAdd(address);
    }

private:
    static bool initialized;
    static std::string initializedName;
};

// Initialize static members - moved to a separate .cpp file to avoid multiple definition errors
// bool BLEDeviceWrapper::initialized = false;
// std::string BLEDeviceWrapper::initializedName = "";

#endif // BLE_DEVICE_WRAPPER_H
