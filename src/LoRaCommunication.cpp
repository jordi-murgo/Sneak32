#include "LoRaCommunication.h"

LoRaCommunication LoRaComm;

#ifdef ENABLE_LORA

#include <SPI.h>
#include <LoRa.h>

#include "AppPreferences.h"
#include "BLEDataTransfer.h"
#include "BLEDetect.h"
#include "BLEDeviceList.h"
#include "BLEScan.h"
#include "WifiDetect.h"
#include "WifiDeviceList.h"
#include "WifiNetworkList.h"
#include "WifiScan.h"

extern WifiNetworkList ssidList;
extern WifiDeviceList stationsList;
extern BLEDeviceList bleDeviceList;
extern AppPreferencesData appPrefs;

#ifndef LORA_SCK
#define LORA_SCK 5
#endif

#ifndef LORA_MISO
#define LORA_MISO 19
#endif

#ifndef LORA_MOSI
#define LORA_MOSI 27
#endif

#ifndef LORA_SS
#define LORA_SS 18
#endif

#ifndef LORA_RST
#define LORA_RST 14
#endif

#ifndef LORA_DIO0
#define LORA_DIO0 26
#endif

#ifndef LORA_FREQUENCY
#define LORA_FREQUENCY 868E6
#endif

#ifndef LORA_SPREADING_FACTOR
#define LORA_SPREADING_FACTOR 7
#endif

#ifndef LORA_BANDWIDTH
#define LORA_BANDWIDTH 125E3
#endif

#ifndef LORA_CODING_RATE
#define LORA_CODING_RATE 5
#endif

#ifndef LORA_SYNC_WORD
#define LORA_SYNC_WORD 0x12
#endif

#ifndef LORA_PACKET_INTERVAL
#define LORA_PACKET_INTERVAL 250
#endif

#ifndef LORA_TRANSMISSION_TIMEOUT
#define LORA_TRANSMISSION_TIMEOUT 60000
#endif

void LoRaCommunication::setup()
{
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

    if (!LoRa.begin(LORA_FREQUENCY))
    {
        Serial.println("[LoRa] Failed to start radio");
        loraReady = false;
        return;
    }

    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    LoRa.setSignalBandwidth(LORA_BANDWIDTH);
    LoRa.setCodingRate4(LORA_CODING_RATE);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.enableCrc();

    loraReady = true;
    Serial.println("[LoRa] Radio initialized");
}

void LoRaCommunication::loop()
{
    if (!loraReady)
    {
        return;
    }

    if (radioPaused)
    {
        // Ensure capture tasks remain stopped if the operation mode changes mid-transfer
        pauseCaptures();
    }

    if (transmissionActive)
    {
        unsigned long now = millis();
        if (now - lastActivityTime > LORA_TRANSMISSION_TIMEOUT)
        {
            Serial.println("[LoRa] Transmission timeout");
            finishTransmission(false, "TIMEOUT");
        }
        else if (now - lastPacketSentTime >= LORA_PACKET_INTERVAL)
        {
            sendNextPacket();
        }
    }

    int packetSize = LoRa.parsePacket();
    if (packetSize <= 0)
    {
        return;
    }

    String incoming;
    while (LoRa.available())
    {
        incoming += static_cast<char>(LoRa.read());
    }
    incoming.trim();

    if (!incoming.length())
    {
        return;
    }

    if (transmissionActive)
    {
        Serial.println("[LoRa] Busy, ignoring new request");
        sendImmediate("STATUS|BUSY");
        return;
    }

    handleRequest(incoming);
}

bool LoRaCommunication::isTransmissionActive() const
{
    return transmissionActive;
}

bool LoRaCommunication::isRadioPaused() const
{
    return radioPaused;
}

void LoRaCommunication::handleRequest(const String &request)
{
    String normalized = request;
    normalized.trim();
    normalized.toLowerCase();

    Serial.printf("[LoRa] Received request: %s\n", request.c_str());

    if (normalized == "ping")
    {
        sendImmediate("PONG");
        return;
    }

    String requestType;
    if (normalized == "fetch" || normalized == "fetch_all")
    {
        requestType = "all";
    }
    else if (normalized == REQUEST_SSID_LIST)
    {
        requestType = REQUEST_SSID_LIST;
    }
    else if (normalized == REQUEST_CLIENT_LIST)
    {
        requestType = REQUEST_CLIENT_LIST;
    }
    else if (normalized == REQUEST_BLE_LIST)
    {
        requestType = REQUEST_BLE_LIST;
    }
    else
    {
        sendImmediate("ERROR|UNKNOWN_REQUEST");
        return;
    }

    preparePacketsForRequest(requestType);

    if (packetsToSend.empty())
    {
        sendImmediate("ERROR|NO_DATA");
        return;
    }

    activeRequest = requestType;
    startTransmission();
}

void LoRaCommunication::preparePacketsForRequest(const String &requestType)
{
    packetsToSend.clear();

    if (requestType == "all")
    {
        packetsToSend.push_back("BEGIN|all");
        appendListData(REQUEST_SSID_LIST);
        appendListData(REQUEST_CLIENT_LIST);
        appendListData(REQUEST_BLE_LIST);
        packetsToSend.push_back("END|all");
    }
    else
    {
        packetsToSend.push_back("BEGIN|" + requestType);
        appendListData(requestType);
        packetsToSend.push_back("END|" + requestType);
    }
}

