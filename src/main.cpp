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

#ifdef ENABLE_LED
#include <Adafruit_NeoPixel.h>
#endif

#define MAX_STATIONS 350
#define MAX_SSIDS 200
#define LED_PIN 2
#define BLE_PACKET_SIZE 100
#define SSID_MAX_LEN 33
#define BLE_MTU_SIZE 500 // Adjust this based on your BLE MTU size
#define PACKET_DELAY 100 // Delay between packets in milliseconds

// Our own Uart Service UUID
#define UART_SERVICE_UUID "81af4cd7-e091-490a-99ee-caa99032ef4e"
#define UART_DATATRANSFER_UUID 0xFFE2
#define ONLY_MANAGEMENT_FRAMES_UUID 0xFFE0
#define MINIMAL_RSSI_UUID 0xFFE1
#define LOOP_DELAY_UUID 0xFFE3

// Define specific UUIDs for wearable devices
#define WEARABLE_SERVICE_UUID 0x180A // Device Information Service
#define MANUFACTURER_NAME_UUID 0x2A29
#define MODEL_NUMBER_UUID 0x2A24

#define DEVICE_NAME "Redmi Watch 3.14C"
#define DEVICE_APPEARANCE 0x03C0 // SmartWatch
#define DEVICE_MANUFACTURER "Xiaomi"

struct WifiDevice
{
  std::string address;
  int8_t rssi;
  uint8_t channel;
  time_t last_seen;
};

struct WifiNetwork
{
  std::string ssid;
  int8_t rssi;
  uint8_t channel;
  std::string type; // New field for frame type
  time_t last_seen;
};

std::vector<WifiDevice> stationsList;
std::vector<WifiNetwork> ssidList;
BLEServer *pServer = nullptr;
BLEService *pDeviceInfoService = nullptr;
BLECharacteristic *pManufacturerNameCharacteristic = nullptr;
BLECharacteristic *pModelNumberCharacteristic = nullptr;
BLECharacteristic *pTxCharacteristic = nullptr;
BLECharacteristic *pOnlyManagementFramesCharacteristic = nullptr;
BLECharacteristic *pMinimalRSSICharacteristic = nullptr;
int currentChannel = 1;
bool deviceConnected = false;
bool only_management_frames = false;
int minimal_rssi = -50;
uint32_t loop_delay = 2000; // Default 2000ms
char device_name[32];

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Constants for packet markers
const char *PACKET_START_MARKER = "START:";
const char *PACKET_END_MARKER = "END";

static void process_management_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel);
static void process_control_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel);
static void process_data_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel);

#ifdef ENABLE_LED
#ifndef PIN_NEOPIXEL
#define PIN_NEOPIXEL 8 // Adjust this number to the correct pin for your board
#endif
#define NUMPIXELS 1 // Usually there's only one NeoPixel on the board

// Define colors
#define COLOR_OFF pixels.Color(0, 0, 0)
#define COLOR_RED pixels.Color(255, 0, 0)
#define COLOR_GREEN pixels.Color(0, 255, 0)
#define COLOR_BLUE pixels.Color(0, 0, 255)

