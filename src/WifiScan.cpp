#include "WifiScan.h"
#include "WifiDeviceList.h"
#include "WifiNetworkList.h"
#include "MACAddress.h"
#include "AppPreferences.h"
#include <Arduino.h>

extern WifiDeviceList stationsList;
extern WifiNetworkList ssidList;

WifiScanClass WifiScanner;

uint8_t broadcast_addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t null_addr[6] = {0, 0, 0, 0, 0, 0};

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
  const uint8_t *dst_addr = &payload[4];
  const uint8_t *src_addr = &payload[10];
  const uint8_t *bssid = &payload[16];

  char ssid[SSID_MAX_LEN] = {0};
  String frameType;
  bool suspicious = false;

  switch (subtype)
  {
  case 0: // Association Request
  case 2: // Reassociation Request
    parse_ssid(payload, payload_len, subtype, ssid);
    frameType = "assoc";
    break;

  case 4: // Probe Request
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
  case 5: // Probe Response
    parse_ssid(payload, payload_len, subtype, ssid);
    frameType = "probe-resp";
    break;
  case 8: // Beacon
    parse_ssid(payload, payload_len, subtype, ssid);
    frameType = "beacon";
    break;
  case 1:  // Association Response
  case 3:  // Reassociation Response
  case 10: // Disassociation
  case 6:  // Timing Advertisement
  case 11: // Authentication
  case 12: // Deauthentication
  case 14: // Action
  default:
    frameType = "other";
    break;
  }

  if (memcmp(bssid, null_addr, 6) == 0)
  {
    Serial.println("Null BSSID detected");
    printHexDump(payload, payload_len);
  }

  if (frameType == "probe" || frameType == "assoc" || frameType == "probe-resp")
  {
    Serial.printf(">> Src: %s, Dst: %s, BSSID: %s, RSSI: %d, Channel: %d, FrameType: %s (%d) %s\n",
                  MacAddress(src_addr).toString().c_str(),
                  MacAddress(dst_addr).toString().c_str(),
                  MacAddress(bssid).toString().c_str(),
                  rssi,
                  channel,
                  frameType.c_str(),
                  subtype,
                  ssid[0] ? ssid : "WILDCARD-SSID");
  }
  else if (!ssid[0])
  {
    Serial.printf(">> Src: %s, Dst: %s, BSSID: %s, RSSI: %d, Channel: %d, FrameType: %s (%d)\n",
                  MacAddress(src_addr).toString().c_str(),
                  MacAddress(dst_addr).toString().c_str(),
                  MacAddress(bssid).toString().c_str(),
                  rssi,
                  channel,
                  frameType.c_str(),
                  subtype);
  }

  // Si es una respuesta de probe, usamos el tipo beacon, ya que es lo mismo para nosotros.
  if (frameType == "probe-resp")
  {
    frameType = "beacon";
  }

  // Actualizamos la lista de redes si existen el BSSID o el SSID.
  if (ssid[0] || memcmp(bssid, broadcast_addr, 6) != 0)
  {
    ssidList.updateOrAddNetwork(String(ssid), MacAddress(bssid), rssi, channel, frameType);
  }

  // Actualizamos la lista de estaciones con el origen
  stationsList.updateOrAddDevice(MacAddress(src_addr), MacAddress(bssid), rssi, channel);

  // Si el destino no es broadcast, actualizamos la lista de estaciones con el destino
  if (memcmp(dst_addr, broadcast_addr, 6) != 0)
  {
    stationsList.updateOrAddDevice(MacAddress(dst_addr), MacAddress(bssid), rssi, channel);
  }

  // actualizamos con el BSSID para que sume el tráfico de toda la red al AP 
  if (memcmp(bssid, broadcast_addr, 6) != 0 && memcmp(bssid, null_addr, 6) != 0)
  {
    stationsList.updateOrAddDevice(MacAddress(bssid), MacAddress(bssid), rssi, channel);
  }

}

void WifiScanClass::process_control_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel)
{
  const uint8_t *dst_addr = nullptr;
  const uint8_t *src_addr;
  const uint8_t *bssid;

  switch (subtype)
  {
  case 8:  // Block Ack Request
  case 9:  // Block Ack
  case 10: // PS-Poll
  case 11: // RTS
  case 13: // Acknowledgement
    dst_addr = &payload[4];
    src_addr = &payload[10];
    bssid = &payload[16];
    break;
  case 14: // CF-End
  case 15: // CF-End + CF-Ack
    src_addr = &payload[4];
    bssid = &payload[10];
    break;
  default:
    return; // Ignore other subtypes
  }

  Serial.printf(">> Src: %s, Dst: %s, BSSID: %s, RSSI: %d, Channel: %d, FrameType: Control (%d) \n",
                MacAddress(src_addr).toString().c_str(),
                dst_addr ? MacAddress(dst_addr).toString().c_str() : "EMPTY",
                MacAddress(bssid).toString().c_str(),
                rssi,
                channel,
                subtype);

  // Actualizamos la lista de redes si existe el BSSID.
  if (memcmp(bssid, broadcast_addr, 6) != 0)
  {
    ssidList.updateOrAddNetwork("", MacAddress(bssid), rssi, channel, "other");
  }

  stationsList.updateOrAddDevice(MacAddress(src_addr), MacAddress(bssid), rssi, channel);

  // Si el destino no es broadcast, actualizamos la lista de estaciones con el destino
  if (dst_addr && memcmp(dst_addr, broadcast_addr, 6) != 0)
  {
    stationsList.updateOrAddDevice(MacAddress(dst_addr), MacAddress(bssid), rssi, channel);
  }

  // actualizamos con el BSSID para que sume el tráfico de toda la red al AP 
  if (memcmp(bssid, broadcast_addr, 6) != 0 && memcmp(bssid, null_addr, 6) != 0)
  {
    stationsList.updateOrAddDevice(MacAddress(bssid), MacAddress(bssid), rssi, channel);
  }
}

