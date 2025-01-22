/**
 * @file main.cpp
 * @author Jordi Murgo (jordi.murgo@gmail.com)
 * @brief Main file for the WiFi monitoring and analysis project
 * @version 0.1
 * @date 2024-07-28
 *
 * @copyright Copyright (c) 2024 Jordi Murgo
 *
 * @license MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <WiFi.h>
#include <esp_wifi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <nvs_flash.h>
#include <esp_wifi_types.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>
#include <algorithm>
#include <time.h>
#include <string>
#include <Preferences.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include "LedManager.h"

#include "BLE.h"
#include "Preferences.h"
#include "WifiScan.h"

#define MAX_STATIONS 255
#define MAX_SSIDS 200
#define MAX_BLE_DEVICES 100

#include "BLEDeviceList.h"
#include "WifiDeviceList.h"
#include "WifiNetworkList.h"
#include "WifiScan.h"
#include "WifiDetect.h"
#include "BLEDetect.h"
#include "BLEScan.h"
#include "AppPreferences.h"
#include "FlashStorage.h"
#include "BLEAdvertisingManager.h"
#include "FirmwareInfo.h"
#include "BLEStatusUpdater.h"

// Define the boot button pin (adjust if necessary)
#define BOOT_BUTTON_PIN 0

/**
 * @brief BLEDeviceList is a list of BLE devices.
 *
 * This class is used to store and manage a list of BLE devices.
 * It uses a mutex to ensure that the list is accessed in a thread-safe manner.
 */
BLEDeviceList bleDeviceList(MAX_BLE_DEVICES);

/**
 * @brief WifiDeviceList is a list of WiFi devices.
 *
 * This class is used to store and manage a list of WiFi devices.
 * It uses a mutex to ensure that the list is accessed in a thread-safe manner.
 */
WifiDeviceList stationsList(MAX_STATIONS);

/**
 * @brief WifiNetworkList is a list of WiFi networks.
 *
 * This class is used to store and manage a list of WiFi networks.
 * It uses a mutex to ensure that the list is accessed in a thread-safe manner.
 */
WifiNetworkList ssidList(MAX_SSIDS);

time_t my_universal_time = 0;

extern bool deviceConnected;

void printSSIDAndBLELists();

#ifdef PIN_NEOPIXEL
LedManager ledManager(PIN_NEOPIXEL, 1);
#else
LedManager ledManager(LED_BUILTIN);
#endif

// Add these global variables at the beginning of the file
unsigned long lastPrintTime = 0;
const unsigned long printInterval = 30000; // 30 seconds in milliseconds

// Base time for the last_seen field in the lists
time_t base_time = 0;

// Function to update base_time from a list
template <typename T>
void updateBaseTime(const std::vector<T, DynamicPsramAllocator<T>> &list)
{
  for (const auto &item : list)
  {
    base_time = std::max(base_time, item.last_seen);
  }
}

void printMemoryStats() {
    log_d("\nMemory Statistics:");
    log_d("Total PSRAM: %d bytes", ESP.getPsramSize());
    log_d("Free PSRAM: %d bytes", ESP.getFreePsram());
    log_d("Total heap: %d bytes", ESP.getHeapSize());
    log_d("Free heap: %d bytes", ESP.getFreeHeap());
    log_w("Minimum free heap: %d bytes", ESP.getMinFreeHeap());
    log_d("");
}


/**
 * @brief Prints the list of detected SSIDs and BLE devices.
 *
 * This function formats and prints a table of detected WiFi networks and BLE devices
 * to the serial console, including details such as RSSI, channel, and last seen time.
 */
void printSSIDAndBLELists()
{
  String listString;

  // SSID List
  listString = "\n---------------------------------------------------------------------------------\n";
  listString += "SSID                             | RSSI | Channel | Type   | Times | Last seen\n";
  listString += "---------------------------------|------|---------|--------|-------|------------\n";
  for (const auto &network : ssidList.getClonedList())
  {
    char line[150];
    snprintf(line, sizeof(line), "%-32s | %4d | %7d | %-6s | %5d | %d\n",
             network.ssid.c_str(), network.rssi, network.channel, network.type.c_str(),
             network.times_seen, network.last_seen);
    listString += line;
  }
  listString += "---------------------------------|------|---------|--------|-------|------------\n";
  listString += "Total SSIDs: " + String(ssidList.size()) + "\n";
  listString += "--------------------------------------------------------------------------------\n";

  // BLE Device List
  listString += "\n----------------------------------------------------------------------\n";
  listString += "Name                             | Public | RSSI | Times | Last seen\n";
  listString += "---------------------------------|--------|------|-------|------------\n";

  auto devices = bleDeviceList.getClonedList();
  for (const auto &device : devices)
  {
    char isPublicStr[6];
    snprintf(isPublicStr, sizeof(isPublicStr), "%s", device.isPublic ? "true" : "false");
    char line[150];
    if (device.name.length())
    {
      snprintf(line, sizeof(line), "%-32s | %-6s | %4d | %5d | %d\n",
               device.name.substring(0, 32).c_str(), isPublicStr, device.rssi,
               device.times_seen, device.last_seen);
    }
    else
    {
      // Unnamed (F8:77:B8:1F:B9:7C)
      snprintf(line, sizeof(line), "Unnamed (%17s)      | %-6s | %4d | %5d | %d\n",
               device.address.toString().c_str(), isPublicStr, device.rssi,
               device.times_seen, device.last_seen);
    }
    listString += line;
  }

  listString += "---------------------------------|--------|------|-------|------------\n";
  listString += "Total BLE devices: " + String(bleDeviceList.size()) + "\n";
  listString += "----------------------------------------------------------------------\n";

  log_i("\n%s", listString.c_str());
}