Adafruit_NeoPixel pixels(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

void setPixelColor(uint32_t color)
{
  pixels.setPixelColor(0, color);
  pixels.show();
}
#else
// Dummy function when LED is disabled
void setPixelColor(uint32_t color) {}
#endif

// Declare the mutex
SemaphoreHandle_t dataAccessMutex = NULL;

// Add these global variables at the beginning of the file
unsigned long lastPrintTime = 0;
const unsigned long printInterval = 30000; // 30 seconds in milliseconds

Preferences preferences;

String generateJsonString()
{
  if (xSemaphoreTake(dataAccessMutex, pdMS_TO_TICKS(50)) == pdTRUE)
  {
    String jsonString = "{";

    // Add timestamp, counts, and free heap
    jsonString += "\"timestamp\":" + String(time(nullptr)) + ",";
    jsonString += "\"stations_count\":" + String(stationsList.size()) + ",";
    jsonString += "\"ssids_count\":" + String(ssidList.size()) + ",";
    jsonString += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    jsonString += "\"only_management_frames\":" + (String)(only_management_frames ? "true" : "false") + ",";
    jsonString += "\"minimal_rssi\":" + String(minimal_rssi) + ",";

    // Add stations array
    jsonString += "\"stations\":[";
    for (size_t i = 0; i < stationsList.size(); ++i)
    {
      if (i > 0)
        jsonString += ",";
      char station_json[256];

      snprintf(station_json, sizeof(station_json),
               "{\"mac\":\"%s\",\"rssi\":%d,\"channel\":%d,\"last_seen\":%d}",
               stationsList[i].address.c_str(),
               stationsList[i].rssi,
               stationsList[i].channel,
               stationsList[i].last_seen);

      jsonString += String(station_json);
    }
    jsonString += "],";

    // Add SSIDs array
    jsonString += "\"ssids\":[";
    for (size_t i = 0; i < ssidList.size(); ++i)
    {
      if (i > 0)
        jsonString += ",";
      char ssid_json[256];

      snprintf(ssid_json, sizeof(ssid_json),
               "{\"ssid\":\"%s\",\"rssi\":%d,\"channel\":%d,\"type\":\"%s\",\"last_seen\":%d}",
               ssidList[i].ssid.c_str(),
               ssidList[i].rssi,
               ssidList[i].channel,
               ssidList[i].type.c_str(),
               ssidList[i].last_seen);

      jsonString += ssid_json;
    }
    jsonString += "]";

    jsonString += "}";

    xSemaphoreGive(dataAccessMutex);
    return jsonString;
  }
  else
  {
    Serial.println("Could not obtain mutex to generate JSON");
    return "{}";
  }
}

/* Parse SSID from the packet with validation */
static void parse_ssid(const uint8_t *payload, int payload_len, uint8_t subtype, char ssid[SSID_MAX_LEN])
{
  int pos = 24;   // Start after the management frame header
  ssid[0] = '\0'; // Default empty SSID

  // Ensure there are enough bytes for the header and at least one IE
  if (payload_len < pos + 2)
  {
    return;
  }

  // Determine if it's a Beacon frame or a Probe Request
  uint16_t frame_control = payload[0] | (payload[1] << 8);
  uint8_t frame_type = (frame_control & 0x000C) >> 2;
  uint8_t frame_subtype = (frame_control & 0x00F0) >> 4;

  // For Probe Requests, SSID is immediately after the frame header
  // For Beacon frames, there are additional fields before the SSID
  if (frame_type == 0 && frame_subtype == 4)
  { // Probe Request
    pos = 24;
  }
  else if (frame_type == 0 && frame_subtype == 8)
  {           // Beacon frame
    pos = 36; // Skip additional Beacon frame fields
  }
  else
  {
    return;
  }

  while (pos < payload_len - 2)
  {
    uint8_t id = payload[pos];
    uint8_t len = payload[pos + 1];

    if (id == 0)
    { // SSID
      if (len == 0)
      {
        // Probe Request with hidden SSID, do not display, others do
        if (subtype != 4)
        {
        }
        return; // Hidden SSID, exit function
      }
      else if (len > SSID_MAX_LEN - 1)
      {
        len = SSID_MAX_LEN - 1;
      }

      if (len > 0 && pos + 2 + len <= payload_len)
      {
        memcpy(ssid, &payload[pos + 2], len);
        ssid[len] = '\0'; // Null-terminate

        // Validate that SSID contains only printable characters
        for (int i = 0; i < len; i++)
        {
          if (!isprint((unsigned char)ssid[i]))
          {
            ssid[i] = '.'; // Replace non-printable characters
          }
        }
        return; // SSID found, exit function
      }
      else
      {
        return;
      }
    }

    pos += 2 + len; // Move to the next IE
  }
}

// Enhanced Wi-Fi promiscuous mode callback function
static void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
  if (type != WIFI_PKT_MGMT && type != WIFI_PKT_CTRL && type != WIFI_PKT_DATA)
    return;

  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
  const uint8_t *payload = pkt->payload;
  int payload_len = pkt->rx_ctrl.sig_len;
  int8_t rssi = pkt->rx_ctrl.rssi;

  uint16_t frame_control = payload[0] | (payload[1] << 8);
  uint8_t frame_type = (frame_control & 0x000C) >> 2;
  uint8_t frame_subtype = (frame_control & 0x00F0) >> 4;

  uint8_t channel = pkt->rx_ctrl.channel;

  if (rssi >= minimal_rssi)
  {
    // Process the packet based on its type
    switch (frame_type)
    {
    case 0: // Management frame
      process_management_frame(payload, payload_len, frame_subtype, rssi, channel);
      break;
    case 1: // Control frame
      if (!only_management_frames)
      {
        process_control_frame(payload, payload_len, frame_subtype, rssi, channel);
      }
      break;
    case 2: // Data frame
      if (!only_management_frames)
      {
        process_data_frame(payload, payload_len, frame_subtype, rssi, channel);
      }
      break;
    default:
      break;
    }
  }
}