void WifiScanClass::process_data_frame(const uint8_t *payload, int payload_len, uint8_t subtype, int8_t rssi, uint8_t channel)
{
  // Data frames pueden tener 3 o 4 addresses dependiendo de los flags DS
  const uint8_t *addr1 = &payload[4];  // Destination
  const uint8_t *addr2 = &payload[10]; // Source
  const uint8_t *addr3 = &payload[16]; // BSSID/Source/Destination dependiendo de DS flags

  uint16_t frame_ctrl = payload[0] | (payload[1] << 8);
  bool toDS = frame_ctrl & 0x0100;
  bool fromDS = frame_ctrl & 0x0200;

  const uint8_t *src_addr;
  const uint8_t *dst_addr;
  const uint8_t *bssid;

  if (!toDS && !fromDS)
  { // IBSS (ad-hoc)
    src_addr = addr2;
    dst_addr = addr1;
    bssid = addr3;
  }
  else if (toDS && !fromDS)
  { // To AP
    src_addr = addr2;
    bssid = addr3;
    dst_addr = addr1;
  }
  else if (!toDS && fromDS)
  { // From AP
    dst_addr = addr1;
    bssid = addr2;
    src_addr = addr3;
  }
  else
  { // WDS (bridge)
    dst_addr = addr3;
    src_addr = addr2;
    bssid = addr1;
  }

  Serial.printf(">> Src: %s, Dst: %s, BSSID: %s, RSSI: %d, Channel: %d, FrameType: Data (len %d) \n",
                MacAddress(src_addr).toString().c_str(),
                MacAddress(dst_addr).toString().c_str(),
                MacAddress(bssid).toString().c_str(),
                rssi,
                channel, payload_len);


  // Actualizamos la lista de redes si existe el BSSID.
  if (memcmp(bssid, broadcast_addr, 6) != 0)
  {
    ssidList.updateOrAddNetwork("", MacAddress(bssid), rssi, channel, "other");
  }

  stationsList.updateOrAddDevice(MacAddress(src_addr), MacAddress(bssid), rssi, channel);

  // Si el destino no es broadcast, actualizamos la lista de estaciones con el destino
  if (memcmp(dst_addr, broadcast_addr, 6) != 0)
  {
    stationsList.updateOrAddDevice(MacAddress(dst_addr), MacAddress(bssid), rssi, channel);
  }

  // actualizamos con el BSSID para que sume el tráfico de toda la red al AP 
  if (memcmp(bssid, broadcast_addr, 6) != 0 && memcmp(bssid, null_addr, 6) != 0)
  {
    stationsList.updateOrAddDevice(MacAddress(bssid), MacAddress(bssid), rssi, channel);
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
void WifiScanClass::parse_ssid(const uint8_t *payload, int payload_len, uint8_t subtype, char ssid[SSID_MAX_LEN])
{
  int pos = 24;   // Start after the management frame header
  ssid[0] = '\0'; // Default empty SSID

  // Ensure there are enough bytes for the header and at least one IE
  if (payload_len < pos + 2)
  {
    Serial.println("Payload too short for header");
    return;
  }

  // Determine starting position based on frame subtype
  switch (subtype) {
    case 0:  // Association Request
    case 2:  // Reassociation Request
      pos = 28; // Skip capability info + listen interval
      if (subtype == 2) pos += 6; // Reassoc has current AP field
      break;
    case 4:  // Probe Request
      pos = 24; // Start right after the header
      break;
    case 5:  // Probe Response
    case 8:  // Beacon
      pos = 36; // Skip timestamp + beacon interval + capability info
      break;
    default:
      Serial.println("Unknown subtype");
      return;
  }

  // Ensure we have enough bytes to read IEs
  if (pos + 2 > payload_len)
  {
    Serial.println("Payload too short for IEs");
    return;
  }

  // Parse Information Elements
  while (pos + 2 <= payload_len)
  {
    uint8_t id = payload[pos];
    uint8_t len = payload[pos + 1];

    // Check if we can safely read the IE
    if (pos + 2 + len > payload_len)
    {
      Serial.println("IE length exceeds payload");
      break;
    }

    if (id == 0)
    { // SSID element
      if (len == 0)
      {
        Serial.println("Empty SSID");
        return;
      }
      else if (len >= SSID_MAX_LEN)
      {
        Serial.println("SSID too long, truncating");
        len = SSID_MAX_LEN - 1;
      }

      memcpy(ssid, &payload[pos + 2], len);
      ssid[len] = '\0';

      // Validate SSID characters
      for (int i = 0; i < len; i++)
      {
        if (!isprint((unsigned char)ssid[i]))
        {
          ssid[i] = '.';
        }
      }
      return;
    }

    pos += 2 + len; // Move to next IE
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