void firmwareInfo()
{
  log_d("\n\n----------------------------------------------------------------------");
  log_d("%s", getFirmwareInfoString().c_str());
  log_d("----------------------------------------------------------------------\n");
}

void checkAndRestartAdvertising()
{
  static unsigned long lastAdvertisingRestart = 0;
  const unsigned long advertisingRestartInterval = 60 * 60 * 1000; // 1 hora en milisegundos

  if (millis() - lastAdvertisingRestart > advertisingRestartInterval)
  {
    log_i("Restarting BLE advertising");
    BLEDevice::stopAdvertising();
    delay(100);
    BLEDevice::startAdvertising();
    lastAdvertisingRestart = millis();
  }
}

/**
 * @brief Setup function that runs once when the device starts.
 *
 * This function initializes serial communication, loads preferences,
 * sets up WiFi in promiscuous mode, and initializes BLE.
 */
void setup()
{
  log_i("Starting...");

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    log_w("Erasing NVS flash...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Initialize PSRAM
  if (!psramFound())
  {
    log_e("PSRAM not found!");
    while (1)
    {
        delay(100);
    }
  }
  log_i("PSRAM size: %d bytes", ESP.getPsramSize());

  // Load preferences
  loadAppPreferences();

  // Initialize BLE
  BLEDevice::init(appPrefs.device_name);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, (esp_power_level_t)appPrefs.bleTxPower);

  // Set WiFi power
  esp_wifi_set_max_tx_power((wifi_power_t)appPrefs.wifiTxPower);

  // Setup BLE services and characteristics
  setupBLE();

  // Start operation mode
  switch (appPrefs.operation_mode)
  {
  case OPERATION_MODE_SCAN:
    log_i("Starting in SCAN mode");
    WifiScanner.setup();
    BLEScanner.setup();
    break;
  case OPERATION_MODE_DETECTION:
    log_i("Starting in DETECTION mode");
    WifiDetector.setup();
    BLEDetector.setup();
    break;
  default:
    log_i("Starting in OFF mode");
    break;
  }

  log_i("Setup complete");

  // Initialize PSRAM with proper configuration
  if(!psramInit()) {
    log_e("PSRAM initialization failed!");
    ESP_ERROR_CHECK(esp_spiram_init());
    ESP_ERROR_CHECK(esp_spiram_add_to_heapalloc());
  }
  
  size_t psram_size = ESP.getPsramSize();
  size_t free_psram = ESP.getFreePsram();
  
  log_d("PSRAM Size: %d bytes", psram_size);
  log_d("Free PSRAM: %d bytes", free_psram);

  // Encuentra la partición NVS
  const esp_partition_t *nvs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);

  if (nvs_partition != NULL)
  {
    log_d("NVS partition found. Size: %d bytes, Offset: %d", 
          nvs_partition->size, nvs_partition->address);
  } else {
    log_e("NVS partition not found!");
  }

  ledManager.begin();
  ledManager.setPixelColor(0, LedManager::COLOR_GREEN);
  ledManager.show();

  delay(1000); // Espera 1 segundo antes de comenzar

  firmwareInfo();

  // Set CPU frequency
  if (appPrefs.cpu_speed != getCpuFrequencyMhz())
  {
    log_i("Setting CPU frequency to %u MHz", appPrefs.cpu_speed);
    setCpuFrequencyMhz(appPrefs.cpu_speed);
  }

  try
  {
    FlashStorage::loadAll();
  }
  catch (const std::exception &e)
  {
    log_e("Error loading from flash storage: %s", e.what());

    // Handle the error (e.g., clear the lists, use default values, etc.)
    stationsList.clear();
    ssidList.clear();
    bleDeviceList.clear();
  }

  // Update base_time from all lists in one go
  updateBaseTime(ssidList.getClonedList());
  updateBaseTime(bleDeviceList.getClonedList());
  updateBaseTime(stationsList.getClonedList());
  base_time++;
  log_i("Base time set to: %ld", base_time);

  // Setup BLE Core (Advertising and Service Characteristics)
  setupBLE();

  if (appPrefs.operation_mode == OPERATION_MODE_DETECTION)
  {
    WifiDetector.setup();
    BLEDetector.setup();
  }
  else if (appPrefs.operation_mode == OPERATION_MODE_SCAN)
  {
    WifiScanner.setup();
    BLEScanner.setup();
  }

  // Add a delay after setting up detectors
  delay(1000);

  ledManager.setPixelColor(0, LedManager::COLOR_OFF);
  ledManager.show();

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
}