// Update or add SSID to the list
void updateOrAddSSID(const char *ssid, int8_t rssi, uint8_t channel, const std::string &frameType)
{
  if (xSemaphoreTake(dataAccessMutex, pdMS_TO_TICKS(50)) == pdTRUE)
  {
    time_t now = time(nullptr);
    auto it = std::find_if(ssidList.begin(), ssidList.end(),
                           [&ssid](const WifiNetwork &network)
                           { return network.ssid == ssid; });

    if (it != ssidList.end())
    {
      // Update existing SSID
      if (rssi > it->rssi)
      {
        it->rssi = rssi;
        it->channel = channel;
        it->type = frameType;
      }
      it->last_seen = now;
    }
    else if (ssidList.size() < MAX_SSIDS)
    {
      // Add new SSID
      ssidList.push_back({std::string(ssid), rssi, channel, frameType, now});
      Serial.printf("New SSID detected: %s (RSSI: %d, Channel: %d, Type: %s)\n",
                    ssid, rssi, channel, frameType.c_str());
    }
    else
    {
      // Replace the oldest SSID
      auto oldest = std::min_element(ssidList.begin(), ssidList.end(),
                                     [](const WifiNetwork &a, const WifiNetwork &b)
                                     { return a.last_seen < b.last_seen; });
      *oldest = {std::string(ssid), rssi, channel, frameType, now};
      Serial.printf("Replaced oldest SSID with: %s (RSSI: %d, Channel: %d, Type: %s)\n",
                    ssid, rssi, channel, frameType.c_str());
    }
    xSemaphoreGive(dataAccessMutex);
  }
  else
  {
    Serial.println("Could not obtain mutex to update SSID data");
  }
}

// Modificar updateOrAddStation para aceptar un puntero uint8_t en lugar de una cadena
void updateOrAddStation(const uint8_t *mac, int8_t rssi, uint8_t channel)
{
  if (xSemaphoreTake(dataAccessMutex, pdMS_TO_TICKS(50)) == pdTRUE)
  {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    time_t now = time(nullptr);
    auto it = std::find_if(stationsList.begin(), stationsList.end(),
                           [&macStr](const WifiDevice &device)
                           { return device.address == macStr; });

    if (it != stationsList.end())
    {
      // Update existing station
      if (rssi > it->rssi)
      {
        it->rssi = rssi;
        it->channel = channel;
      }
      it->last_seen = now;
    }
    else if (stationsList.size() < MAX_STATIONS)
    {
      // Add new station
      stationsList.push_back({std::string(macStr), rssi, channel, now});
      Serial.printf("New station detected: %s (RSSI: %d, Channel: %d)\n", macStr, rssi, channel);
    }
    else
    {
      // Replace the oldest station
      auto oldest = std::min_element(stationsList.begin(), stationsList.end(),
                                     [](const WifiDevice &a, const WifiDevice &b)
                                     { return a.last_seen < b.last_seen; });
      *oldest = {std::string(macStr), rssi, channel, now};
      Serial.printf("Replaced oldest station with: %s (RSSI: %d, Channel: %d)\n", macStr, rssi, channel);
    }
    xSemaphoreGive(dataAccessMutex);
  }
  else
  {
    Serial.println("Could not obtain mutex to update station data");
  }
}

// Actualizar process_management_frame
static void process_management_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel)
{
  const uint8_t *src_addr;
  char ssid[SSID_MAX_LEN] = {0};
  std::string frameType;

  switch (subtype)
  {
  case 0: // Association Request
  case 2: // Reassociation Request
    src_addr = &payload[10];
    parse_ssid(payload, payload_len, subtype, ssid);
    frameType = "assoc";
    break;
  case 4: // Probe Request
    src_addr = &payload[10];
    parse_ssid(payload, payload_len, subtype, ssid);
    frameType = "probe";
    break;
  case 8: // Beacon
    src_addr = &payload[10];
    parse_ssid(payload, payload_len, subtype, ssid);
    frameType = "beacon";
    break;
  case 10: // Disassociation
  case 12: // Deauthentication
    src_addr = &payload[10];
    break;
  default:
    return; // Ignore other subtypes
  }

  if (ssid[0] != 0)
  {
    updateOrAddSSID(ssid, rssi, channel, frameType);
  }

  if (src_addr)
  {
    updateOrAddStation(src_addr, rssi, channel);
  }
}

