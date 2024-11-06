#include <Arduino.h>
#include <mutex>

#include "WifiDetect.h"
#include "WifiDeviceList.h"
#include "WifiNetworkList.h"
#include "MACAddress.h"
#include "AppPreferences.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "BLE.h"
#include "BLEStatusUpdater.h"

extern WifiDeviceList stationsList;
extern WifiNetworkList ssidList;

WifiDetectClass WifiDetector;

#define SSID_MAX_LEN 33

WifiDetectClass* WifiDetectClass::instance = nullptr;

WifiDetectClass::WifiDetectClass() {
    instance = this;
}

void WifiDetectClass::setup()
{
    WiFi.mode(WIFI_AP_STA);
    WiFi.setTxPower(WIFI_POWER_8_5dBm); // Super Mini se calienta con más potencia.
    start();
    Serial.println("WifiDetectClass: Setup completed");
}

void WifiDetectClass::start()
{
    Serial.println("WifiDetectClass: Starting");
    setFilter(appPrefs.only_management_frames);
    registerWifiEventHandlers();
    esp_wifi_set_promiscuous(true);
    Serial.println("WifiDetectClass: Started");
}

void WifiDetectClass::stop()
{
    Serial.println("WifiDetectClass: Stopping");
    WiFi.softAPdisconnect(true);
    deregisterWifiEventHandlers();
    esp_wifi_set_promiscuous(false);
    Serial.println("WifiDetectClass: Stopped");
}

void WifiDetectClass::setFilter(bool onlyManagementFrames)
{
    wifi_promiscuous_filter_t filter;
    if (onlyManagementFrames)
    {
        filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
    }
    else
    {
        filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_CTRL};
    }
    esp_wifi_set_promiscuous_filter(&filter);
}

void WifiDetectClass::setChannel(int channel)
{
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

void WifiDetectClass::setupAP(const char *ssid, const char *password, int channel = 1)
{
    WiFi.softAP(ssid, password, channel, 0, 4, false);
}

void WifiDetectClass::deregisterWifiEventHandlers()
{
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(NULL));
}

void WifiDetectClass::registerWifiEventHandlers()
{
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(WifiDetectClass::promiscuous_rx_cb));
}

std::vector<MacAddress> WifiDetectClass::getDetectedDevices()
{
    return detectedDevices;
}

std::vector<String> WifiDetectClass::getDetectedNetworks()
{
    return detectedNetworks;
}

void WifiDetectClass::cleanDetectionData()
{
    std::lock_guard<std::mutex> lock(detectionMutex);
    detectedDevices.clear();
    detectedNetworks.clear();
    lastDetectionTime = 0;
    BLEStatusUpdater.update();
}

void WifiDetectClass::addDetectedNetwork(const String &ssid)
{
    std::lock_guard<std::mutex> lock(detectionMutex);
    static size_t last_detected_networks_size = 0;

    // Verificar si la red ya está en la lista detectedNetworks
    auto it = std::find(detectedNetworks.begin(), detectedNetworks.end(), ssid);
    if (it == detectedNetworks.end()) {
        // La red no está en la lista, así que la añadimos
        detectedNetworks.push_back(ssid);
        lastDetectionTime = millis() / 1000;
        if (detectedNetworks.size() != last_detected_networks_size) { 
            last_detected_networks_size = detectedNetworks.size();
            BLEStatusUpdater.update();
        }
    }
}

void WifiDetectClass::addDetectedDevice(const MacAddress &device)
{
    std::lock_guard<std::mutex> lock(detectionMutex);
    static size_t last_detected_devices_size = 0;

    // Verificar si el dispositivo ya está en la lista detectedDevices
    auto it = std::find(detectedDevices.begin(), detectedDevices.end(), device);
    if (it == detectedDevices.end()) {
        // El dispositivo no está en la lista, así que lo añadimos
        detectedDevices.push_back(device);
        lastDetectionTime = millis() / 1000;
        if (detectedDevices.size() != last_detected_devices_size) { 
            last_detected_devices_size = detectedDevices.size();
            BLEStatusUpdater.update();
        }
    }
}