/**
 * @brief Scan mode loop.
 *
 * This function handles the scan mode loop, which is used to scan for WiFi devices.
 * It sets the WiFi channel, performs a BLE scan, and checks for transmission timeout.
 */
void scan_mode_loop()
{
  static unsigned long lastSaved = 0;
  static int currentChannel = 0;

  currentChannel = (currentChannel % 14) + 1;
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

  // Mostrar heap lliure específic a SRAM (INTERNAL)
  size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  // Mostrar heap lliure a PSRAM (SPIRAM)
  size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

  log_d(">> Time: %lu, WiFi Ch: %2d, SSIDs: %zu, Stations: %zu, BLE: %zu, Heap: %d, Heap PSRAM: %d",
        millis() / 1000, currentChannel, ssidList.size(), stationsList.size(), 
        bleDeviceList.size(), freeInternal, freePsram);

  delay(appPrefs.wifi_channel_dwell_time);

  checkTransmissionTimeout();

  checkAndRestartAdvertising();

  if (currentChannel == 14)
  {
    printSSIDAndBLELists();
  }

  // Save all data to flash storage every autosave_interval minutes
  if (millis() - lastSaved >= appPrefs.autosave_interval * 60 * 1000)
  {
    log_i("Saving all data to flash storage");
    try
    {
      FlashStorage::saveAll();
      lastSaved = millis();
      log_i("Data saved successfully");
    }
    catch (const std::exception &e)
    {
      log_e("Error saving to flash storage: %s", e.what());
    }
  }

  bool bootButtonPressed = digitalRead(BOOT_BUTTON_PIN) == LOW;

  if (!deviceConnected)
  {
    if (appPrefs.stealth_mode)
    {
      if (bootButtonPressed)
      {
        log_i(">>> Boot button pressed, disabling stealth mode");
        BLEAdvertisingManager::configureNormalMode();
      }
      else
      {
        BLEAdvertisingManager::configureStealthMode();
      }
    }
    else
    {
      BLEAdvertisingManager::configureNormalMode();
    }
  }
}

/**
 * @brief Monitor mode loop.
 *
 * This function handles the monitor mode loop, which is used to monitor WiFi devices.
 * It sets the WiFi channel 1, performs a BLE scan, and checks for transmission timeout.
 */

void detection_mode_loop()
{
  static auto clonedList = ssidList.getClonedList();
  static size_t currentSSIDIndex = 0;

  if (appPrefs.passive_scan)
  {
    WifiDetector.setChannel(1);
    log_i(">> Passive WiFi scan");
  }
  else
  {
    // Add bounds checking for currentSSIDIndex
    if (currentSSIDIndex >= clonedList.size())
    {
      currentSSIDIndex = 0;
    }

    const auto &currentNetwork = clonedList[currentSSIDIndex];
    // Configure ESP32 to broadcast the selected SSID
    // WifiDetector.setupAP(currentNetwork.ssid.c_str(), nullptr, 1);
    if (&currentNetwork && currentNetwork.ssid && currentNetwork.ssid.length() > 0)
    {
      WifiDetector.setupAP(currentNetwork.ssid.c_str(), nullptr, 1);
      log_i(">> Detection Mode (%02d/%02d) >> Alarm: %d, Broadcasting SSID: \"%s\", Last detection: %d",
            currentSSIDIndex + 1, clonedList.size(), WifiDetector.isSomethingDetected(), 
            currentNetwork.ssid.c_str(),
            millis() / 1000 - WifiDetector.getLastDetectionTime());
    }

    currentSSIDIndex++;
  }

  checkTransmissionTimeout();
  checkAndRestartAdvertising();

  delay(appPrefs.wifi_channel_dwell_time);
}

/**
 * @brief Main loop function that runs repeatedly.
 *
 * This function handles WiFi channel hopping, periodic BLE scanning,
 * and manages the transmission of collected data over BLE.
 */
void loop()
{
    static unsigned long lastMemoryPrint = 0;
    const unsigned long MEMORY_PRINT_INTERVAL = 30000; // 30 segundos

    if (millis() - lastMemoryPrint >= MEMORY_PRINT_INTERVAL) {
        printMemoryStats();
        lastMemoryPrint = millis();
    }

    if (appPrefs.operation_mode == OPERATION_MODE_SCAN)
    {
        scan_mode_loop();
    }
    else if (appPrefs.operation_mode == OPERATION_MODE_DETECTION)
    {
        detection_mode_loop();
    }
    else
    {
        // ON / OFF Red LED
        log_i("Operation mode == OFF");
        ledManager.setPixelColor(0, LedManager::COLOR_RED);
        ledManager.show();
        delay(appPrefs.wifi_channel_dwell_time);
        ledManager.setPixelColor(0, LedManager::COLOR_OFF);
        ledManager.show();
        delay(appPrefs.wifi_channel_dwell_time);
    }
    BLEStatusUpdater.update();
}



