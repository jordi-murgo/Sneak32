#include "BLEDetect.h"
#include "BLEDeviceList.h"
#include "AppPreferences.h"
#include "BLE.h"
#include "BLEStatusUpdater.h"
#include <mutex>

BLEDetectClass BLEDetector;

BLEDetectClass::BLEDetectClass() : isDetecting(false), pBLEScan(nullptr) {
}

void BLEDetectClass::setup()
{
    Serial.println("Setting up BLE Detector");
    pBLEScan = BLEDevice::getScan();
    start();
    Serial.println("BLE Detector setup complete");
}

void BLEDetectClass::start()
{
    Serial.println("Starting BLE Detection task");
    pBLEScan = BLEDevice::getScan();
    if (pBLEScan == nullptr)
    {
        Serial.println("Error: Failed to initialize pBLEScan");
        return;
    }
    pBLEScan->setAdvertisedDeviceCallbacks(new BLEDetectAdvertisedDeviceCallbacks(this));
    pBLEScan->setActiveScan(!appPrefs.passive_scan);
    if (!isDetecting)
    {
        isDetecting = true;
        xTaskCreatePinnedToCore(
            [](void *parameter)
            { static_cast<BLEDetectClass *>(parameter)->detect_loop(); },
            "BLE_Detect_Task", 4096, this, 1, &detectTaskHandle, 0);
    }
    Serial.println("BLE Detection task started");
}

void BLEDetectClass::stop()
{
    Serial.println("Stopping BLE Detection task");
    isDetecting = false;
    if (pBLEScan != nullptr)
    {
        pBLEScan->stop();
        pBLEScan->setAdvertisedDeviceCallbacks(nullptr);
    }
    if (detectTaskHandle != nullptr)
    {
        vTaskDelete(detectTaskHandle);
        detectTaskHandle = nullptr;
    }
    Serial.println("BLE Detection task stopped");
}

void BLEDetectClass::cleanDetectionData()
{
    std::lock_guard<std::mutex> lock(detectedDevicesMutex);
    detectedDevices.clear();
    lastDetectionTime = 0;
    BLEStatusUpdater.update();
}

size_t BLEDetectClass::getDetectedDevicesCount()
{
    return detectedDevices.size();
}

void BLEDetectClass::BLEDetectAdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice advertisedDevice)
{
    std::lock_guard<std::mutex> lock(parent->detectedDevicesMutex);

    int rssi = advertisedDevice.getRSSI();

    if (rssi >= appPrefs.minimal_rssi)
    {
        esp_bd_addr_t bleaddr;
        memcpy(bleaddr, advertisedDevice.getAddress().getNative(), sizeof(esp_bd_addr_t));
        MacAddress deviceMac(bleaddr);

        if (bleDeviceList.is_device_in_list(deviceMac))
        {
            Serial.printf("Detected BLE device: %s\n", deviceMac.toString().c_str());
            parent->lastDetectionTime = millis() / 1000;

            auto it = std::find(parent->detectedDevices.begin(), parent->detectedDevices.end(), deviceMac);
            if (it == parent->detectedDevices.end())
            {
                parent->detectedDevices.push_back(deviceMac);
                BLEStatusUpdater.update();
            }
        }
    }
}

std::vector<MacAddress> BLEDetectClass::getDetectedDevices()
{
    return detectedDevices;
}

time_t BLEDetectClass::getLastDetectionTime()
{
    return lastDetectionTime;
}

bool BLEDetectClass::isSomethingDetected()
{
    return (!detectedDevices.empty()) &&
           ((millis() / 1000) - lastDetectionTime < 60);
}

/**
 * @brief Main loop for BLE detection
 *
 * This function is the main loop for BLE detection. It starts the BLE scan and updates the detected devices list.
 * It also updates the detected devices characteristic.
 */
void BLEDetectClass::detect_loop()
{
    Serial.println("BLEDetectClass::detect_loop - Started");
    while (isDetecting)
    {
        try
        {
            Serial.printf(">> BLEDetectClass::detect_loop - Starting BLE Detection pause during %d seconds\n", appPrefs.ble_scan_delay);
            delay(appPrefs.ble_scan_delay * 1000);
            
            Serial.printf(">> BLEDetectClass::detect_loop - Starting BLE Detection during %d seconds\n", appPrefs.ble_scan_duration);
            
            if (pBLEScan == nullptr)
            {
                Serial.println("Error: pBLEScan is null. Reinitializing...");
                pBLEScan = BLEDevice::getScan();
                if (pBLEScan == nullptr)
                {
                    Serial.println("Failed to reinitialize pBLEScan. Skipping this iteration.");
                    continue;
                }
            }
            
            BLEScanResults foundDevices = pBLEScan->start(appPrefs.ble_scan_duration, false);
            Serial.printf(">> BLEDetectClass::detect_loop - BLE Detection complete. %d devices found.\n", foundDevices.getCount());
            pBLEScan->clearResults();
            
            {
                std::lock_guard<std::mutex> lock(detectedDevicesMutex);
                if (isSomethingDetected())
                {
                    BLEStatusUpdater.update();
                }
            }
        }
        catch (const std::exception& e)
        {
            Serial.printf("Exception in BLEDetectClass::detect_loop: %s\n", e.what());
        }
        catch (...)
        {
            Serial.println("Unknown exception in BLEDetectClass::detect_loop");
        }
    }
    Serial.println("BLEDetectClass::detect_loop - Ended");
}