// Actualizar process_control_frame
static void process_control_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel)
{
  const uint8_t *src_addr;

  switch (subtype)
  {
  case 8:  // Block Ack Request
  case 9:  // Block Ack
  case 10: // PS-Poll
  case 11: // RTS
  case 13: // Acknowledgement
    src_addr = &payload[10];
    break;
  case 14: // CF-End
  case 15: // CF-End + CF-Ack
    src_addr = &payload[4];
    break;
  default:
    return; // Ignore other subtypes
  }

  updateOrAddStation(src_addr, rssi, channel);
}

// Actualizar process_data_frame
static void process_data_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel)
{
  const uint8_t *src_addr = &payload[10];

  updateOrAddStation(src_addr, rssi, channel);
}

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
#ifdef ENABLE_LED
    setPixelColor(COLOR_BLUE); // Blue when a device connects
#endif
  };

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
#ifdef ENABLE_LED
    setPixelColor(COLOR_OFF); // Back to off when disconnected
#endif
    // Restart advertising
    pServer->getAdvertising()->start();
  }
};

void sendJsonOverBLE(const String &jsonData)
{
  if (deviceConnected)
  {
    Serial.println("Starting BLE data transmission...");
#ifdef ENABLE_LED
    setPixelColor(COLOR_BLUE); // Blue during transmission
#endif

    // Send JSON in chunks
    size_t totalLength = jsonData.length();
    size_t sentLength = 0;
    uint16_t packetNumber = 0;
    uint16_t numPackets = (totalLength / (BLE_MTU_SIZE - 4)) + 1;

    // Send start marker
    char packetsHeader[5];
    snprintf(packetsHeader, sizeof(packetsHeader), "%04X", numPackets);
    String startMarker = PACKET_START_MARKER + String(packetsHeader);
    pTxCharacteristic->setValue(startMarker.c_str());
    pTxCharacteristic->notify();
    delay(PACKET_DELAY);

    while (sentLength < totalLength)
    {
      size_t chunkSize = min((size_t)BLE_MTU_SIZE - 4, totalLength - sentLength); // 4 bytes for packet number
      String chunk = jsonData.substring(sentLength, sentLength + chunkSize);

      // Prepend packet number to chunk
      char packetHeader[5];
      snprintf(packetHeader, sizeof(packetHeader), "%04X", packetNumber);
      String packetData = String(packetHeader) + chunk;

      pTxCharacteristic->setValue(packetData.c_str());
      pTxCharacteristic->notify();

      sentLength += chunkSize;
      packetNumber++;

      delay(PACKET_DELAY); // Add delay between packets
    }

    // Send end marker
    pTxCharacteristic->setValue(PACKET_END_MARKER);
    pTxCharacteristic->notify();
    delay(PACKET_DELAY);

    Serial.println("BLE data transmission completed");
#ifdef ENABLE_LED
    setPixelColor(COLOR_OFF); // Back to off after sending
#endif
  }
  else
  {
    Serial.println("BLE device not connected. Cannot send data.");
  }
}

BLEService *pNusService = nullptr;
BLECharacteristic *pRxCharacteristic = nullptr;

// Add these new classes after the MyServerCallbacks class

void loadPreferences()
{
  preferences.begin("wifi_monitor", false);
  only_management_frames = preferences.getBool("only_mgmt", false);
  minimal_rssi = preferences.getInt("min_rssi", -50);
  loop_delay = preferences.getUInt("loop_delay", 2000);
  preferences.end();
  Serial.printf("loadPreferences > Only Management Frames set to: %s\n", only_management_frames ? "true" : "false");
  Serial.printf("loadPreferences > Minimal RSSI set to: %d\n", minimal_rssi);
  Serial.printf("loadPreferences > Loop delay set to: %u ms\n", loop_delay);
}

