#include "WifiScan.h"
#include "WifiDeviceList.h"
#include "WifiNetworkList.h"
#include "MACAddress.h"
#include "AppPreferences.h"
#include <Arduino.h>

extern WifiDeviceList stationsList;
extern WifiNetworkList ssidList;

WifiScanClass WifiScanner;

#define SSID_MAX_LEN 33

WifiScanClass *WifiScanClass::instance = nullptr;

WifiScanClass::WifiScanClass()
{
  instance = this;
}

void WifiScanClass::setup()
{
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // Super Mini se calienta con más potencia.
  start();
  Serial.println("WifiScanClass: Setup completed");
}

void WifiScanClass::start()
{
  setFilter(appPrefs.only_management_frames);
  Serial.println("WifiScanClass: Starting");
  esp_wifi_set_promiscuous_rx_cb(&WifiScanClass::promiscuous_rx_cb);
  esp_wifi_set_promiscuous(true);
  Serial.println("WifiScanClass: Started");
}

void WifiScanClass::stop()
{
  Serial.println("WifiScanClass: Stopping");
  esp_wifi_set_promiscuous_rx_cb(NULL);
  esp_wifi_set_promiscuous(false);
  Serial.println("WifiScanClass: Stopped");
}

void WifiScanClass::setChannel(int channel)
{
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

void WifiScanClass::setFilter(bool onlyManagementFrames)
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

void WifiScanClass::process_management_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel)
{
  const uint8_t *src_addr = nullptr;
  char ssid[SSID_MAX_LEN] = {0};
  String frameType;
  bool suspicious = false;

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

    // Verificar si el SSID contiene caracteres sospechosos
    for (int i = 0; ssid[i] != '\0'; i++)
    {
      if (!isalnum(ssid[i]) && !isspace(ssid[i]) && ssid[i] != '-' && ssid[i] != '_')
      {
        suspicious = true;
        break;
      }
    }

    if (suspicious)
    {
      Serial.printf("\nSuspicious Probe Request SSID found: '%s'\n", ssid);
      Serial.println("Frame hexdump:");
      printHexDump(payload, payload_len);
      // Wait until user press button
      while (!digitalRead(0))
      {
        delay(100);
      }
    }
    break;

  case 8: // Beacon
    // src_addr = &payload[10];
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
    ssidList.updateOrAddNetwork(String(ssid), rssi, channel, frameType);
  }

  if (src_addr)
  {
    stationsList.updateOrAddDevice(MacAddress(src_addr), rssi, channel);
  }
}

void WifiScanClass::process_control_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel)
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

  stationsList.updateOrAddDevice(MacAddress(src_addr), rssi, channel);
}

void WifiScanClass::process_data_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel)
{
  const uint8_t *src_addr = &payload[10];

  stationsList.updateOrAddDevice(MacAddress(src_addr), rssi, channel);
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
void WifiScanClass::parse_ssid(const uint8_t *payload, int payload_len, uint8_t subtype, char ssid[SSID_MAX_LEN])
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
void WifiScanClass::promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
  if (instance)
  {
    instance->handle_rx(buf, type);
  }
}

void WifiScanClass::handle_rx(void *buf, wifi_promiscuous_pkt_type_t type)
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

  // Verificar la longitud mínima del paquete
  if (payload_len < 28)
  { // 24 bytes de cabecera + 4 bytes de FCS
    Serial.printf("Packet too short %d\n", payload_len);
    return;
  }

  // Verificar el FCS (Frame Check Sequence) del hardware
  if (pkt->rx_ctrl.rx_state != 0)
  {
    Serial.println("FCS check failed");
    return;
  }

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
