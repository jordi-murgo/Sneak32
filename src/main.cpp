/**
 * @file main.cpp
 * @author Jordi Murgo (savage@apostols.org)
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
#include <esp_wifi_types.h>
#include "esp_log.h"
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

#define MAX_STATIONS 100
#define MAX_SSIDS 100
#define MAX_BLE_DEVICES 100
#define LED_PIN 2
#define BLE_PACKET_SIZE 100
#define SSID_MAX_LEN 33


#include "BLEDeviceList.h"
#include "WifiDeviceList.h"
#include "WifiNetworkList.h"
#include "AppPreferences.h"
#include "FlashStorage.h"


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

extern BLEServer *pServer;
extern BLECharacteristic *pTxCharacteristic;
extern BLECharacteristic *pListSizesCharacteristic;
extern bool deviceConnected;



// Function declarations
static void process_management_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel);
static void process_control_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel);
static void process_data_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel);
String generateJsonString();
static void parse_ssid(const uint8_t *payload, int payload_len, uint8_t subtype, char ssid[SSID_MAX_LEN]);
void updateOrAddBTDevice(const String &address, int rssi, const String &name);
void setupBLE();
void printSSIDAndBLELists();

#ifndef PIN_NEOPIXEL
#define PIN_NEOPIXEL 8 // Ajusta este número al pin correcto para tu placa
#endif
#define NUMPIXELS 1 // Normalmente hay solo un NeoPixel en la placa

LedManager ledManager(PIN_NEOPIXEL, NUMPIXELS);

// Add these global variables at the beginning of the file
unsigned long lastPrintTime = 0;
const unsigned long printInterval = 30000; // 30 seconds in milliseconds

// Base time for the last_seen field in the lists
time_t base_time = 0;

// Function to update base_time from a list
template<typename T>
void updateBaseTime(const std::vector<T>& list) {
    for (const auto& item : list) {
        base_time = std::max(base_time, item.last_seen);
    }
}

String getChipInfo()
{
  char buffer[50];
  snprintf(buffer, sizeof(buffer), "%s (Cores: %d, Rev: %d)",
           ESP.getChipModel(), ESP.getChipCores(), ESP.getChipRevision());
  return String(buffer);
}

/**
 * @brief Callback class for handling list sizes characteristic.
 *
 * This class handles the callback for the list sizes characteristic, updating the list sizes.
 */
class ListSizesCallbacks : public BLECharacteristicCallbacks
{
  void onRead(BLECharacteristic *pCharacteristic)
  {
    updateListSizesCharacteristic();
  }
};

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
      if (device.name.length()) {
        snprintf(line, sizeof(line), "%-32s | %-6s | %4d | %5d | %d\n",
                 device.name.substring(0, 32).c_str(), isPublicStr, device.rssi, 
                 device.times_seen, device.last_seen);
      } else {
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

  Serial.println(listString);
}


void firmwareInfo()
{
  Serial.println("\n\n----------------------------------------------------------------------");
  Serial.print(getFirmwareInfo());
  Serial.println("----------------------------------------------------------------------\n");
}

void checkAndRestartAdvertising() {
    static unsigned long lastAdvertisingRestart = 0;
    const unsigned long advertisingRestartInterval = 60 * 60 * 1000; // 1 hora en milisegundos

    if (millis() - lastAdvertisingRestart > advertisingRestartInterval) {
        Serial.println("Restarting BLE advertising");
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
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  ledManager.begin();
  ledManager.setPixelColor(0, LedManager::COLOR_GREEN);
  ledManager.show();

  delay(1000); // Espera 1 segundo antes de comenzar

  firmwareInfo();

  Serial.println("Loading preferences");
  loadAppPreferences();

  try {
    FlashStorage::loadAll();
  } catch (const std::exception& e) {
    Serial.printf("Error loading from flash storage: %s\n", e.what());

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
  Serial.printf("Base time set to: %ld\n", base_time);

  setupBLE();
  setupWiFi();

  ledManager.setPixelColor(0, LedManager::COLOR_OFF);
  ledManager.show();
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

  Serial.printf(">> Time: %lu, WiFi Ch: %2d, St: %zu, SS: %zu, BLE: %zu, Heap: %d, minRSSI: %d, MgmtOnly: %d, WiFiScan: %u, BLEScan: %u, InTx: %d\n",
                millis() / 1000, currentChannel, stationsList.size(), ssidList.size(), bleDeviceList.size(), ESP.getFreeHeap(),
                appPrefs.minimal_rssi, appPrefs.only_management_frames, appPrefs.loop_delay, appPrefs.ble_scan_period, preparedJsonData.length() > 0);

  delay(appPrefs.loop_delay);

  doBLEScan();
  
  checkTransmissionTimeout();

  checkAndRestartAdvertising();

  if (currentChannel == 14)
  {
    printSSIDAndBLELists();

    // Save all data to flash storage
    Serial.println("Saving all data to flash storage");
    try {
      Serial.println("All data saved to flash storage successfully");
    } catch (const std::exception& e) {
      Serial.printf("Error saving to flash storage: %s\n", e.what());
    }
  }

  // Save all data to flash storage every hour
  if (millis()/1000 - lastSaved > 60 * 60) {
    Serial.println("Saving all data to flash storage");
    FlashStorage::saveAll();
    lastSaved = millis()/1000;
  }

  // Actualiza la característica de tamaños de listas
  updateListSizesCharacteristic();
}

/**
 * @brief Monitor mode loop.
 *
 * This function handles the monitor mode loop, which is used to monitor WiFi devices.
 * It sets the WiFi channel 1, performs a BLE scan, and checks for transmission timeout.
 */
void monitor_mode_loop()
{
  Serial.println("Monitor mode loop");
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  Serial.printf(">> Monitor Mode >> Time: %lu, St: %zu, SS: %zu, BLE: %zu, minRSSI: %d, MgmtOnly: %d, WiFiScan: %u, BLEScan: %u, InTx: %d\n",
                millis() / 1000, stationsList.size(), ssidList.size(), bleDeviceList.size(), ESP.getFreeHeap(),
                appPrefs.minimal_rssi, appPrefs.only_management_frames, appPrefs.loop_delay, appPrefs.ble_scan_period, preparedJsonData.length() > 0);

  doBLEScan();

  checkTransmissionTimeout();
  checkAndRestartAdvertising();

  delay(appPrefs.loop_delay);
}

/**
 * @brief Main loop function that runs repeatedly.
 *
 * This function handles WiFi channel hopping, periodic BLE scanning,
 * and manages the transmission of collected data over BLE.
 */
void loop()
{
  if (appPrefs.operation_mode == OPERATION_MODE_SCAN) {
    scan_mode_loop();
  } else if (appPrefs.operation_mode == OPERATION_MODE_DETECTION) {
    monitor_mode_loop();
  } else {
    Serial.println("Operation mode == OFF");
    ledManager.setPixelColor(0, LedManager::COLOR_RED);
    ledManager.show();
    delay(appPrefs.loop_delay);
    ledManager.setPixelColor(0, LedManager::COLOR_OFF);
    ledManager.show();
    delay(appPrefs.loop_delay);
  }

}