void savePreferences()
{
  preferences.begin("wifi_monitor", false);
  preferences.putBool("only_mgmt", only_management_frames);
  preferences.putInt("min_rssi", minimal_rssi);
  preferences.putUInt("loop_delay", loop_delay);
  preferences.end();
}

class SendDataOverBLECallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();
    Serial.printf("Received '%s' over BLE\n", value.c_str());
    // Generate JSON and send via BLE
    String jsonData = generateJsonString();
    Serial.println("Generated JSON:");
    Serial.println(jsonData);
    sendJsonOverBLE(jsonData);
  }
};

class OnlyManagementFramesCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0)
    {
      bool new_value = false;
      if (value == "true" || value == "1")
      {
        new_value = true;
      }

      // Use a critical section to update the shared variable
      portENTER_CRITICAL(&mux);
      only_management_frames = new_value;
      portEXIT_CRITICAL(&mux);

      Serial.printf("Only Management Frames set to: %s\n", only_management_frames ? "true" : "false");
      savePreferences();
    }
  }

  void onRead(BLECharacteristic *pCharacteristic)
  {
    // Use a critical section to read the shared variable
    portENTER_CRITICAL(&mux);
    bool current_value = only_management_frames;
    portEXIT_CRITICAL(&mux);

    pCharacteristic->setValue(current_value ? "true" : "false");
  }
};

class MinimalRSSICallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0)
    {
      minimal_rssi = std::stoi(value);
      Serial.printf("Minimal RSSI set to: %d\n", minimal_rssi);
      savePreferences();
    }
  }

  void onRead(BLECharacteristic *pCharacteristic)
  {
    std::string value = std::to_string(minimal_rssi);
    pCharacteristic->setValue(value);
  }
};

class LoopDelayCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0)
    {
      loop_delay = std::stoul(value);
      Serial.printf("Loop delay set to: %u ms\n", loop_delay);
      savePreferences();
    }
  }

  void onRead(BLECharacteristic *pCharacteristic)
  {
    std::string value = std::to_string(loop_delay);
    pCharacteristic->setValue(value);
  }
};

