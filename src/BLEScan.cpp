#include "BLEScan.h"
#include "BLEDeviceList.h"
#include "MacAddress.h"
#include "AppPreferences.h"

extern BLEDeviceList bleDeviceList;
extern AppPreferencesData appPrefs;

BLEScanClass BLEScanner;

BLEScanAdvertisedDeviceCallbacks::BLEScanAdvertisedDeviceCallbacks(BLEScanClass *scan) : scan(scan) {}

void BLEScanClass::setup()
{
    isr_log_i("Setting up BLE Scanner");
    pBLEScan = BLEDevice::getScan();
    callbacks = new BLEScanAdvertisedDeviceCallbacks(this);
    start();
    isr_log_i("BLE Scanner setup complete");
}

void BLEScanClass::start()
{
    isr_log_i("Starting BLE Scan task");
    pBLEScan->setAdvertisedDeviceCallbacks(callbacks);
    pBLEScan->setActiveScan(true);
    if (!isScanning)
    {
        isScanning = true;
        xTaskCreatePinnedToCore(
            [](void *parameter)
            { static_cast<BLEScanClass *>(parameter)->scan_loop(); },
            "BLE_Scan_Task", 4096, this, 1, &scanTaskHandle, 0);
    }
    isr_log_i("BLE Scan task started");
}

void BLEScanClass::stop()
{
    isr_log_i("Stopping BLE Scan task");
    pBLEScan->stop();
    pBLEScan->setAdvertisedDeviceCallbacks(nullptr);
    if (isScanning)
    {
        isScanning = false;
        vTaskDelete(scanTaskHandle);
        scanTaskHandle = nullptr;
        pBLEScan->stop();
    }
    isr_log_i("BLE Scan task stopped");
}

void BLEScanClass::scan_loop()
{
    isr_log_i("ScanLoop started");
    while (true)
    {
        delay(appPrefs.ble_scan_delay * 1000);
        isr_log_d("Starting BLE Scan");
        pBLEScan->setInterval(100);
        pBLEScan->setWindow(90);
        pBLEScan->setActiveScan(!appPrefs.passive_scan);
        BLEScanResults foundDevices = pBLEScan->start(appPrefs.ble_scan_duration, false);
        isr_log_i("BLE Scan complete. %d devices found", foundDevices.getCount());
    }
}

void printHexDump(const uint8_t *data, size_t length)
{
    char ascii[17];
    char formatted_ascii[17];
    char line[100];  // Buffer para construir cada línea

    for (size_t i = 0; i < length; i++)
    {
        // Print offset at the start of each line
        if (i % 16 == 0)
        {
            if (i != 0)
            {
                isr_log_v("  |%s|", formatted_ascii);
            }
            snprintf(line, sizeof(line), "%08x  ", i);
            isr_log_v("%s", line);
            // Initialize ascii buffer for new line
            memset(ascii, 0, sizeof(ascii));
            memset(formatted_ascii, ' ', sizeof(formatted_ascii) - 1);
            formatted_ascii[16] = '\0';
        }

        // Print hex value
        snprintf(line, sizeof(line), "%02x ", data[i]);
        isr_log_v("%s", line);

        // Store ASCII representation
        ascii[i % 16] = (data[i] >= 32 && data[i] <= 126) ? data[i] : '.';
        formatted_ascii[i % 16] = ascii[i % 16];

        // Print extra space between 8th and 9th bytes
        if ((i + 1) % 8 == 0 && (i + 1) % 16 != 0)
        {
            isr_log_v(" ");
        }
    }

    // Print padding spaces if the last line is incomplete
    if (length % 16 != 0)
    {
        size_t padding = 16 - (length % 16);
        for (size_t i = 0; i < padding; i++)
        {
            isr_log_v("   ");
            if ((length + i + 1) % 8 == 0 && (length + i + 1) % 16 != 0)
            {
                isr_log_v(" ");
            }
        }
        isr_log_v("  |%s|", formatted_ascii);
    }
    else if (length > 0)
    {
        isr_log_v("  |%s|", formatted_ascii);
    }
}

void BLEScanAdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice advertisedDevice)
{
    try
    {
        int rssi = advertisedDevice.getRSSI();

        if (rssi >= appPrefs.minimal_rssi)
        {
            // Move string operations outside of the critical section
            std::string addressStr = advertisedDevice.getAddress().toString();
            std::string nameStr = advertisedDevice.haveName() ? advertisedDevice.getName() : "";
            std::string appearanceStr = std::to_string(advertisedDevice.getAppearance());
            std::string uuidStr = advertisedDevice.haveServiceUUID() ? advertisedDevice.getServiceUUID().toString() : "";

            esp_bd_addr_t bleaddr;
            memcpy(bleaddr, advertisedDevice.getAddress().getNative(), sizeof(esp_bd_addr_t));

            boolean isPublic = false;
            String addressType;
            switch (advertisedDevice.getAddressType())
            {
            case BLE_ADDR_TYPE_PUBLIC:
                addressType = "public";
                isPublic = true;
                break;
            case BLE_ADDR_TYPE_RANDOM:
                addressType = "random";
                break;
            case BLE_ADDR_TYPE_RPA_PUBLIC:
                addressType = "RPA-public";
                isPublic = true;
                break;
            case BLE_ADDR_TYPE_RPA_RANDOM:
                addressType = "RPA-random";
                break;
            default:
                addressType = "unknown";
                break;
            }

            // Base output always includes address and type
            isr_log_d("Address: %s (%s)",
                  addressStr.c_str(), addressType.c_str());

            // Optional fields only if they have value
            if (!nameStr.empty())
            {
                isr_log_v(", Name: '%s'", nameStr.c_str());
            }

            uint16_t appearance = advertisedDevice.getAppearance();
            if (appearance != 0)
            {
                isr_log_v(", Appearance: %d", appearance);
            }

            if (advertisedDevice.haveServiceUUID())
            {
                isr_log_v(", Service UUID: %s", uuidStr.c_str());
            }

            // Print payload in hexdump format
            if (advertisedDevice.getPayloadLength() > 0)
            {
                isr_log_v("Payload hexdump:");
                printHexDump(advertisedDevice.getPayload(), advertisedDevice.getPayloadLength());
            }

            if (addressStr == "00:00:00:00:00:00")
            {
                // This is an invalid address, ignore it
                return;
            }

            if (!isPublic && appPrefs.ignore_random_ble_addresses)
            {
                // This is a random BLE address, ignore it
                return;
            }

            // Use a lock_guard for exception-safe locking
            {
                std::lock_guard<std::mutex> lock(scan->mtx);

                if (!appPrefs.ignore_random_ble_addresses || isPublic)
                {
                    bleDeviceList.updateOrAddDevice(MacAddress(bleaddr), rssi, String(nameStr.c_str()), isPublic);
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        isr_log_e("Exception in onResult: %s", e.what());
    }
    catch (...)
    {
        isr_log_e("Unknown exception in onResult");
    }
}