void LoRaCommunication::appendListData(const String &requestType)
{
    if (requestType == REQUEST_SSID_LIST)
    {
        auto networks = ssidList.getClonedList();
        packetsToSend.push_back("LIST|ssid_list|" + String(networks.size()));
        for (const auto &network : networks)
        {
            String entry = "SSID|" + String(network.address.toString().c_str()) + "|" + sanitize(network.ssid) + "|" +
                           String(network.rssi) + "|" + String(network.channel) + "|" + sanitize(network.type) + "|" +
                           String(network.times_seen) + "|" + String(network.last_seen);
            packetsToSend.push_back(entry);
        }
    }
    else if (requestType == REQUEST_CLIENT_LIST)
    {
        auto clients = stationsList.getClonedList();
        packetsToSend.push_back("LIST|client_list|" + String(clients.size()));
        for (const auto &client : clients)
        {
            String entry = "CLIENT|" + String(client.address.toString().c_str()) + "|" +
                           String(client.bssid.toString().c_str()) + "|" + String(client.rssi) + "|" +
                           String(client.channel) + "|" + String(client.times_seen) + "|" + String(client.last_seen);
            packetsToSend.push_back(entry);
        }
    }
    else if (requestType == REQUEST_BLE_LIST)
    {
        auto devices = bleDeviceList.getClonedList();
        packetsToSend.push_back("LIST|ble_list|" + String(devices.size()));
        for (const auto &device : devices)
        {
            String entry = "BLE|" + String(device.address.toString().c_str()) + "|" + sanitize(device.name) + "|" +
                           String(device.rssi) + "|" + String(device.isPublic ? 1 : 0) + "|" +
                           String(device.times_seen) + "|" + String(device.last_seen);
            packetsToSend.push_back(entry);
        }
    }
    else
    {
        packetsToSend.push_back("ERROR|UNSUPPORTED_LIST");
    }
}

void LoRaCommunication::sendImmediate(const String &message)
{
    if (!loraReady)
    {
        return;
    }

    LoRa.beginPacket();
    LoRa.print(message);
    LoRa.endPacket();
}

void LoRaCommunication::startTransmission()
{
    pauseCaptures();

    transmissionActive = true;
    currentPacketIndex = 0;
    lastPacketSentTime = 0;
    lastActivityTime = millis();

    sendNextPacket();
}

void LoRaCommunication::sendNextPacket()
{
    if (!transmissionActive)
    {
        return;
    }

    if (currentPacketIndex >= packetsToSend.size())
    {
        finishTransmission(true, "DONE");
        return;
    }

    String packet = packetsToSend[currentPacketIndex];

    LoRa.beginPacket();
    LoRa.print(packet);
    LoRa.endPacket();

    currentPacketIndex++;
    lastPacketSentTime = millis();
    lastActivityTime = lastPacketSentTime;
}

void LoRaCommunication::finishTransmission(bool success, const String &reason)
{
    if (success)
    {
        const char *label = activeRequest.length() ? activeRequest.c_str() : "unknown";
        Serial.printf("[LoRa] Transmission completed (%s)\n", label);
    }
    else
    {
        const char *label = activeRequest.length() ? activeRequest.c_str() : "unknown";
        Serial.printf("[LoRa] Transmission aborted (%s): %s\n", label, reason.c_str());
        sendImmediate("STATUS|" + reason);
    }

    transmissionActive = false;
    packetsToSend.clear();
    activeRequest = "";
    resumeCaptures();
}

void LoRaCommunication::pauseCaptures()
{
    uint8_t currentMode = appPrefs.operation_mode;

    if (radioPaused && pausedMode == currentMode)
    {
        return;
    }

    Serial.println("[LoRa] Pausing capture tasks");

    switch (currentMode)
    {
    case OPERATION_MODE_SCAN:
        WifiScanner.stop();
        BLEScanner.stop();
        break;
    case OPERATION_MODE_DETECTION:
        WifiDetector.stop();
        BLEDetector.stop();
        break;
    default:
        WifiScanner.stop();
        BLEScanner.stop();
        WifiDetector.stop();
        BLEDetector.stop();
        break;
    }

    radioPaused = true;
    pausedMode = currentMode;
}

void LoRaCommunication::resumeCaptures()
{
    if (!radioPaused)
    {
        return;
    }

    uint8_t currentMode = appPrefs.operation_mode;

    Serial.println("[LoRa] Resuming capture tasks");

    switch (currentMode)
    {
    case OPERATION_MODE_SCAN:
        WifiScanner.start();
        BLEScanner.start();
        break;
    case OPERATION_MODE_DETECTION:
        WifiDetector.start();
        BLEDetector.start();
        break;
    default:
        break;
    }

    radioPaused = false;
    pausedMode = currentMode;
}

String LoRaCommunication::sanitize(const String &value) const
{
    String result = value;
    result.replace("|", "/");
    result.replace("\n", " ");
    result.replace("\r", " ");
    return result;
}

#else

void LoRaCommunication::setup() {}

void LoRaCommunication::loop() {}

bool LoRaCommunication::isTransmissionActive() const
{
    return false;
}

bool LoRaCommunication::isRadioPaused() const
{
    return false;
}

#endif