void setupBLE()
{
  // Initialize BLE
  BLEDevice::init(device_name);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the Device Information Service
  pDeviceInfoService = pServer->createService(BLEUUID((uint16_t)0x180A));

  // Create characteristics for the Device Information Service
  pManufacturerNameCharacteristic = pDeviceInfoService->createCharacteristic(
      BLEUUID((uint16_t)0x2A29),
      BLECharacteristic::PROPERTY_READ);
  pManufacturerNameCharacteristic->setValue(DEVICE_MANUFACTURER);

  pModelNumberCharacteristic = pDeviceInfoService->createCharacteristic(
      BLEUUID((uint16_t)0x2A24),
      BLECharacteristic::PROPERTY_READ);
  pModelNumberCharacteristic->setValue(device_name);

  // Create the UART Service
  pNusService = pServer->createService(UART_SERVICE_UUID);

  // Create characteristics for UART
  pTxCharacteristic = pNusService->createCharacteristic(
      BLEUUID((uint16_t)UART_DATATRANSFER_UUID),
      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_INDICATE);
  pTxCharacteristic->setCallbacks(new SendDataOverBLECallbacks());
  pTxCharacteristic->addDescriptor(new BLE2902());

  pOnlyManagementFramesCharacteristic = pNusService->createCharacteristic(
      BLEUUID((uint16_t)ONLY_MANAGEMENT_FRAMES_UUID),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pOnlyManagementFramesCharacteristic->setCallbacks(new OnlyManagementFramesCallbacks());
  pOnlyManagementFramesCharacteristic->addDescriptor(new BLE2902());

  pMinimalRSSICharacteristic = pNusService->createCharacteristic(
      BLEUUID((uint16_t)MINIMAL_RSSI_UUID),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pMinimalRSSICharacteristic->setCallbacks(new MinimalRSSICallbacks());
  pMinimalRSSICharacteristic->addDescriptor(new BLE2902());

  // Add this new characteristic
  BLECharacteristic *pLoopDelayCharacteristic = pNusService->createCharacteristic(
      BLEUUID((uint16_t)LOOP_DELAY_UUID),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pLoopDelayCharacteristic->setCallbacks(new LoopDelayCallbacks());
  pLoopDelayCharacteristic->addDescriptor(new BLE2902());

  // Initialize the characteristics with the loaded values
  std::string only_mgmt_str = std::to_string(only_management_frames);
  pOnlyManagementFramesCharacteristic->setValue(only_mgmt_str);
  std::string rssi_str = std::to_string(minimal_rssi);
  pMinimalRSSICharacteristic->setValue(rssi_str);
  std::string loop_delay_str = std::to_string(loop_delay);
  pLoopDelayCharacteristic->setValue(loop_delay_str);

  // Start the services
  pDeviceInfoService->start();
  pNusService->start();

  // Advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(UART_SERVICE_UUID);
  pAdvertising->addServiceUUID(BLEUUID((uint16_t)0x180A));
  pAdvertising->setAppearance(DEVICE_APPEARANCE);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.println("BLE Advertising started with UART Service");
}

void setup()
{
  delay(1000); // Wait 1 second before starting

  Serial.begin(115200);
  Serial.println("Starting setup...");

  // Get my BT MAC Address
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_BT);
  snprintf(device_name, sizeof(device_name), "%s (%02X%02X)", DEVICE_NAME, mac[4], mac[5]);
  Serial.println("Device name: " + String(device_name));

  Serial.println("Loading preferences");
  loadPreferences();

#ifdef ENABLE_LED
  pinMode(LED_PIN, OUTPUT);
  // Initialize NeoPixel
  pixels.begin();
  setPixelColor(COLOR_GREEN);
#endif

  // Create the mutex
  Serial.println("Creating the mutex");
  dataAccessMutex = xSemaphoreCreateMutex();
  if (dataAccessMutex == NULL)
  {
    Serial.println("Error creating the mutex");
    // Consider restarting the device or taking another appropriate action
    ESP.restart();
  }
  Serial.println("Mutex created");

  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous_rx_cb(promiscuous_rx_cb);
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
  // Set promiscuous mode with specific filter for management frames
  wifi_promiscuous_filter_t filter = { .filter_mask = only_management_frames ? WIFI_PROMIS_FILTER_MASK_MGMT : WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_CTRL};
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous(true);

  Serial.println("WiFi configuration completed");

  setupBLE();

#ifdef ENABLE_LED
  setPixelColor(COLOR_OFF);
#endif
}

void loop()
{
  currentChannel = (currentChannel % 14) + 1;
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

  // Use a critical section when accessing shared variables
  portENTER_CRITICAL(&mux);
  bool current_only_management_frames = only_management_frames;
  int current_minimal_rssi = minimal_rssi;
  uint32_t current_loop_delay = loop_delay;
  portEXIT_CRITICAL(&mux);

  Serial.printf("WiFi channel: %d, Stations: %zu, SSIDs: %zu, Free Heap: %d, Minimal RSSI: %d, Only Management Frames: %s, Loop Delay: %u ms\n",
                currentChannel, stationsList.size(), ssidList.size(), ESP.getFreeHeap(),
                current_minimal_rssi, current_only_management_frames ? "true" : "false", current_loop_delay);

  // Print SSID list every 30 seconds
  unsigned long currentTime = millis();
  if (currentTime - lastPrintTime >= printInterval)
  {
    String ssidListString;
    if (xSemaphoreTake(dataAccessMutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
      ssidListString = "\n--- SSID List ---\n";
      ssidListString += "SSID                             | RSSI | Channel | Type   | Last seen\n";
      ssidListString += "---------------------------------|------|---------|--------|------------------\n";
      for (const auto &network : ssidList)
      {
        char line[150];
        snprintf(line, sizeof(line), "%-32s | %4d | %7d | %-6s | %d\n",
                 network.ssid.c_str(), network.rssi, network.channel, network.type.c_str(), network.last_seen);
        ssidListString += line;
      }
      ssidListString += "---------------------------------|------|---------|--------|------------------\n";
      char totalLine[50];
      snprintf(totalLine, sizeof(totalLine), "Total SSIDs: %zu\n", ssidList.size());
      ssidListString += totalLine;
      ssidListString += "----------------------\n";
      xSemaphoreGive(dataAccessMutex);
    }
    else
    {
      ssidListString = "Could not obtain mutex to print SSID list\n";
    }
    Serial.println(ssidListString);
    lastPrintTime = currentTime;
  }

  delay(current_loop_delay);
}