void WifiDetectClass::process_management_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel)
{
    const uint8_t *src_addr = nullptr;
    char ssid[SSID_MAX_LEN] = {0};
    String frameType;

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

    case 8: // Ignore Beacons
        if (false)
        {
            src_addr = &payload[10];
            parse_ssid(payload, payload_len, subtype, ssid);
            frameType = "beacon";
        }
        break;

    case 10: // Disassociation
    case 12: // Deauthentication
        frameType = "deauth";
        src_addr = &payload[10];
        break;
    default:
        return; // Ignore other subtypes
    }

    if (ssid[0] != 0)
    {
        if (ssidList.is_ssid_in_list(String(ssid)))
        {
            Serial.printf("SSID detected (%s): %s\n", frameType.c_str(), String(ssid).c_str());
            addDetectedNetwork(String(ssid));
        }
    }

    if (src_addr)
    {
        if (stationsList.is_device_in_list(MacAddress(src_addr)))
        {
            Serial.printf("Device detected (%s): %s\n", frameType.c_str(), MacAddress(src_addr).toString().c_str());
            addDetectedDevice(MacAddress(src_addr));
        }
    }
}

void WifiDetectClass::process_control_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel)
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

    if (stationsList.is_device_in_list(MacAddress(src_addr)))
    {
        Serial.printf("Device detected (%02x): %s\n", subtype, MacAddress(src_addr).toString().c_str());
        addDetectedDevice(MacAddress(src_addr));
    }
}

void WifiDetectClass::process_data_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel)
{
    const uint8_t *src_addr = &payload[10];

    if (stationsList.is_device_in_list(MacAddress(src_addr)))
    {
        Serial.printf("Device detected (data): %s\n", MacAddress(src_addr).toString().c_str());
        addDetectedDevice(MacAddress(src_addr));
    }
}

/**
 * @brief Parses the SSID from a WiFi packet.
 *
 * This function extracts the SSID from a WiFi management frame (Beacon or Probe Request).
 * It validates the SSID to ensure it contains only printable characters.
 *
 * @param payload Pointer to the packet payload.
 * @param payload_len Length of the payload.
 * @param subtype Subtype of the management frame.
 * @param ssid Buffer to store the extracted SSID.
 */
void WifiDetectClass::parse_ssid(const uint8_t *payload, int payload_len, uint8_t subtype, char ssid[SSID_MAX_LEN])
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
    {             // Beacon frame
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

time_t WifiDetectClass::getLastDetectionTime()
{
    return lastDetectionTime;
}

bool WifiDetectClass::isSomethingDetected()
{
    return (detectedDevices.size() > 0 || detectedNetworks.size() > 0) && (millis() / 1000) - lastDetectionTime < 60;
}

size_t WifiDetectClass::getDetectedDevicesCount()
{
    return detectedDevices.size();
}

size_t WifiDetectClass::getDetectedNetworksCount()
{
    return detectedNetworks.size();
}

/**
 * @brief Callback function for WiFi promiscuous mode.
 *
 * This function is called for each received WiFi packet. It processes the packet
 * based on its type (management, control, or data) and updates the corresponding
 * data structures.
 *
 * @param buf Pointer to the received packet buffer.
 * @param type Type of the WiFi packet.
 */
void WifiDetectClass::promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
  if (instance) {
    instance->handle_rx(buf, type);
  }
}

void WifiDetectClass::handle_rx(void *buf, wifi_promiscuous_pkt_type_t type)
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

    if (rssi >= appPrefs.minimal_rssi)
    {
        switch (frame_type)
        {
        case 0: // Management frame
            process_management_frame(payload, payload_len, frame_subtype, rssi, channel);
            break;
        case 1: // Control frame
            if (!appPrefs.only_management_frames)
            {
                process_control_frame(payload, payload_len, frame_subtype, rssi, channel);
            }
            break;
        case 2: // Data frame
            if (!appPrefs.only_management_frames)
            {
                process_data_frame(payload, payload_len, frame_subtype, rssi, channel);
            }
            break;
        default:
            break;
        }
    }
}
